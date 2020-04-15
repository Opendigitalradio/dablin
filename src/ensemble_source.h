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

#ifndef ENSEMBLE_SOURCE_H_
#define ENSEMBLE_SOURCE_H_

#include <sys/select.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <atomic>
#include <string>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <vector>

#include "tools.h"


struct ENSEMBLE_PROGRESS {
	double value;
	std::string text;
};

struct SYNC_MAGIC {
	size_t offset;
	std::vector<uint8_t> bytes;
	std::string name;

	SYNC_MAGIC(size_t offset, std::vector<uint8_t> bytes, std::string name) : offset(offset), bytes(bytes), name(name) {}

	size_t len() const {return offset + bytes.size();}
	bool matches(const uint8_t* data) const {return !memcmp(data + offset, &bytes[0], bytes.size());}
};

typedef std::vector<SYNC_MAGIC> sync_magics_t;


// --- EnsembleSourceObserver -----------------------------------------------------------------
class EnsembleSourceObserver {
public:
	virtual ~EnsembleSourceObserver() {}

	virtual void EnsembleProcessFrame(const uint8_t* /*data*/) {}
	virtual void EnsembleUpdateProgress(const ENSEMBLE_PROGRESS& /*progress*/) {}
	virtual void EnsembleDoRegularWork() {}
};


// --- EnsembleSource -----------------------------------------------------------------
class EnsembleSource {
protected:
	std::string filename;
	EnsembleSourceObserver *observer;
	std::string format_name;
	size_t initial_frame_size;
	sync_magics_t sync_magics;
	size_t sync_magics_max_len;

	std::atomic<bool> do_exit;

	FILE *input_file;

	std::vector<uint8_t> ensemble_frame;
	size_t ensemble_frames_count;
	size_t ensemble_bytes_count;
	size_t ensemble_bytes_total;
	unsigned long int ensemble_progress_next_ms;

	void AddSyncMagic(size_t offset, std::vector<uint8_t> bytes, std::string name);
	bool OpenFile();
	bool UpdateTotalBytes();
	bool UpdateProgress();
	virtual void Init() {}
	virtual void PrintSource();

	virtual bool CheckFrameCompleted(const SYNC_MAGIC& matched_sync_magic) = 0;
	virtual void ProcessCompletedFrame(const SYNC_MAGIC& /*matched_sync_magic*/) {observer->EnsembleProcessFrame(&ensemble_frame[0]);}
public:
	EnsembleSource(std::string filename, EnsembleSourceObserver *observer, std::string format_name, size_t initial_frame_size);
	virtual ~EnsembleSource();

	int Main();
	void DoExit() {do_exit = true;}

	static const std::string FORMAT_ETI;
	static const std::string FORMAT_EDI;
};

#endif /* ENSEMBLE_SOURCE_H_ */
