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

#ifndef DABLIN_H_
#define DABLIN_H_

#include <signal.h>
#include <string>

#include "eti_source.h"
#include "eti_player.h"
#include "edi_source.h"
#include "edi_player.h"
#include "fic_decoder.h"
#include "tools.h"
#include "version.h"


// --- DABlinTextOptions -----------------------------------------------------------------
struct DABlinTextOptions {
	std::string filename;
	std::string source_format;
	std::string initial_label;
	bool initial_first_found_service;
	int initial_sid;
	int initial_scids;
	int initial_subchid_dab;
	int initial_subchid_dab_plus;
	std::string dab_live_source_binary;
	std::string dab_live_source_type;
	std::string initial_channel;
	bool pcm_output;
	bool untouched_output;
	bool disable_int_catch_up;
	bool disable_dyn_fic_msgs;
	int gain;
DABlinTextOptions() :
	source_format(EnsembleSource::FORMAT_ETI),
	initial_first_found_service(false),
	initial_sid(LISTED_SERVICE::sid_none),
	initial_scids(LISTED_SERVICE::scids_none),
	initial_subchid_dab(AUDIO_SERVICE::subchid_none),
	initial_subchid_dab_plus(AUDIO_SERVICE::subchid_none),
	dab_live_source_type(DABLiveETISource::TYPE_DAB2ETI),
	pcm_output(false),
	untouched_output(false),
	disable_int_catch_up(false),
	disable_dyn_fic_msgs(false),
	gain(DAB_LIVE_SOURCE_CHANNEL::auto_gain)
	{}
};


// --- DABlinText -----------------------------------------------------------------
class DABlinText : EnsembleSourceObserver, EnsemblePlayerObserver, FICDecoderObserver {
private:
	DABlinTextOptions options;

	EnsembleSource *ensemble_source;
	EnsemblePlayer *ensemble_player;
	FICDecoder *fic_decoder;

	void EnsembleProcessFrame(const uint8_t *data) {ensemble_player->ProcessFrame(data);}
	void EnsembleUpdateProgress(const ENSEMBLE_PROGRESS& progress);

	void EnsembleProcessFIC(const uint8_t *data, size_t len) {fic_decoder->Process(data, len);}

	void FICChangeService(const LISTED_SERVICE& service);
	void FICDiscardedFIB();
public:
	DABlinText(DABlinTextOptions options);
	~DABlinText();

	void DoExit() {ensemble_source->DoExit();}
	int Main() {return ensemble_source->Main();}
};


#endif /* DABLIN_H_ */
