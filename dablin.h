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

#ifndef DABLIN_H_
#define DABLIN_H_

#include "eti_player.h"
#include "fic_decoder.h"


// --- DABlinText -----------------------------------------------------------------
class DABlinText : ETIPlayerObserver, FICDecoderObserver {
private:
	int initial_sid;

	ETIPlayer *eti_player;
	FICDecoder *fic_decoder;

	void ETIProcessFIC(const uint8_t *data, size_t len) {fic_decoder->Process(data, len);}
	void FICChangeServices();
public:
	DABlinText(std::string filename, int initial_sid);
	~DABlinText();
	void DoExit() {eti_player->DoExit();}
	int Main() {return eti_player->Main();}
};


#endif /* DABLIN_H_ */
