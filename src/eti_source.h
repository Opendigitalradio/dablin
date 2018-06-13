/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2018 Stefan Pöschel

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

// support 2GB+ files on 32bit systems
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <atomic>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <fcntl.h>


#include<sys/socket.h>    //socket
#include<arpa/inet.h>
#include<netdb.h>
#include <netinet/in.h>




struct ETI_PROGRESS {
	double value;
	std::string text;
};

struct DAB_LIVE_SOURCE_CHANNEL {
	std::string block;
	uint32_t freq;
	int gain;

	static const int auto_gain = -10000;
	bool HasAutoGain() const {return gain == auto_gain;}
	std::string GainToString() {return HasAutoGain() ? "auto" : std::to_string(gain);}

	DAB_LIVE_SOURCE_CHANNEL() : freq(-1), gain(auto_gain) {}
	DAB_LIVE_SOURCE_CHANNEL(std::string block, uint32_t freq, int gain) : block(block), freq(freq), gain(gain) {}

	bool operator<(const DAB_LIVE_SOURCE_CHANNEL & ch) const {
		return freq < ch.freq;
	}
};


// --- ETISourceObserver -----------------------------------------------------------------
class ETISourceObserver {
public:
	virtual ~ETISourceObserver() {};

	virtual void ETIProcessFrame(const uint8_t* /*data*/) {};
	virtual void ETIUpdateProgress(const ETI_PROGRESS /*progress*/) {};
};


// --- ETISource -----------------------------------------------------------------
class ETISource {
protected:
	std::string filename;
	ETISourceObserver *observer;
	bool isURL;
	
	int sock;
	struct sockaddr_in server;
    
	std::atomic<bool> do_exit;

	FILE *input_file;

	uint8_t eti_frame[6144];
	size_t eti_frame_count;
	size_t eti_frame_total;
	unsigned long int eti_progress_next_ms;

	bool OpenFile();
	bool UpdateTotalFrames();
	virtual void Init() {}
	virtual void PrintSource();

	static std::string FramecountToTimecode(size_t value);
public:
	ETISource(std::string filename, ETISourceObserver *observer, bool isURL);
	virtual ~ETISource();

	int Main();
	void DoExit() {do_exit = true;}
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
	DABLiveETISource(std::string binary, DAB_LIVE_SOURCE_CHANNEL channel, ETISourceObserver *observer, std::string source_name);
	~DABLiveETISource();

	static const std::string TYPE_DAB2ETI;
	static const std::string TYPE_ETI_CMDLINE;
};


// --- DAB2ETIETISource -----------------------------------------------------------------
class DAB2ETIETISource : public DABLiveETISource {
protected:
	std::string GetParams();
public:
	DAB2ETIETISource(std::string binary, DAB_LIVE_SOURCE_CHANNEL channel, ETISourceObserver *observer) : DABLiveETISource(binary, channel, observer, "dab2eti") {}
};


// --- EtiCmdlineETISource -----------------------------------------------------------------
class EtiCmdlineETISource : public DABLiveETISource {
protected:
	std::string GetParams();
public:
	EtiCmdlineETISource(std::string binary, DAB_LIVE_SOURCE_CHANNEL channel, ETISourceObserver *observer) : DABLiveETISource(binary, channel, observer, "eti-cmdline") {}
};

#endif /* ETI_SOURCE_H_ */
