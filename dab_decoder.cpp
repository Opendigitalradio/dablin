/*
    DABlin - capital DAB experience
    Copyright (C) 2015 Stefan Pöschel

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dab_decoder.h"

// --- MP2Decoder -----------------------------------------------------------------
MP2Decoder::MP2Decoder(SubchannelSinkObserver* observer) : SubchannelSink(observer) {
	crc_len = -1;

	int mpg_result;

	// init
	mpg_result = mpg123_init();
	if(mpg_result != MPG123_OK) {
		std::stringstream ss;
		ss << "MP2Decoder: error while mpg123_init: " << mpg123_plain_strerror(mpg_result);
		throw std::runtime_error(ss.str());
	}

	// ensure features
	if(!mpg123_feature(MPG123_FEATURE_OUTPUT_32BIT))
		throw std::runtime_error("MP2Decoder: no 32bit output support!");
	if(!mpg123_feature(MPG123_FEATURE_DECODE_LAYER2))
		throw std::runtime_error("MP2Decoder: no Layer II decode support!");

	handle = mpg123_new(NULL, &mpg_result);
	if(!handle) {
		std::stringstream ss;
		ss << "MP2Decoder: error while mpg123_new: " << mpg123_plain_strerror(mpg_result);
		throw std::runtime_error(ss.str());
	}

	fprintf(stderr, "MP2Decoder: using decoder '%s'.\n", mpg123_current_decoder(handle));


	// set allowed formats
	mpg_result = mpg123_format_none(handle);
	if(mpg_result != MPG123_OK) {
		std::stringstream ss;
		ss << "MP2Decoder: error while mpg123_format_none: " << mpg123_plain_strerror(mpg_result);
		throw std::runtime_error(ss.str());
	}

	mpg_result = mpg123_format(handle, 48000, MPG123_MONO | MPG123_STEREO, MPG123_ENC_FLOAT_32);
	if(mpg_result != MPG123_OK) {
		std::stringstream ss;
		ss << "MP2Decoder: error while mpg123_format #1: " << mpg123_plain_strerror(mpg_result);
		throw std::runtime_error(ss.str());
	}

	mpg_result = mpg123_format(handle, 24000, MPG123_MONO | MPG123_STEREO, MPG123_ENC_FLOAT_32);
	if(mpg_result != MPG123_OK) {
		std::stringstream ss;
		ss << "MP2Decoder: error while mpg123_format #2: " << mpg123_plain_strerror(mpg_result);
		throw std::runtime_error(ss.str());
	}

	mpg_result = mpg123_open_feed(handle);
	if(mpg_result != MPG123_OK) {
		std::stringstream ss;
		ss << "MP2Decoder: error while mpg123_open_feed: " << mpg123_plain_strerror(mpg_result);
		throw std::runtime_error(ss.str());
	}
}

MP2Decoder::~MP2Decoder() {
	if(handle) {
		int mpg_result = mpg123_close(handle);
		if(mpg_result != MPG123_OK)
			fprintf(stderr, "MP2Decoder: error while mpg123_close: %s\n", mpg123_plain_strerror(mpg_result));
	}

	mpg123_delete(handle);
	mpg123_exit();
}

void MP2Decoder::Feed(const uint8_t *data, size_t len) {
	int mpg_result = mpg123_feed(handle, data, len);
	if(mpg_result != MPG123_OK) {
		std::stringstream ss;
		ss << "MP2Decoder: error while mpg123_feed: " << mpg123_plain_strerror(mpg_result);
		throw std::runtime_error(ss.str());
	}

	// forward decoded frames
	uint8_t *frame_data;
	size_t frame_len;
	while((frame_len = GetFrame(&frame_data)) > 0)
		observer->PutAudio(frame_data, frame_len);
}

size_t MP2Decoder::GetFrame(uint8_t **data) {
	int mpg_result;

	// go to next frame
	mpg_result = mpg123_framebyframe_next(handle);
	switch(mpg_result) {
	case MPG123_NEED_MORE:
		return 0;
	case MPG123_NEW_FORMAT:
		ProcessFormat();
		break;
	case MPG123_OK:
		break;
	default:
		std::stringstream ss;
		ss << "MP2Decoder: error while mpg123_framebyframe_next: " << mpg123_plain_strerror(mpg_result);
		throw std::runtime_error(ss.str());
	}

	if(crc_len == -1)
		throw std::runtime_error("MP2Decoder: CRC len not yet set at PAD extraction!");

	// derive PAD data from frame
	uint8_t *body_data;
	size_t body_bytes;
	mpg_result = mpg123_framedata(handle, NULL, &body_data, &body_bytes);
	if(mpg_result != MPG123_OK) {
		std::stringstream ss;
		ss << "MP2Decoder: error while mpg123_framedata: " << mpg123_plain_strerror(mpg_result);
		throw std::runtime_error(ss.str());
	}

	// TODO: check CRC!?

	// remove CRC gap between X-PAD and F-PAD
	int used_body_bytes = std::min(body_bytes, sizeof(pad) - FPAD_LEN);
	memcpy(pad, body_data + body_bytes - FPAD_LEN - crc_len - used_body_bytes, used_body_bytes);
	memcpy(pad + used_body_bytes, body_data + body_bytes - FPAD_LEN, FPAD_LEN);

	observer->ProcessPAD(pad, used_body_bytes + FPAD_LEN);

	size_t frame_len;
	mpg_result = mpg123_framebyframe_decode(handle, NULL, data, &frame_len);
	if(mpg_result != MPG123_OK) {
		std::stringstream ss;
		ss << "MP2Decoder: error while mpg123_framebyframe_decode: " << mpg123_plain_strerror(mpg_result);
		throw std::runtime_error(ss.str());
	}

	return frame_len;
}


void MP2Decoder::ProcessFormat() {
	mpg123_frameinfo info;
	int mpg_result = mpg123_info(handle, &info);
	if(mpg_result != MPG123_OK) {
		std::stringstream ss;
		ss << "MP2Decoder: error while mpg123_info: " << mpg123_plain_strerror(mpg_result);
		throw std::runtime_error(ss.str());
	}

	crc_len = (info.version == MPG123_1_0 && info.bitrate < (info.mode == MPG123_M_MONO ? 56 : 112)) ? 2 : 4;

	// output format
	const char* version = "unknown";
	switch(info.version) {
	case MPG123_1_0:
		version = "1.0";
		break;
	case MPG123_2_0:
		version = "2.0";
		break;
	case MPG123_2_5:
		version = "2.5";
		break;
	}

	const char* layer = "unknown";
	switch(info.layer) {
	case 1:
		layer = "I";
		break;
	case 2:
		layer = "II";
		break;
	case 3:
		layer = "III";
		break;
	}

	const char* mode = "unknown";
	switch(info.mode) {
	case MPG123_M_STEREO:
		mode = "Stereo";
		break;
	case MPG123_M_JOINT:
		mode = "Joint Stereo";
		break;
	case MPG123_M_DUAL:
		mode = "Dual Channel";
		break;
	case MPG123_M_MONO:
		mode = "Mono";
		break;
	}

	fprintf(stderr, "MP2Decoder: format: MPEG %s Layer %s, %ld kHz %s @ %d kbit/s\n", version, layer, info.rate / 1000, mode, info.bitrate);

	std::stringstream ss;
	ss << "MPEG " << version << " Layer " << layer << ", ";
	ss << (info.rate / 1000) << " kHz ";
	ss << mode << " ";
	ss << "@ " << info.bitrate << " kbit/s";
	observer->FormatChange(ss.str());

	observer->StartAudio(info.rate, info.mode != MPG123_M_MONO ? 2 : 1, true);
}
