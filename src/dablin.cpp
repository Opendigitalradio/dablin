/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2018 Stefan Pöschel
    Copyright (C) 2018 Andy Mace (Windows/Network Additions)

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

static DABlinText *dablin = nullptr;

static void break_handler(int) {
	fprintf(stderr, "...DABlin exits...\n");
	if(dablin)
		dablin->DoExit();
}


static void usage(const char* exe) {
	fprint_dablin_banner(stderr);
	fprintf(stderr, "Usage: %s [OPTIONS] [file]\n", exe);
	fprintf(stderr, "  -h            Show this help\n"
					"  -d <binary>   Use DAB live source (using the mentioned binary)\n"
					"  -D <type>     DAB live source type: \"%s\" (default), \"%s\"\n"
					"  -c <ch>       Channel to be played (requires DAB live source)\n"
					"  -l <label>    Label of the service to be played\n"
					"  -s <sid>      ID of the service to be played\n"
					"  -x <scids>    ID of the service component to be played (requires service ID)\n"
					"  -r <subchid>  ID of the sub-channel (DAB) to be played\n"
					"  -R <subchid>  ID of the sub-channel (DAB+) to be played\n"
					"  -g <gain>     USB stick gain to pass to DAB live source (auto gain is default)\n"
					"  -p            Output PCM to stdout instead of using SDL\n"
					"  -u <url>      Host of ETI-NA Mux. (RAW) eg mygeneratedmux:8181\n"
					"  -f            Input file to be played ('stdin' if stdin is required)\n",
					DABLiveETISource::TYPE_DAB2ETI.c_str(),
					DABLiveETISource::TYPE_ETI_CMDLINE.c_str()
			);
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
	int initial_param_count = 0;

	// option args
	int c;
	while((c = getopt(argc, argv, "hc:l:d:D:g:s:x:pr:R:u:f:")) != -1) {
		switch(c) {
		case 'h':
			usage(argv[0]);
			break;
		case 'd':
			options.dab_live_source_binary = optarg;
			break;
		case 'D':
			options.dab_live_source_type = optarg;
			break;
		case 'c':
			options.initial_channel = optarg;
			break;
		case 'l':
			options.initial_label = optarg;
			initial_param_count++;
			break;
		case 's':
			options.initial_sid = strtol(optarg, nullptr, 0);
			initial_param_count++;
			break;
		case 'x':
			options.initial_scids = strtol(optarg, nullptr, 0);
			break;
		case 'r':
			options.initial_subchid_dab = strtol(optarg, nullptr, 0);
			initial_param_count++;
			break;
		case 'R':
			options.initial_subchid_dab_plus = strtol(optarg, nullptr, 0);
			initial_param_count++;
			break;
		case 'g':
			options.gain = strtol(optarg, nullptr, 0);
			break;
		case 'p':
			options.pcm_output = true;
			break;
		case 'u':
			options.url_input = optarg;
			break;
		case 'f':
			options.filename = optarg;
			break;
	 case '?':
		default:
			usage(argv[0]);
		}
	}

	if(options.filename.empty() && options.url_input.empty()) {
			fprintf(stderr, "You must set an input type\n");
			usage(argv[0]);
		}
	
	
	// ensure valid options
	if(options.dab_live_source_binary.empty()) {
		if(!options.initial_channel.empty()) {
			fprintf(stderr, "If a channel is selected, DAB live source must be used!\n");
			usage(argv[0]);
		}
	} else {
		
		if(!options.filename.empty()) {
			fprintf(stderr, "Both a file and DAB live source cannot be used as source!\n");
			usage(argv[0]);
		}
		if(options.initial_channel.empty()) {
			fprintf(stderr, "If DAB live source is used, a channel must be selected!\n");
			usage(argv[0]);
		}
		if(dab_channels.find(options.initial_channel) == dab_channels.end()) {
			fprintf(stderr, "The channel '%s' is not supported!\n", options.initial_channel.c_str());
			usage(argv[0]);
		}
		if(options.dab_live_source_type != DABLiveETISource::TYPE_DAB2ETI && options.dab_live_source_type != DABLiveETISource::TYPE_ETI_CMDLINE) {
			fprintf(stderr, "The DAB live source type '%s' is not supported!\n", options.dab_live_source_type.c_str());
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


	// at most one initial param needed!
	if(initial_param_count > 1) {
		fprintf(stderr, "At most one initial parameter shall be specified!\n");
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

	if(options.dab_live_source_binary.empty()) {
		//Needs some logic around where to pull the input from
		
		if(!options.filename.empty()) {
			fprintf(stderr, "Using File Input: %s\n", options.filename.c_str());
			eti_source = new ETISource(options.filename, this, false);
		} else {
			fprintf(stderr, "Using URL Input: %s\n", options.url_input.c_str());
			eti_source = new ETISource(options.url_input, this, true);
		}
		
	} else {
		DAB_LIVE_SOURCE_CHANNEL channel(options.initial_channel, dab_channels.at(options.initial_channel), options.gain);

		if(options.dab_live_source_type == DABLiveETISource::TYPE_ETI_CMDLINE)
			eti_source = new EtiCmdlineETISource(options.dab_live_source_binary, channel, this);
		else
			eti_source = new DAB2ETIETISource(options.dab_live_source_binary, channel, this);
	}

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

	std::string label = FICDecoder::ConvertLabelToUTF8(service.label);

	// abort, if no/not initial service
	if(!(label == options.initial_label || (service.sid == options.initial_sid && service.scids == options.initial_scids)))
		return;

	// if the audio service changed, switch
	if(!eti_player->IsSameAudioService(service.audio_service))
		eti_player->SetAudioService(service.audio_service);

	// set XTerm window title to service name
	fprintf(stderr, "\x1B]0;" "%s - DABlin" "\a", label.c_str());
}
