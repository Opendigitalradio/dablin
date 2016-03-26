/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2016 Stefan Pöschel

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

#ifndef SDL_OUTPUT_H_
#define SDL_OUTPUT_H_

#include <stdexcept>
#include <sstream>
#include <cstdint>
#include <mutex>

#include "SDL.h"

#include "audio_output.h"
#include "tools.h"


// --- AudioSource -----------------------------------------------------------------
class AudioSource {
public:
	virtual ~AudioSource() {};

	virtual size_t GetAudio(uint8_t *data, size_t len, uint8_t silence) = 0;
};


struct SDLOutputCallbackData {
	AudioSource *audio_source;

	uint8_t silence;
	int silence_len;
};

// --- SDLOutput -----------------------------------------------------------------
class SDLOutput : public AudioOutput, AudioSource {
private:
	SDLOutputCallbackData cb_data;
	SDL_AudioDeviceID audio_device;

	int samplerate;
	int channels;
	bool float32;

	std::mutex audio_buffer_mutex;
	CircularBuffer *audio_buffer;
	size_t audio_start_buffer_size;
	bool audio_mute;

	size_t GetAudio(uint8_t *data, size_t len, uint8_t silence);
	void StopAudio();
	void SetAudioStartBufferSize();
public:
	SDLOutput();
	~SDLOutput();

	void StartAudio(int samplerate, int channels, bool float32);
	void PutAudio(const uint8_t *data, size_t len);
	void SetAudioMute(bool audio_mute);
};

#endif /* SDL_OUTPUT_H_ */
