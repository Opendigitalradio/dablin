/*
    DABlin - capital DAB experience
    Copyright (C) 2024 Stefan Pöschel

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

#ifndef WAV_OUTPUT_H_
#define WAV_OUTPUT_H_

#include <string>

#include "pcm_output.h"


// --- WAVOutput -----------------------------------------------------------------
class WAVOutput : public PCMOutput {
private:
	void WriteString(std::string value);
	void WriteUInt16(uint16_t value);
	void WriteUInt32(uint32_t value);
protected:
	virtual void ChangeFormat(int samplerate, int channels);
public:
	WAVOutput() : PCMOutput() {}
	~WAVOutput() {}
};

#endif /* WAV_OUTPUT_H_ */
