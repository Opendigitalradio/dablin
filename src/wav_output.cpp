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

#include "wav_output.h"


// --- WAVOutput -----------------------------------------------------------------
void WAVOutput::WriteString(std::string value) {
	fwrite(value.c_str(), value.length(), 1, stdout);
}

void WAVOutput::WriteUInt16(uint16_t value) {
	fwrite(&value, 2, 1, stdout);
}

void WAVOutput::WriteUInt32(uint32_t value) {
	fwrite(&value, 4, 1, stdout);
}

void WAVOutput::ChangeFormat(int samplerate, int channels) {
	fprintf(stderr, "WAVOutput: format set; samplerate: %d, channels: %d\n", samplerate, channels);

	// write RIFF chunk
	WriteString("RIFF");		// ckID
	WriteUInt32(UINT32_MAX);	// ckSize - maximum value to allow streaming

	WriteString("WAVE");		// formType

	// write format chunk
	WriteString("fmt ");					// ckID
	WriteUInt32(16);						// ckSize

	WriteUInt16(0x0001);					// wFormatTag - WAVE_FORMAT_PCM
	WriteUInt16(channels);					// nChannels
	WriteUInt32(samplerate);				// nSamplesPerSec
	WriteUInt32(2 * channels * samplerate);	// nAvgBytesPerSec
	WriteUInt16(2 * channels);				// nBlockAlign
	WriteUInt16(16);						// wBitsPerSample

	// write data chunk
	WriteString("data");					// ckID
	WriteUInt32(UINT32_MAX);				// ckSize - maximum value to allow streaming
	// raw PCM samples follow separately...
}
