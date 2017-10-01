/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2017 Stefan Pöschel

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

#include "eti_player.h"


// --- ETIPlayer -----------------------------------------------------------------
ETIPlayer::ETIPlayer(bool pcm_output, ETIPlayerObserver *observer) {
	this->observer = observer;

	next_frame_time = std::chrono::steady_clock::now();

	dec = NULL;

#ifndef DABLIN_DISABLE_SDL
	if(!pcm_output)
		out = new SDLOutput;
	else
#endif
		out = new PCMOutput;
}

ETIPlayer::~ETIPlayer() {
	delete dec;
	delete out;
}

bool ETIPlayer::IsSameAudioService(const AUDIO_SERVICE& audio_service) {
	std::lock_guard<std::mutex> lock(status_mutex);

	// check, if already the same present/next service (usually present == next)
	return audio_service_next == audio_service;
}

void ETIPlayer::SetAudioService(const AUDIO_SERVICE& audio_service) {
	std::lock_guard<std::mutex> lock(status_mutex);

	// abort, if already the same present/next service (usually present == next)
	if(audio_service_next == audio_service)
		return;

	if(audio_service.IsNone())
		fprintf(stderr, "ETIPlayer: playing no sub-channel\n");
	else
		fprintf(stderr, "ETIPlayer: playing sub-channel %d (%s)\n", audio_service.subchid, audio_service.dab_plus ? "DAB+" : "DAB");

	audio_service_next = audio_service;
}

void ETIPlayer::ProcessFrame(const uint8_t *data) {
	// handle sub-channel change
	{
		std::lock_guard<std::mutex> lock(status_mutex);

		if(audio_service_now != audio_service_next) {
			// cleanup
			if(dec) {
//					out->StopAudio();
				delete dec;
				dec = NULL;
			}

			audio_service_now = audio_service_next;

			observer->ETIResetPAD();

			// append
			if(!audio_service_now.IsNone()) {
				if(audio_service_now.dab_plus)
					dec = new SuperframeFilter(this);
				else
					dec = new MP2Decoder(this);
			}
		}
	}

	// flow control
	std::this_thread::sleep_until(next_frame_time);
	next_frame_time += std::chrono::milliseconds(24);

	DecodeFrame(data);
}

void ETIPlayer::DecodeFrame(const uint8_t *eti_frame) {
	// ERR
	if(eti_frame[0] != 0xFF) {
		fprintf(stderr, "ETIPlayer: ignored ETI frame with ERR = 0x%02X\n", eti_frame[0]);
		return;
	}

	uint32_t fsync = eti_frame[1] << 16 | eti_frame[2] << 8 | eti_frame[3];
	if(fsync != 0x073AB6 && fsync != 0xF8C549) {
		fprintf(stderr, "ETIPlayer: ignored ETI frame with FSYNC = 0x%06X\n", fsync);
		return;
	}

	bool ficf = eti_frame[5] & 0x80;
	int nst = eti_frame[5] & 0x7F;
	int mid = (eti_frame[6] & 0x18) >> 3;
//	int fl = (eti_frame[6] & 0x07) << 8 | eti_frame[7];

	// check header CRC
	size_t header_crc_data_len = 4 + nst * 4 + 2;
	uint16_t header_crc_stored = eti_frame[4 + header_crc_data_len] << 8 | eti_frame[4 + header_crc_data_len + 1];
	uint16_t header_crc_calced = CalcCRC::CalcCRC_CRC16_CCITT.Calc(eti_frame + 4, header_crc_data_len);
	if(header_crc_stored != header_crc_calced) {
		fprintf(stderr, "ETIPlayer: ignored ETI frame due to wrong header CRC\n");
		return;
	}

	int ficl = ficf ? (mid == 3 ? 32 : 24) : 0;

	int subch_bytes = 0;
	int subch_offset = 8 + nst * 4 + 4;

	if(ficl) {
		ProcessFIC(eti_frame + subch_offset, ficl * 4);
		subch_offset += ficl * 4;
	}

	// abort here, if ATM no sub-channel selected
	if(audio_service_now.IsNone())
		return;

	for(int i = 0; i < nst; i++) {
		int scid = (eti_frame[8 + i*4] & 0xFC) >> 2;
		int stl = (eti_frame[8 + i*4 + 2] & 0x03) << 8 | eti_frame[8 + i*4 + 3];

		if(scid == audio_service_now.subchid) {
			subch_bytes = stl * 8;
			break;
		} else {
			subch_offset += stl * 8;
		}
	}
	if(subch_bytes == 0) {
		fprintf(stderr, "ETIPlayer: ignored ETI frame without sub-channel %d\n", audio_service_now.subchid);
		return;
	}

	// TODO: check body CRC?


	dec->Feed(eti_frame + subch_offset, subch_bytes);
}

void ETIPlayer::FormatChange(const std::string& format) {
	fprintf(stderr, "ETIPlayer: format: %s\n", format.c_str());
	if(observer)
		observer->ETIChangeFormat(format);
}

void ETIPlayer::ProcessFIC(const uint8_t *data, size_t len) {
//	fprintf(stderr, "Received %zu bytes FIC\n", len);
	if(observer)
		observer->ETIProcessFIC(data, len);
}

void ETIPlayer::ProcessPAD(const uint8_t *xpad_data, size_t xpad_len, bool exact_xpad_len, const uint8_t *fpad_data) {
//	fprintf(stderr, "Received %zu bytes X-PAD\n", xpad_len);
	if(observer)
		observer->ETIProcessPAD(xpad_data, xpad_len, exact_xpad_len, fpad_data);
}
