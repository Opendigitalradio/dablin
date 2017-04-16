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

#ifndef DABLIN_H_
#define DABLIN_H_

#include <signal.h>

#include "eti_source.h"
#include "eti_player.h"
#include "fic_decoder.h"
#include "tools.h"
#include "version.h"


// --- DABlinTextOptions -----------------------------------------------------------------
struct DABlinTextOptions {
	std::string filename;
	int initial_sid;
	int initial_subchid_dab;
	int initial_subchid_dab_plus;
	std::string dab2eti_binary;
	std::string initial_channel;
	bool pcm_output;
	int gain;
DABlinTextOptions() : initial_sid(-1), initial_subchid_dab(-1), initial_subchid_dab_plus(-1), pcm_output(false), gain(DAB2ETI_AUTO_GAIN) {}
};


// --- DABlinText -----------------------------------------------------------------
class DABlinText : ETISourceObserver, ETIPlayerObserver, FICDecoderObserver {
private:
	DABlinTextOptions options;

	ETISource *eti_source;
	ETIPlayer *eti_player;
	FICDecoder *fic_decoder;

	void ETIProcessFrame(const uint8_t *data) {eti_player->ProcessFrame(data);};
	void ETIUpdateProgress(const ETI_PROGRESS progress);
	void ETIProcessFIC(const uint8_t *data, size_t len) {fic_decoder->Process(data, len);}
	void FICChangeService(const SERVICE& service);
public:
	DABlinText(DABlinTextOptions options);
	~DABlinText();
	void DoExit() {eti_source->DoExit();}
	int Main() {return eti_source->Main();}
};


#endif /* DABLIN_H_ */
