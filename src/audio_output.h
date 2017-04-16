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

#ifndef AUDIO_OUTPUT_H_
#define AUDIO_OUTPUT_H_

#include <string.h>


// --- AudioOutput -----------------------------------------------------------------
class AudioOutput {
public:
	virtual ~AudioOutput() {}

	virtual void StartAudio(int /*samplerate*/, int /*channels*/, bool /*float32*/) = 0;
	virtual void PutAudio(const uint8_t* /*data*/, size_t /*len*/) = 0;

	virtual void SetAudioMute(bool /*audio_mute*/) = 0;
	virtual void SetAudioVolume(double /*audio_volume*/) = 0;
	virtual bool HasAudioVolumeControl() = 0;
};


#endif /* AUDIO_OUTPUT_H_ */
