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

#include "eti_source.h"


// --- ETISource -----------------------------------------------------------------
ETISource::ETISource(std::string filename, ETISourceObserver *observer) {
	this->filename = filename;
	this->observer = observer;

	input_file = NULL;

	do_exit = false;
}

ETISource::~ETISource() {
	// cleanup
	if(input_file && input_file != stdin)
		fclose(input_file);
}

void ETISource::DoExit() {
	std::lock_guard<std::mutex> lock(status_mutex);

	do_exit = true;
}

void ETISource::PrintSource() {
	fprintf(stderr, "ETISource: reading from '%s'\n", filename.c_str());
}

int ETISource::Main() {
	int file_no;

	if(!input_file) {
		if(filename.empty()) {
			input_file = stdin;
			filename = "stdin";
		} else {
			input_file = fopen(filename.c_str(), "rb");
			if(!input_file) {
				perror("ETISource: error opening input file");
				return 1;
			}
		}
	}

	file_no = fileno(input_file);
	PrintSource();


	// set non-blocking mode
	int old_flags = fcntl(file_no, F_GETFL);
	if(old_flags == -1) {
		perror("ETISource: error getting socket flags");
		return 1;
	}
	if(fcntl(file_no, F_SETFL, (old_flags == -1 ? 0 : old_flags) | O_NONBLOCK)) {
		perror("ETISource: error setting socket flags");
		return 1;
	}

	fd_set fds;
	timeval select_timeval;
	size_t filled = 0;

	for(;;) {
		{
			std::lock_guard<std::mutex> lock(status_mutex);

			if(do_exit)
				break;
		}

		FD_ZERO(&fds);
		FD_SET(file_no, &fds);

		select_timeval.tv_sec = 0;
		select_timeval.tv_usec = 100 * 1000;

		int ready_fds = select(file_no + 1, &fds, NULL, NULL, &select_timeval);
		if(!(ready_fds && FD_ISSET(file_no, &fds)))
			continue;

		ssize_t bytes = read(file_no, eti_frame + filled, sizeof(eti_frame) - filled);

		if(bytes > 0)
			filled += bytes;
		if(bytes == 0) {
			fprintf(stderr, "ETISource: EOF reached!\n");
			break;
		}
		if(bytes == -1) {
			perror("ETISource: error while read");
			return 1;
		}

		if(filled < sizeof(eti_frame))
			continue;

		observer->ETIProcessFrame(eti_frame);
		filled = 0;
	}

	return 0;
}


// --- DAB2ETISource -----------------------------------------------------------------
DAB2ETISource::DAB2ETISource(std::string binary, uint32_t freq, ETISourceObserver *observer) : ETISource("", observer) {
	this->freq = freq;

	// it doesn't matter whether there is a prefixed path or not
	binary_name = binary.substr(binary.find_last_of('/') + 1);

	std::stringstream ss;
	ss << binary << " " << (freq * 1000);

	input_file = popen(ss.str().c_str(), "r");
	if(!input_file)
		perror("ETISource: error starting dab2eti");
}

void DAB2ETISource::PrintSource() {
	fprintf(stderr, "ETISource: playing live from %u kHz via dab2eti\n", freq);
}

DAB2ETISource::~DAB2ETISource() {
	// TODO: replace bad style temporary solution (here possible, because dab2eti allows only one concurrent session)
	std::string cmd_killall = "killall " + binary_name;
	int result = system(cmd_killall.c_str());
	if(result != 0)
		fprintf(stderr, "ETISource: error killing dab2eti\n");

	pclose(input_file);
	input_file = NULL;
}
