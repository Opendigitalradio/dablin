/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2016 Stefan Pöschel

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

#include "dablin.h"

static DABlinText *dablin = NULL;

static void break_handler(int param) {
	fprintf(stderr, "...DABlin exits...\n");
	if(dablin)
		dablin->DoExit();
}


static void usage(const char* exe) {
	fprintf(stderr, "DABlin - plays a DAB(+) subchannel from a frame-aligned ETI(NI) stream via stdin\n");
	fprintf(stderr, "Usage: %s [-d <binary> -c <ch>] -s <sid> [-p] [file]\n", exe);
	fprintf(stderr, "  -d <binary>   Use dab2eti as source (using the mentioned binary)\n");
	fprintf(stderr, "  -c <ch>       Channel to be played (requires dab2eti as source)\n");
	fprintf(stderr, "  -s <sid>      ID of the service to be played\n");
	fprintf(stderr, "  -g <gain>     Set USB stick gain to pass to dab2eti (auto_gain is default)\n");
	fprintf(stderr, "  -p            Output PCM to stdout instead of using SDL\n");
	fprintf(stderr, "  file          Input file to be played (stdin, if not specified)\n");
	exit(1);
}


int main(int argc, char **argv) {
	// handle signals
	if(signal(SIGINT, break_handler) == SIG_ERR) {
		perror("DABlin: error while setting SIGINT handler");
		return 1;
	}

	DABlinTextOptions options;

	// option args
	int c;
	while((c = getopt(argc, argv, "c:d:g:s:p")) != -1) {
		switch(c) {
		case 'd':
			options.dab2eti_binary = optarg;
			break;
		case 'c':
			options.initial_channel = optarg;
			break;
		case 's':
			options.initial_sid = strtol(optarg, NULL, 0);
			break;
		case 'g':
			options.gain = strtol(optarg, NULL, 0);
			break;
		case 'p':
			options.pcm_output = true;
			break;
		case '?':
		default:
			usage(argv[0]);
		}
	}

	// non-option args
	switch(argc - optind) {
	case 0:
		break;
	case 1:
		options.filename = argv[optind];
		break;
	default:
		usage(argv[0]);
	}

	// ensure valid options
	if(options.dab2eti_binary.empty()) {
		if(!options.initial_channel.empty()) {
			fprintf(stderr, "If a channel is selected, dab2eti must be used!\n");
			usage(argv[0]);
		}
	} else {
		if(!options.filename.empty()) {
			fprintf(stderr, "Both a file and dab2eti cannot be used as source!\n");
			usage(argv[0]);
		}
		if(options.initial_channel.empty()) {
			fprintf(stderr, "If dab2eti is used, a channel must be selected!\n");
			usage(argv[0]);
		}
		if(dab_channels.find(options.initial_channel) == dab_channels.end()) {
			fprintf(stderr, "The channel '%s' is not supported!\n", options.initial_channel.c_str());
			usage(argv[0]);
		}
	}
#ifdef DABLIN_DISABLE_SDL
	if(!options.pcm_output) {
		fprintf(stderr, "SDL output was disabled, so PCM output must be selected!\n");
		usage(argv[0]);
	}
#endif


	// SID needed!
	if(options.initial_sid == -1) {
		fprintf(stderr, "A service ID must be selected!\n");
		usage(argv[0]);
	}


	fprintf(stderr, "DABlin - capital DAB experience\n");


	dablin = new DABlinText(options);
	int result = dablin->Main();
	delete dablin;

	return result;
}



// --- DABlinText -----------------------------------------------------------------
DABlinText::DABlinText(DABlinTextOptions options) {
	this->options = options;

	eti_player = new ETIPlayer(options.pcm_output, this);

	if(options.dab2eti_binary.empty())
		eti_source = new ETISource(options.filename, this);
	else
		eti_source = new DAB2ETISource(options.dab2eti_binary, dab_channels.find(options.initial_channel)->second, options.gain, this);

	fic_decoder = new FICDecoder(this);
}

DABlinText::~DABlinText() {
	DoExit();
	delete eti_source;
	delete eti_player;
	delete fic_decoder;
}

void DABlinText::FICChangeServices() {
//	fprintf(stderr, "### FICChangeServices\n");

	services_t new_services = fic_decoder->GetNewServices();

	for(services_t::const_iterator it = new_services.cbegin(); it != new_services.cend(); it++)
		if(it->sid == options.initial_sid)
			eti_player->SetAudioSubchannel(it->service.subchid, it->service.dab_plus);
}
