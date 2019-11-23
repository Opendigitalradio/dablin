/*
    DABlin - capital DAB experience
    Copyright (C) 2019 Stefan Pöschel

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

#include "edi_source.h"


// --- EDISource -----------------------------------------------------------------
bool EDISource::CheckFrameCompleted() {
	if(ensemble_frame.size() == 10) {
		// 1. AF header only (to retrieve payload len)
		size_t len = ensemble_frame[2] << 24 | ensemble_frame[3] << 16 | ensemble_frame[4] << 8 | ensemble_frame[5];
		ensemble_frame.resize(10 + len + 2);
		return false;
	} else {
		// 2. complete AF packet
		return true;
	}
}
