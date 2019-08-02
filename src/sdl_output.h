/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2019 Stefan Pöschel

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
#include <stdint.h>
#include <string>
#include <mutex>
#include <vector>
#include <atomic>

#include "SDL.h"

#include "audio_output.h"
#include "tools.h"


// --- AudioSource -----------------------------------------------------------------
class AudioSource {
public:
	virtual ~AudioSource() {}

	virtual void AudioCallback(Uint8* /*stream*/, int /*len*/) = 0;
};


// --- SDLOutput -----------------------------------------------------------------
class SDLOutput : public AudioOutput, AudioSource {
private:
	SDL_AudioDeviceID audio_device;
	SDL_AudioSpec audio_spec;
	int silence_len;

	int samplerate;
	int channels;
	bool float32;

	std::mutex audio_buffer_mutex;
	CircularBuffer *audio_buffer;
	size_t audio_start_buffer_size;
	std::vector<uint8_t> audio_mix_buffer;

	std::atomic<bool> audio_mute;
	std::atomic<double> audio_volume;

	void AudioCallback(Uint8* stream, int len);
	size_t GetAudio(uint8_t *data, size_t len);
	void SetAudioStartBufferSize();
public:
	SDLOutput();
	~SDLOutput();

	void StartAudio(int samplerate, int channels, bool float32);
	void StopAudio();
	void PutAudio(const uint8_t *data, size_t len);
	void SetAudioMute(bool audio_mute) {this->audio_mute = audio_mute;}
	void SetAudioVolume(double audio_volume) {this->audio_volume = audio_volume;}
	bool HasAudioVolumeControl() {return true;}
};

#endif /* SDL_OUTPUT_H_ */
