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

#include "eti_player.h"


// --- ETIPlayer -----------------------------------------------------------------
ETIPlayer::ETIPlayer(std::string filename, ETIPlayerObserver *observer) {
	this->filename = filename;
	this->observer = observer;

	input_file = NULL;
	next_frame_time = std::chrono::steady_clock::now();

	do_exit = false;

	subchannel_now = subchannel_next = -1;
	dab_plus_now = dab_plus_next = false;

	dec = NULL;
	out = NULL;

	audio_buffer = NULL;
	audio_start_buffer_size = 0;
	audio_mute = false;
}

ETIPlayer::~ETIPlayer() {
	// cleanup
	out->StopAudio();

	delete dec;
	delete out;
	delete audio_buffer;

	if(input_file)
		fclose(input_file);
}

void ETIPlayer::DoExit() {
	std::lock_guard<std::mutex> lock(status_mutex);

	do_exit = true;
}

void ETIPlayer::SetAudioSubchannel(int subchannel, bool dab_plus) {
	std::lock_guard<std::mutex> lock(status_mutex);

	fprintf(stderr, "ETIPlayer: playing subchannel %d (%s)\n", subchannel, dab_plus ? "DAB+" : "DAB");

	subchannel_next = subchannel;
	dab_plus_next = dab_plus;
}


int ETIPlayer::Main() {
	int file_no;

	if(filename.empty()) {
		file_no = STDIN_FILENO;
		filename = "stdin";
	} else {
		input_file = fopen(filename.c_str(), "rb");
		if(!input_file) {
			perror("ETIPlayer: error opening input file");
			return 1;
		}
		file_no = fileno(input_file);
	}

	fprintf(stderr, "ETIPlayer: reading from '%s'\n", filename.c_str());


	out = new SDLOutput(this);

	// set non-blocking mode
	int old_flags = fcntl(file_no, F_GETFL);
	if(old_flags == -1) {
		perror("ETIPlayer: error getting socket flags");
		return 1;
	}
	if(fcntl(file_no, F_SETFL, (old_flags == -1 ? 0 : old_flags) | O_NONBLOCK)) {
		perror("ETIPlayer: error setting socket flags");
		return 1;
	}

	fd_set fds;
	timeval select_timeval;
	size_t filled = 0;

	for(;;) {
		{
			std::lock_guard<std::mutex> lock(status_mutex);

			if(do_exit)
				break;

			if(subchannel_now != subchannel_next || dab_plus_now != dab_plus_next) {
				// cleanup
				if(dec) {
//					out->StopAudio();
					delete dec;
					dec = NULL;
				}

				subchannel_now = subchannel_next;
				dab_plus_now = dab_plus_next;

				observer->ETIResetPAD();

				// append
				if(subchannel_now != -1) {
					if(dab_plus_now)
						dec = new SuperframeFilter(this);
					else
						dec = new MP2Decoder(this);
				}
			}
		}

		FD_ZERO(&fds);
		FD_SET(file_no, &fds);

		select_timeval.tv_sec = 0;
		select_timeval.tv_usec = 100 * 1000;

		int ready_fds = select(file_no + 1, &fds, NULL, NULL, &select_timeval);
		if(!(ready_fds && FD_ISSET(file_no, &fds)))
			continue;

		ssize_t bytes = read(file_no, eti_frame + filled, sizeof(eti_frame) - filled);

		if(bytes > 0)
			filled += bytes;
		if(bytes == -1) {
			perror("ETIPlayer: error while read");
			return 1;
		}

		if(filled < sizeof(eti_frame))
			continue;

		// flow control
		std::this_thread::sleep_until(next_frame_time);
		next_frame_time += std::chrono::milliseconds(24);

		DecodeFrame();
		filled = 0;
	}

	return 0;
}


void ETIPlayer::DecodeFrame() {
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

	// abort here, if ATM no subchannel selected
	if(subchannel_now == -1)
		return;

	for(int i = 0; i < nst; i++) {
		int scid = (eti_frame[8 + i*4] & 0xFC) >> 2;
		int stl = (eti_frame[8 + i*4 + 2] & 0x03) << 8 | eti_frame[8 + i*4 + 3];

		if(scid == subchannel_now) {
			subch_bytes = stl * 8;
			break;
		} else {
			subch_offset += stl * 8;
		}
	}
	if(subch_bytes == 0) {
		fprintf(stderr, "ETIPlayer: ignored ETI frame without subch %d\n", subchannel_now);
		return;
	}

	// TODO: check body CRC?


	dec->Feed(eti_frame + subch_offset, subch_bytes);
}

void ETIPlayer::StartAudio(int samplerate, int channels, bool float32) {
//	out->StopAudio();

	{
		std::lock_guard<std::mutex> lock(audio_buffer_mutex);

		if(audio_buffer)
			delete audio_buffer;

		// use 500ms buffer (start audio when 1/2 filled)
		size_t buffersize = samplerate / 2 * channels * (float32 ? 4 : 2);
		fprintf(stderr, "ETIPlayer: using audio buffer of %zu bytes\n", buffersize);
		audio_buffer = new CircularBuffer(buffersize);
		audio_start_buffer_size = buffersize / 2;
	}

	out->StartAudio(samplerate, channels, float32);
}

void ETIPlayer::PutAudio(const uint8_t *data, size_t len) {
	std::lock_guard<std::mutex> lock(audio_buffer_mutex);

	size_t capa = audio_buffer->Capacity() - audio_buffer->Size();
//	if(capa < len) {
//		fprintf(stderr, "DABlin: audio buffer overflow, therefore cleaning buffer!\n");
//		audio_buffer->Clear();
//		capa = audio_buffer->capacity();
//	}

	if(len > capa)
		fprintf(stderr, "ETIPlayer: audio buffer overflow: %zu > %zu\n", len, capa);

	audio_buffer->Write(data, len);

//	fprintf(stderr, "Buffer: %zu / %zu\n", audio_buffer->Size(), audio_buffer->Capacity());
}

size_t ETIPlayer::GetAudio(uint8_t *data, size_t len, uint8_t silence) {
	std::lock_guard<std::mutex> lock(audio_buffer_mutex);

	if(audio_start_buffer_size && audio_buffer->Size() >= audio_start_buffer_size)
		audio_start_buffer_size = 0;

	if(audio_mute || audio_start_buffer_size) {
		if(audio_start_buffer_size == 0)
			audio_buffer->Read(NULL, len);
		memset(data, silence, len);
		return len;
	}

	return audio_buffer->Read(data, len);
}

void ETIPlayer::SetAudioMute(bool audio_mute) {
	std::lock_guard<std::mutex> lock(audio_buffer_mutex);
	this->audio_mute = audio_mute;
}

void ETIPlayer::FormatChange(std::string format) {
	std::lock_guard<std::mutex> lock(status_mutex);
	this->format = format;

	fprintf(stderr, "ETIPlayer: format: %s\n", format.c_str());

	if(observer)
		observer->ETIChangeFormat();
}

std::string ETIPlayer::GetFormat() {
	std::lock_guard<std::mutex> lock(status_mutex);
	return format;
}

void ETIPlayer::ProcessFIC(const uint8_t *data, size_t len) {
//	fprintf(stderr, "Received %zu bytes FIC\n", len);
	if(observer)
		observer->ETIProcessFIC(data, len);
}

void ETIPlayer::ProcessPAD(const uint8_t *data, size_t len) {
//	fprintf(stderr, "Received %zu bytes PAD\n", len);
	if(observer)
		observer->ETIProcessPAD(data, len);
}
