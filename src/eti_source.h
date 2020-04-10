/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2020 Stefan Pöschel

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

#ifndef ETI_SOURCE_H_
#define ETI_SOURCE_H_

#include "ensemble_source.h"


struct DAB_LIVE_SOURCE_CHANNEL {
	std::string block;
	uint32_t freq;
	int gain;

	static const int auto_gain = -10000;
	static const int default_gain = -10001;
	std::string GainToString() {
		switch(gain) {
		case auto_gain:
			return "auto";
		case default_gain:
			return "default";
		default:
			return std::to_string(gain);
		}
	}

	DAB_LIVE_SOURCE_CHANNEL() : freq(-1), gain(auto_gain) {}
	DAB_LIVE_SOURCE_CHANNEL(std::string block, uint32_t freq, int gain) : block(block), freq(freq), gain(gain) {}

	bool operator<(const DAB_LIVE_SOURCE_CHANNEL & ch) const {
		return freq < ch.freq;
	}
};


// --- ETISource -----------------------------------------------------------------
class ETISource : public EnsembleSource {
private:
	bool CheckFrameCompleted(const SYNC_MAGIC& /*matched_sync_magic*/) {return true;}
public:
	ETISource(std::string filename, EnsembleSourceObserver *observer) : EnsembleSource(filename, observer, "ETI", 6144) {
		AddSyncMagic(1, {0x07, 0x3A, 0xB6}, "FSYNC0");
		AddSyncMagic(1, {0xF8, 0xC5, 0x49}, "FSYNC1");
	}
	~ETISource() {}
};


// --- DABLiveETISource -----------------------------------------------------------------
class DABLiveETISource : public ETISource {
protected:
	DAB_LIVE_SOURCE_CHANNEL channel;
	std::string binary;
	std::string binary_name;
	std::string source_name;

	void Init();
	void PrintSource();
	virtual std::string GetParams() = 0;
public:
	DABLiveETISource(std::string binary, DAB_LIVE_SOURCE_CHANNEL channel, EnsembleSourceObserver *observer, std::string source_name);
	~DABLiveETISource();

	static const std::string TYPE_DAB2ETI;
	static const std::string TYPE_ETI_CMDLINE;
};


// --- DAB2ETIETISource -----------------------------------------------------------------
class DAB2ETIETISource : public DABLiveETISource {
protected:
	std::string GetParams();
public:
	DAB2ETIETISource(std::string binary, DAB_LIVE_SOURCE_CHANNEL channel, EnsembleSourceObserver *observer) : DABLiveETISource(binary, channel, observer, TYPE_DAB2ETI) {}
};


// --- EtiCmdlineETISource -----------------------------------------------------------------
class EtiCmdlineETISource : public DABLiveETISource {
protected:
	std::string GetParams();
public:
	EtiCmdlineETISource(std::string binary, DAB_LIVE_SOURCE_CHANNEL channel, EnsembleSourceObserver *observer) : DABLiveETISource(binary, channel, observer, TYPE_ETI_CMDLINE) {}
};

#endif /* ETI_SOURCE_H_ */
