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

#ifndef SUBCHANNEL_SINK_H_
#define SUBCHANNEL_SINK_H_

#include <stdint.h>
#include <string>

#define FPAD_LEN 2

// --- SubchannelSinkObserver -----------------------------------------------------------------
class SubchannelSinkObserver {
public:
	virtual ~SubchannelSinkObserver() {};

	virtual void FormatChange(const std::string& /*format*/) {}
	virtual void StartAudio(int /*samplerate*/, int /*channels*/, bool /*float32*/) {}
	virtual void PutAudio(const uint8_t* /*data*/, size_t /*len*/) {}
	virtual void ProcessPAD(const uint8_t* /*xpad_data*/, size_t /*xpad_len*/, bool /*exact_xpad_len*/, const uint8_t* /*fpad_data*/) {}
};


// --- SubchannelSink -----------------------------------------------------------------
class SubchannelSink {
protected:
	SubchannelSinkObserver* observer;
public:
	SubchannelSink(SubchannelSinkObserver* observer) : observer(observer) {}
	virtual ~SubchannelSink() {};

	virtual void Feed(const uint8_t *data, size_t len) = 0;
};

#endif /* SUBCHANNEL_SINK_H_ */
