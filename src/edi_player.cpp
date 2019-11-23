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

#include "edi_player.h"


// --- EDIPlayer -----------------------------------------------------------------
void EDIPlayer::DecodeFrame(const uint8_t *edi_frame) {
	// SYNC
	uint16_t sync = edi_frame[0] << 8 | edi_frame[1];
	switch(sync) {
	case 0x4146:	// AF
		// supported
		break;
	case 0x5046:	// PF
		fprintf(stderr, "EDIPlayer: ignored unsupported EDI PF packet\n");
		return;
	default:
		fprintf(stderr, "EDIPlayer: ignored EDI packet with SYNC = 0x%04X\n", sync);
		return;
	}

	// LEN
	size_t len = edi_frame[2] << 24 | edi_frame[3] << 16 | edi_frame[4] << 8 | edi_frame[5];

	// CF
	bool cf = edi_frame[8] & 0x80;
	if(!cf) {
		fprintf(stderr, "EDIPlayer: ignored EDI AF packet without CRC\n");
		return;
	}

	// MAJ
	int maj = (edi_frame[8] & 0x70) >> 4;
	if(maj != 0x01) {
		fprintf(stderr, "EDIPlayer: ignored EDI AF packet with MAJ = 0x%02X\n", maj);
		return;
	}

	// MIN
	int min = edi_frame[8] & 0x0F;
	if(min != 0x00) {
		fprintf(stderr, "EDIPlayer: ignored EDI AF packet with MIN = 0x%02X\n", min);
		return;
	}

	// PT
	if(edi_frame[9] != 'T') {
		fprintf(stderr, "EDIPlayer: ignored EDI AF packet with PT = '%c'\n", edi_frame[9]);
		return;
	}

	// check CRC
	uint16_t crc_stored = edi_frame[10 + len] << 8 | edi_frame[10 + len + 1];
	uint16_t crc_calced = CalcCRC::CalcCRC_CRC16_CCITT.Calc(edi_frame, 10 + len);
	if(crc_stored != crc_calced) {
		fprintf(stderr, "EDIPlayer: ignored EDI AF packet due to wrong CRC\n");
		return;
	}

	// parse TAG packet for TAG items (skipping any TAG packet padding)
	size_t tag_item_len_bytes;
	for(size_t i = 0; i < len - 8; i += tag_item_len_bytes) {
		const uint8_t* tag_item = edi_frame + 10 + i;

		std::string tag_name((const char*) tag_item, 4);
		size_t tag_len = tag_item[4] << 24 | tag_item[5] << 16 | tag_item[6] << 8 | tag_item[7];
		const uint8_t* tag_value = tag_item + 8;

		tag_item_len_bytes = 4 + 4 + (tag_len + 7) / 8;

		// protocol type and revision
		if(tag_name == "*ptr") {
			if(tag_len != 64) {
				fprintf(stderr, "EDIPlayer: ignored *ptr TAG item with wrong length (%zu bits)\n", tag_len);
				continue;
			}

			std::string protocol_type((const char*) tag_value, 4);
			if(protocol_type != "DETI")
				fprintf(stderr, "EDIPlayer: unsupported protocol type '%s' in *ptr TAG item\n", protocol_type.c_str());

			int major = tag_value[4] << 8 | tag_value[5];
			int minor = tag_value[6] << 8 | tag_value[7];
			if(major != 0x0000 || minor != 0x0000)
				fprintf(stderr, "EDIPlayer: unsupported major/minor revision 0x%04X/0x%04X in *ptr TAG item\n", major, minor);

			continue;
		}

		// dummy padding - ignored
		if(tag_name == "*dmy")
			continue;

		// DAB ETI(LI) Management
		if(tag_name == "deti") {
			// flag field
			bool atstf = tag_value[0] & 0x80;
			bool ficf = tag_value[0] & 0x40;
			bool rfudf = tag_value[0] & 0x20;

			// STAT
			if(tag_value[2] != 0xFF) {
				fprintf(stderr, "EDIPlayer: EDI AF packet with STAT = 0x%02X\n", tag_value[2]);
				continue;
			}

			int mid = tag_value[3] >> 6;
			size_t fic_len = ficf ? (mid == 3 ? 128 : 96) : 0;

			size_t tag_len_bytes_calced = 2 + 4 + (atstf ? 8 : 0) + fic_len + (rfudf ? 3 : 0);
			if(tag_len != tag_len_bytes_calced * 8) {
				fprintf(stderr, "EDIPlayer: ignored deti TAG item with wrong length (%zu bits)\n", tag_len);
				continue;
			}

			if(fic_len)
				ProcessFIC(tag_value + 2 + 4 + (atstf ? 8 : 0), fic_len);

			continue;
		}

		// ETI Sub-Channel Stream
		if(tag_name.substr(0, 3) == "est" && tag_item[3] >= 1 && tag_item[3] <= 64) {
			if(tag_len < 3 * 8) {
				fprintf(stderr, "EDIPlayer: ignored est<n> TAG item with too short length (%zu bits)\n", tag_len);
				continue;
			}

			int subchid = tag_value[0] >> 2;

			// lock only now, as due to the FIC, the audio service may be set
			std::lock_guard<std::mutex> lock(audio_service_mutex);

			if(subchid != audio_service.subchid)
				continue;

			dec->Feed(tag_value + 3, (tag_len / 8) - 3);

			continue;
		}

		// Information
		if(tag_name == "info") {
			std::string text((const char*) tag_value, tag_len / 8);

			fprintf(stderr, "EDIPlayer: info TAG item '%s'\n", text.c_str());
			continue;
		}

		// Network Adapted Signalling Channel - ignored
		if(tag_name == "nasc")
			continue;

		// Frame Padding User Data - ignored
		if(tag_name == "frpd")
			continue;

		fprintf(stderr, "EDIPlayer: ignored unsupported TAG item '%s' (%zu bits)\n", tag_name.c_str(), tag_len);
	}
}
