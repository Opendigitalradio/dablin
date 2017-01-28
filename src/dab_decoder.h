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

#ifndef DAB_DECODER_H_
#define DAB_DECODER_H_

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <sstream>

#include "mpg123.h"

#include "subchannel_sink.h"


// --- MP2Decoder -----------------------------------------------------------------
class MP2Decoder : public SubchannelSink {
private:
	mpg123_handle *handle;

	int crc_len;

	void ProcessFormat();
	size_t GetFrame(uint8_t **data);
public:
	MP2Decoder(SubchannelSinkObserver* observer);
	~MP2Decoder();

	void Feed(const uint8_t *data, size_t len);
};

#endif /* DAB_DECODER_H_ */
