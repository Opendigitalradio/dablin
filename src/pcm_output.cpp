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

#include "pcm_output.h"


// --- PCMOutput -----------------------------------------------------------------
PCMOutput::PCMOutput() : AudioOutput() {
	samplerate = 0;
	channels = 0;
	float32 = false;

	audio_mute = false;
}

void PCMOutput::StartAudio(int samplerate, int channels, bool float32) {
	// if no change, return
	if(this->samplerate == samplerate && this->channels == channels && this->float32 == float32)
		return;
	this->samplerate = samplerate;
	this->channels = channels;
	this->float32 = float32;

	fprintf(stderr, "PCMOutput: format set; samplerate: %d, channels: %d, output: %s\n",
			samplerate,
			channels,
			float32 ? "32bit float" : "16bit integer");
}


void PCMOutput::PutAudio(const uint8_t *data, size_t len) {
	std::lock_guard<std::mutex> lock(audio_mute_mutex);

	if(!audio_mute)
		fwrite(data, len, 1, stdout);
}

void PCMOutput::SetAudioMute(bool audio_mute) {
	std::lock_guard<std::mutex> lock(audio_mute_mutex);
	this->audio_mute = audio_mute;
}
