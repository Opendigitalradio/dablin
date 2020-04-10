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

#include "eti_source.h"


// --- DABLiveETISource -----------------------------------------------------------------
const std::string DABLiveETISource::TYPE_DAB2ETI = "dab2eti";
const std::string DABLiveETISource::TYPE_ETI_CMDLINE = "eti-cmdline";

DABLiveETISource::DABLiveETISource(std::string binary, DAB_LIVE_SOURCE_CHANNEL channel, EnsembleSourceObserver *observer, std::string source_name) : ETISource("", observer) {
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
		perror("DABLiveETISource: error starting DAB live source");
}

void DABLiveETISource::PrintSource() {
	fprintf(stderr, "DABLiveETISource: playing %s from channel %s (%u kHz) via %s (gain: %s)\n", format_name.c_str(), channel.block.c_str(), channel.freq, source_name.c_str(), channel.GainToString().c_str());
}

DABLiveETISource::~DABLiveETISource() {
	// kill source, if not yet terminated
	if(!feof(input_file)) {
		// TODO: replace bad style temporary solution (here possible, because dab2eti allows only one concurrent session)
		std::string cmd_killall = "killall " + binary_name;
		int result = system(cmd_killall.c_str());
		if(result != 0)
			fprintf(stderr, "DABLiveETISource: error killing %s\n", source_name.c_str());
	}

	pclose(input_file);
	input_file = nullptr;
}


// --- DAB2ETIETISource -----------------------------------------------------------------
std::string DAB2ETIETISource::GetParams() {
	std::string result = std::to_string(channel.freq * 1000);
	switch(channel.gain) {
	case DAB_LIVE_SOURCE_CHANNEL::auto_gain:
	case DAB_LIVE_SOURCE_CHANNEL::default_gain:
		// here: default = auto
		break;
	default:
		result += " " + std::to_string(channel.gain);
	}
	return result;
}


// --- EtiCmdlineETISource -----------------------------------------------------------------
std::string EtiCmdlineETISource::GetParams() {
	std::string cmdline = "-C " + channel.block + " -S -B " + (channel.freq < 1000000 ? "BAND_III" : "L_BAND");
	switch(channel.gain) {
	case DAB_LIVE_SOURCE_CHANNEL::auto_gain:
		cmdline += " -Q";
		break;
	case DAB_LIVE_SOURCE_CHANNEL::default_gain:
		// no parameter
		break;
	default:
		cmdline += " -G " + std::to_string(channel.gain);
	}
	return cmdline;
}
