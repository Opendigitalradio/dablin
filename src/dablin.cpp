/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2017 Stefan Pöschel

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

static void break_handler(int) {
	fprintf(stderr, "...DABlin exits...\n");
	if(dablin)
		dablin->DoExit();
}


static void usage(const char* exe) {
	fprint_dablin_banner(stderr);
	fprintf(stderr, "Usage: %s [OPTIONS] [file]\n", exe);
	fprintf(stderr, "  -h            Show this help\n");
	fprintf(stderr, "  -d <binary>   Use DAB live source (using the mentioned binary)\n");
	fprintf(stderr, "  -c <ch>       Channel to be played (requires DAB live source)\n");
	fprintf(stderr, "  -s <sid>      ID of the service to be played\n");
	fprintf(stderr, "  -x <scids>    ID of the service component to be played (requires service ID)\n");
	fprintf(stderr, "  -r <subchid>  ID of the sub-channel (DAB) to be played\n");
	fprintf(stderr, "  -R <subchid>  ID of the sub-channel (DAB+) to be played\n");
	fprintf(stderr, "  -g <gain>     USB stick gain to pass to DAB live source (auto gain is default)\n");
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
	if(signal(SIGTERM, break_handler) == SIG_ERR) {
		perror("DABlin: error while setting SIGTERM handler");
		return 1;
	}

	DABlinTextOptions options;
	int id_param_count = 0;

	// option args
	int c;
	while((c = getopt(argc, argv, "hc:d:g:s:x:pr:R:")) != -1) {
		switch(c) {
		case 'h':
			usage(argv[0]);
			break;
		case 'd':
			options.dab2eti_binary = optarg;
			break;
		case 'c':
			options.initial_channel = optarg;
			break;
		case 's':
			options.initial_sid = strtol(optarg, NULL, 0);
			id_param_count++;
			break;
		case 'x':
			options.initial_scids = strtol(optarg, NULL, 0);
			break;
		case 'r':
			options.initial_subchid_dab = strtol(optarg, NULL, 0);
			id_param_count++;
			break;
		case 'R':
			options.initial_subchid_dab_plus = strtol(optarg, NULL, 0);
			id_param_count++;
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
	if(options.initial_scids != LISTED_SERVICE::scids_none && options.initial_sid == LISTED_SERVICE::sid_none) {
		fprintf(stderr, "The service component ID requires the service ID to be specified!\n");
		usage(argv[0]);
	}
#ifdef DABLIN_DISABLE_SDL
	if(!options.pcm_output) {
		fprintf(stderr, "SDL output was disabled, so PCM output must be selected!\n");
		usage(argv[0]);
	}
#endif


	// at most one SId/SubChId needed!
	if(id_param_count > 1) {
		fprintf(stderr, "At most one SId or SubChId shall be specified!\n");
		usage(argv[0]);
	}


	fprint_dablin_banner(stderr);

	dablin = new DABlinText(options);
	int result = dablin->Main();
	delete dablin;

	return result;
}



// --- DABlinText -----------------------------------------------------------------
DABlinText::DABlinText(DABlinTextOptions options) {
	this->options = options;

	// set XTerm window title to version string
	fprintf(stderr, "\x1B]0;" "DABlin v" DABLIN_VERSION "\a");

	eti_player = new ETIPlayer(options.pcm_output, this);

	// set initial sub-channel, if desired
	if(options.initial_subchid_dab != AUDIO_SERVICE::subchid_none) {
		eti_player->SetAudioService(AUDIO_SERVICE(options.initial_subchid_dab, false));

		// set XTerm window title to sub-channel number
		fprintf(stderr, "\x1B]0;" "Sub-channel %d (DAB) - DABlin" "\a", options.initial_subchid_dab);
	}
	if(options.initial_subchid_dab_plus != AUDIO_SERVICE::subchid_none) {
		eti_player->SetAudioService(AUDIO_SERVICE(options.initial_subchid_dab_plus, true));

		// set XTerm window title to sub-channel number
		fprintf(stderr, "\x1B]0;" "Sub-channel %d (DAB+) - DABlin" "\a", options.initial_subchid_dab_plus);
	}

	if(options.dab2eti_binary.empty())
		eti_source = new ETISource(options.filename, this);
	else
		eti_source = new DAB2ETISource(options.dab2eti_binary, DAB2ETI_CHANNEL(dab_channels.at(options.initial_channel), options.gain), this);

	fic_decoder = new FICDecoder(this);
}

DABlinText::~DABlinText() {
	DoExit();
	delete eti_source;
	delete eti_player;
	delete fic_decoder;
}

void DABlinText::ETIUpdateProgress(const ETI_PROGRESS progress) {
	// compensate cursor movement
	std::string format = "\x1B[34m" "%s" "\x1B[0m";
	format.append(progress.text.length(), '\b');

	fprintf(stderr, format.c_str(), progress.text.c_str());
}

void DABlinText::FICChangeService(const LISTED_SERVICE& service) {
//	fprintf(stderr, "### FICChangeService\n");

	// abort, if no/not initial service
	if(options.initial_sid == LISTED_SERVICE::sid_none || service.sid != options.initial_sid || service.scids != options.initial_scids)
		return;

	// if the audio service changed, switch
	if(!eti_player->IsSameAudioService(service.audio_service))
		eti_player->SetAudioService(service.audio_service);

	// set XTerm window title to service name
	std::string label = FICDecoder::ConvertLabelToUTF8(service.label);
	fprintf(stderr, "\x1B]0;" "%s - DABlin" "\a", label.c_str());
}
