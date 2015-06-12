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

#ifndef ETI_SOURCE_H_
#define ETI_SOURCE_H_

#include <cstdio>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <fcntl.h>


// --- ETISourceObserver -----------------------------------------------------------------
class ETISourceObserver {
public:
	virtual ~ETISourceObserver() {};

	virtual void ETIProcessFrame(const uint8_t *data) {};
};


// --- ETISource -----------------------------------------------------------------
class ETISource {
private:
	std::string filename;
	ETISourceObserver *observer;

	std::mutex status_mutex;
	bool do_exit;

	FILE *input_file;

	uint8_t eti_frame[6144];
public:
	ETISource(std::string filename, ETISourceObserver *observer);
	~ETISource();

	int Main();
	void DoExit();
};



#endif /* ETI_SOURCE_H_ */
