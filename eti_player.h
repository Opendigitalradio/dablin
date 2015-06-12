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

#ifndef ETI_PLAYER_H_
#define ETI_PLAYER_H_

#include <cstdio>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "subchannel_sink.h"
#include "dab_decoder.h"
#include "dabplus_decoder.h"
#include "sdl_output.h"
#include "tools.h"


// --- ETIPlayerObserver -----------------------------------------------------------------
class ETIPlayerObserver {
public:
	virtual ~ETIPlayerObserver() {};

	virtual void ETIChangeFormat() {};
	virtual void ETIProcessFIC(const uint8_t *data, size_t len) {};
	virtual void ETIProcessPAD(const uint8_t *xpad_data, size_t xpad_len, uint16_t fpad) {};
	virtual void ETIResetPAD() {};
};


// --- ETIPlayer -----------------------------------------------------------------
class ETIPlayer : SubchannelSinkObserver, AudioSource {
private:
	ETIPlayerObserver *observer;

	std::chrono::steady_clock::time_point next_frame_time;

	std::mutex status_mutex;
	int subchannel_now;
	bool dab_plus_now;
	int subchannel_next;
	bool dab_plus_next;
	std::string format;

	uint8_t xpad[256];	// limit never reached by longest possible X-PAD
	SubchannelSink *dec;
	AudioOutput *out;

	std::mutex audio_buffer_mutex;
	CircularBuffer *audio_buffer;
	size_t audio_start_buffer_size;
	bool audio_mute;

	void DecodeFrame(const uint8_t *eti_frame);

	void FormatChange(std::string format);
	void StartAudio(int samplerate, int channels, bool float32);
	void PutAudio(const uint8_t *data, size_t len);
	void ProcessFIC(const uint8_t *data, size_t len);
	void ProcessPAD(const uint8_t *xpad_data, size_t xpad_len, const uint8_t *fpad_data);

	size_t GetAudio(uint8_t *data, size_t len, uint8_t silence);
public:
	ETIPlayer(ETIPlayerObserver *observer);
	~ETIPlayer();

	void ProcessFrame(const uint8_t *data);

	void SetAudioSubchannel(int subchannel, bool dab_plus);
	void SetAudioMute(bool audio_mute);
	std::string GetFormat();
};



#endif /* ETI_PLAYER_H_ */
