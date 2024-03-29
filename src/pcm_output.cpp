/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2024 Stefan Pöschel

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

	audio_mute = false;
}

void PCMOutput::StartAudio(int samplerate, int channels) {
	// if no change, return
	if(this->samplerate == samplerate && this->channels == channels)
		return;
	this->samplerate = samplerate;
	this->channels = channels;

	ChangeFormat(samplerate, channels);
}

void PCMOutput::ChangeFormat(int samplerate, int channels) {
	fprintf(stderr, "PCMOutput: format set; samplerate: %d, channels: %d\n", samplerate, channels);
}

void PCMOutput::PutAudio(const uint8_t *data, size_t len) {
	if(!audio_mute)
		fwrite(data, len, 1, stdout);
}
