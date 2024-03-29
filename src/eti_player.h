/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2024 Stefan Pöschel

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

#ifndef ETI_PLAYER_H_
#define ETI_PLAYER_H_

#include "ensemble_player.h"


// --- ETIPlayer -----------------------------------------------------------------
class ETIPlayer : public EnsemblePlayer {
private:
	uint32_t prev_fsync;

	void DecodeFrame(const uint8_t *eti_frame);
public:
	ETIPlayer(AudioOutputType audio_output_type, bool disable_int_catch_up, EnsemblePlayerObserver *observer)
		: EnsemblePlayer(audio_output_type, disable_int_catch_up, observer), prev_fsync(0) {}
	~ETIPlayer() {}
};

#endif /* ETI_PLAYER_H_ */
