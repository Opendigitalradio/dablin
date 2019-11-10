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
#include <vector>

#include "tools.h"


struct ENSEMBLE_PROGRESS {
	double value;
	std::string text;
};


// --- EnsembleSourceObserver -----------------------------------------------------------------
class EnsembleSourceObserver {
public:
	virtual ~EnsembleSourceObserver() {}

	virtual void EnsembleProcessFrame(const uint8_t* /*data*/) {}
	virtual void EnsembleUpdateProgress(const ENSEMBLE_PROGRESS& /*progress*/) {}
};


// --- EnsembleSource -----------------------------------------------------------------
class EnsembleSource {
protected:
	std::string filename;
	EnsembleSourceObserver *observer;
	std::string format_name;
	size_t initial_frame_size;

	std::atomic<bool> do_exit;

	FILE *input_file;

	std::vector<uint8_t> ensemble_frame;
	size_t ensemble_frames_count;
	size_t ensemble_bytes_count;
	size_t ensemble_bytes_total;
	unsigned long int ensemble_progress_next_ms;

	bool OpenFile();
	bool UpdateTotalBytes();
	bool UpdateProgress();
	virtual void Init() {}
	virtual void PrintSource();

	virtual size_t CheckFrameSync() = 0;
	virtual bool CheckFrameCompleted() = 0;
public:
	EnsembleSource(std::string filename, EnsembleSourceObserver *observer, std::string format_name, size_t initial_frame_size);
	virtual ~EnsembleSource();

	int Main();
	void DoExit() {do_exit = true;}
};

#endif /* ENSEMBLE_SOURCE_H_ */
