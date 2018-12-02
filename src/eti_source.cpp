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

#include "eti_source.h"


// --- ETISource -----------------------------------------------------------------
ETISource::ETISource(std::string filename, ETISourceObserver *observer) {
	this->filename = filename;
	this->observer = observer;

	input_file = nullptr;
	eti_frame_count = 0;
	eti_frame_total = 0;
	eti_progress_next_ms = 0;

	do_exit = false;
}

ETISource::~ETISource() {
	// cleanup
	if(input_file && input_file != stdin)
		fclose(input_file);
}

void ETISource::PrintSource() {
	fprintf(stderr, "ETISource: reading from '%s'\n", filename.c_str());
}

bool ETISource::OpenFile() {
	if(filename.empty()) {
		input_file = stdin;
		filename = "stdin";
	} else {
		input_file = fopen(filename.c_str(), "rb");
		if(!input_file) {
			perror("ETISource: error opening input file");
			return false;
		}
	}

	// init total frames
	return UpdateTotalFrames();
}

bool ETISource::UpdateTotalFrames() {
	// if file size available, calc total frame count
	off_t old_offset = ftello(input_file);
	if(old_offset == -1) {
		// ignore non-seekable files (like usually stdin)
		if(errno == ESPIPE)
			return true;
		perror("ETISource: error getting file offset");
		return false;
	}

	if(fseeko(input_file, 0, SEEK_END)) {
		perror("ETISource: error seeking to end");
		return false;
	}

	off_t len = ftello(input_file);
	if(len == -1) {
		perror("ETISource: error getting file size");
		return false;
	}

	if(fseeko(input_file, old_offset, SEEK_SET)) {
		perror("ETISource: error seeking to offset");
		return false;
	}

	eti_frame_total = (len / sizeof(eti_frame)) - 1;
	return true;
}

int ETISource::Main() {
	Init();

	if(!input_file) {
		if(!OpenFile())
			return 1;
	}

	PrintSource();

	int file_no = fileno(input_file);

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

	while(!do_exit) {
		FD_ZERO(&fds);
		FD_SET(file_no, &fds);

		select_timeval.tv_sec = 0;
		select_timeval.tv_usec = 100 * 1000;

		int ready_fds = select(file_no + 1, &fds, nullptr, nullptr, &select_timeval);
		if(ready_fds == -1) {
			// ignore break request, as handled above
			if(errno != EINTR)
				perror("ETISource: error while select");
			continue;
		}
		if(!(ready_fds && FD_ISSET(file_no, &fds)))
			continue;

		size_t bytes = fread(eti_frame + filled, 1, sizeof(eti_frame) - filled, input_file);

		if(bytes > 0)
			filled += bytes;
		if(bytes == 0) {
			if(feof(input_file)) {
				fprintf(stderr, "ETISource: EOF reached!\n");
				break;
			}
			perror("ETISource: error while fread");
			return 1;
		}

		if(filled < sizeof(eti_frame))
			continue;

		// if present, update progress every 500ms or at file end
		if(eti_frame_total && (eti_frame_count * 24 >= eti_progress_next_ms || eti_frame_count == eti_frame_total)) {
			// update total frames
			if(!UpdateTotalFrames())
				return 1;

			ETI_PROGRESS progress;
			progress.value = (double) eti_frame_count / (double) eti_frame_total;
			progress.text = MiscTools::MsToTimecode(eti_frame_count * 24) + " / " + MiscTools::MsToTimecode(eti_frame_total * 24);
			observer->ETIUpdateProgress(progress);

			eti_progress_next_ms += 500;
		}

		observer->ETIProcessFrame(eti_frame);
		eti_frame_count++;
		filled = 0;
	}

	return 0;
}


// --- DABLiveETISource -----------------------------------------------------------------
const std::string DABLiveETISource::TYPE_DAB2ETI = "dab2eti";
const std::string DABLiveETISource::TYPE_ETI_CMDLINE = "eti-cmdline";

DABLiveETISource::DABLiveETISource(std::string binary, DAB_LIVE_SOURCE_CHANNEL channel, ETISourceObserver *observer, std::string source_name) : ETISource("", observer) {
	this->channel = channel;
	this->binary = binary;
	this->source_name = source_name;

	// it doesn't matter whether there is a prefixed path or not
	binary_name = binary.substr(binary.find_last_of('/') + 1);
	binary_name = binary_name.substr(0, binary_name.find_first_of(' '));
}

void DABLiveETISource::Init() {
	std::string cmdline = binary + " " + GetParams();
	input_file = popen(cmdline.c_str(), "r");
	if(!input_file)
		perror("ETISource: error starting DAB live source");
}

void DABLiveETISource::PrintSource() {
	fprintf(stderr, "ETISource: playing from channel %s (%u kHz) via %s (gain: %s)\n", channel.block.c_str(), channel.freq, source_name.c_str(), channel.GainToString().c_str());
}

DABLiveETISource::~DABLiveETISource() {
	// TODO: replace bad style temporary solution (here possible, because dab2eti allows only one concurrent session)
	std::string cmd_killall = "killall " + binary_name;
	int result = system(cmd_killall.c_str());
	if(result != 0)
		fprintf(stderr, "ETISource: error killing %s\n", source_name.c_str());

	pclose(input_file);
	input_file = nullptr;
}


// --- DAB2ETIETISource -----------------------------------------------------------------
std::string DAB2ETIETISource::GetParams() {
	std::string result = std::to_string(channel.freq * 1000);
	if(!channel.HasAutoGain())
		result += " " + std::to_string(channel.gain);
	return result;
}


// --- EtiCmdlineETISource -----------------------------------------------------------------
std::string EtiCmdlineETISource::GetParams() {
	std::string cmdline = "-C " + channel.block + " -S -B " + (channel.freq < 1000000 ? "BAND_III" : "L_BAND");
	if(channel.HasAutoGain())
		cmdline += " -Q";
	else
		cmdline += " -G " + std::to_string(channel.gain);
	return cmdline;
}
