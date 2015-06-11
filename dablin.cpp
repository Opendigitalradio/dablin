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

#include "dablin.h"

static DABlinText *dablin = NULL;

static void break_handler(int param) {
	fprintf(stderr, "...DABlin exits...\n");
	if(dablin)
		dablin->DoExit();
}


static void usage(const char* exe) {
	fprintf(stderr, "DABlin - plays a DAB(+) subchannel from a frame-aligned ETI(NI) stream via stdin\n");
	fprintf(stderr, "Usage: %s -s <sid> [file]\n", exe);
	fprintf(stderr, "  -s <sid>      ID of the service to be played\n");
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
	while((c = getopt(argc, argv, "s:")) != -1) {
		switch(c) {
		case 's':
			options.initial_sid = strtol(optarg, NULL, 0);
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


	// SID needed!
	if(options.initial_sid == -1)
		usage(argv[0]);


	fprintf(stderr, "DABlin - capital DAB experience\n");


	dablin = new DABlinText(options);
	int result = dablin->Main();
	delete dablin;

	return result;
}



// --- DABlinText -----------------------------------------------------------------
DABlinText::DABlinText(DABlinTextOptions options) {
	this->options = options;

	eti_player = new ETIPlayer(options.filename, this);
	fic_decoder = new FICDecoder(this);
}

DABlinText::~DABlinText() {
	DoExit();
	delete eti_player;
	delete fic_decoder;
}

void DABlinText::FICChangeServices() {
//	fprintf(stderr, "### FICChangeServices\n");

	services_t new_services = fic_decoder->GetNewServices();

	for(services_t::iterator it = new_services.begin(); it != new_services.end(); it++)
		if(it->sid == options.initial_sid)
			eti_player->SetAudioSubchannel(it->service.subchid, it->service.dab_plus);
}
