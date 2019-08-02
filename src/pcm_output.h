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

#ifndef PCM_OUTPUT_H_
#define PCM_OUTPUT_H_

#include <stdint.h>
#include <stdio.h>
#include <atomic>

#include "audio_output.h"


// --- PCMOutput -----------------------------------------------------------------
class PCMOutput : public AudioOutput {
private:
	int samplerate;
	int channels;
	bool float32;

	std::atomic<bool> audio_mute;
public:
	PCMOutput();
	~PCMOutput() {}

	void StartAudio(int samplerate, int channels, bool float32);
	void StopAudio() {}
	void PutAudio(const uint8_t *data, size_t len);
	void SetAudioMute(bool audio_mute) {this->audio_mute = audio_mute;}
	void SetAudioVolume(double) {}
	bool HasAudioVolumeControl() {return false;}
};

#endif /* PCM_OUTPUT_H_ */
