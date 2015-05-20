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

#ifndef SDL_OUTPUT_H_
#define SDL_OUTPUT_H_

#include <stdexcept>
#include <sstream>
#include <cstdint>

#include "SDL.h"

#include "audio_output.h"


struct SDLOutputCallbackData {
	AudioSource *audio_source;

	uint8_t silence;
	int silence_len;
};

// --- SDLOutput -----------------------------------------------------------------
class SDLOutput : public AudioOutput {
private:
	SDLOutputCallbackData cb_data;
	SDL_AudioDeviceID audio_device;

	int samplerate;
	int channels;
	bool float32;
public:
	SDLOutput(AudioSource *audio_source);
	~SDLOutput();

	void StartAudio(int samplerate, int channels, bool float32);
	void StopAudio();
};

#endif /* SDL_OUTPUT_H_ */
