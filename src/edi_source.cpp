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
bool EDISource::CheckFrameCompleted(const SYNC_MAGIC& matched_sync_magic) {
	if(ensemble_frame.size() == 8) {
		// 1. header only (to retrieve payload len)
		if(matched_sync_magic.name == "AF") {
			size_t len = ensemble_frame[2] << 24 | ensemble_frame[3] << 16 | ensemble_frame[4] << 8 | ensemble_frame[5];
			ensemble_frame.resize(10 + len + 2);
		} else {
			size_t len = ensemble_frame[4] << 24 | ensemble_frame[5] << 16 | ensemble_frame[6] << 8 | ensemble_frame[7];
			ensemble_frame.resize(4 + 4 + len / 8);
		}
		return false;
	} else {
		// 2. complete packet
		return true;
	}
}

void EDISource::ProcessCompletedFrame(const SYNC_MAGIC& matched_sync_magic) {
	// show layer
	if(layer != matched_sync_magic.name) {
		layer = matched_sync_magic.name;
		fprintf(stderr, "EDISource: detected %s layer\n", layer.c_str());
	}

	if(matched_sync_magic.name == "AF") {
		// forward to player
		observer->EnsembleProcessFrame(&ensemble_frame[0]);
	} else {
		// parse TAG packet for TAG items (skipping any TAG packet padding)
		size_t tag_item_len_bytes;
		for(size_t i = 0; i < ensemble_frame.size() - 8; i += tag_item_len_bytes) {
			const uint8_t* tag_item = &ensemble_frame[8 + i];

			std::string tag_name((const char*) tag_item, 4);
			size_t tag_len = tag_item[4] << 24 | tag_item[5] << 16 | tag_item[6] << 8 | tag_item[7];
			const uint8_t* tag_value = tag_item + 8;

			tag_item_len_bytes = 4 + 4 + (tag_len + 7) / 8;

			// AF Packet/PFT Fragment
			if(tag_name == "afpf") {
				// forward to player
				observer->EnsembleProcessFrame(tag_value);

				continue;
			}

			// Timestamp - ignored
			if(tag_name == "time")
				continue;

			fprintf(stderr, "EDISource: ignored unsupported TAG item '%s' (%zu bits)\n", tag_name.c_str(), tag_len);
		}
	}
}
