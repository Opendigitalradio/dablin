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

#include "pad_decoder.h"


// --- XPAD_CI -----------------------------------------------------------------
const size_t XPAD_CI::lens[] = {4, 6, 8, 12, 16, 24, 32, 48};

int XPAD_CI::GetContinuedLastCIType(int last_ci_type) {
	switch(last_ci_type) {
	case 1:		// Data group length indicator
		return 1;
	case 2:		// Dynamic Label segment
	case 3:
		return 3;
	case 12:	// MOT, X-PAD data group
	case 13:
		return 13;
	case -1:
	default:
		return -1;
	}
}


// --- PADDecoder -----------------------------------------------------------------
PADDecoder::PADDecoder(PADDecoderObserver *observer) {
	this->observer = observer;
	Reset();
}

void PADDecoder::Reset() {
	last_xpad_ci.Reset();
	{
		std::lock_guard<std::mutex> lock(data_mutex);

		dl_len = 0;
		dl_charset = -1;
	}

	dl_decoder.Reset();
}

size_t PADDecoder::GetDynamicLabel(uint8_t *data, int *charset) {
	std::lock_guard<std::mutex> lock(data_mutex);

	memcpy(data, dl_raw, dl_len);
	*charset = dl_charset;
	return dl_len;
}


void PADDecoder::Process(const uint8_t *xpad_data, size_t xpad_len, uint16_t fpad) {
	xpad_cis_t xpad_cis;
	size_t xpad_cis_len;

	int fpad_type = fpad >> 14;
	int xpad_ind = (fpad & 0x3000) >> 12;
	bool ci_flag = fpad & 0x0002;

	// build CI list
	if(fpad_type == 0b00) {
		if(ci_flag) {
			switch(xpad_ind) {
			case 0b01:	// short X-PAD
				xpad_cis_len = 1;
				xpad_cis.push_back(XPAD_CI(3, xpad_data[0] & 0x1F));
				break;
			case 0b10:	// variable size X-PAD
				xpad_cis_len = 0;
				for(size_t i = 0; i < 4; i++) {
					uint8_t ci_raw = xpad_data[i];
					xpad_cis_len++;

					// break on end marker
					if((ci_raw & 0x1F) == 0x00)
						break;

					xpad_cis.push_back(XPAD_CI(ci_raw));
				}
				break;
			}
		} else {
			switch(xpad_ind) {
			case 0b01:	// short X-PAD
			case 0b10:	// variable size X-PAD
				// if there is a last CI, append it
				if(last_xpad_ci.type != -1) {
					xpad_cis_len = 0;
					xpad_cis.push_back(last_xpad_ci);
				}
				break;
			}
		}
	}

	// reset last CI
	last_xpad_ci.Reset();


	// process CIs
//	fprintf(stderr, "PADDecoder: -----\n");
	if(xpad_cis.empty())
		return;

	size_t xpad_offset = xpad_cis_len;
	for(xpad_cis_t::iterator it = xpad_cis.begin(); it != xpad_cis.end(); it++) {
		// abort, if Data Subfield out of X-PAD
		if(xpad_offset + it->len > xpad_len)
			return;

		// handle Data Subfield
		switch(it->type) {
		case 2:		// Dynamic Label segment
		case 3:
			// if new label available, append it
			if(dl_decoder.ProcessDataSubfield(it->type == 2, xpad_data + xpad_offset, it->len)) {
				{
					std::lock_guard<std::mutex> lock(data_mutex);

					dl_len = dl_decoder.GetLabel(dl_raw, &dl_charset);
				}
				observer->PADChangeDynamicLabel();
			}
			break;
		}
//		fprintf(stderr, "PADDecoder: Data Subfield: type: %2d, len: %2zu\n", it->type, it->len);

		xpad_offset += it->len;
	}

	// set last CI
	last_xpad_ci.len = xpad_offset;
	last_xpad_ci.type = XPAD_CI::GetContinuedLastCIType(xpad_cis.back().type);
}


// --- DynamicLabelDecoder -----------------------------------------------------------------
void DynamicLabelDecoder::Reset() {
	dl_dg_raw.clear();
	dl_segs.clear();

	label_len = 0;
	label_charset = -1;
}

size_t DynamicLabelDecoder::GetLabel(uint8_t *data, int *charset) {
	memcpy(data, label_raw, label_len);
	*charset = label_charset;
	return label_len;
}

bool DynamicLabelDecoder::ProcessDataSubfield(bool start, const uint8_t *data, size_t len) {
	if(start) {
		dl_dg_raw.clear();
	} else {
		// ignore Data Group continuation without previous start
		if(dl_dg_raw.empty())
			return false;
	}

	// append Data Subfield
	dl_dg_raw.resize(dl_dg_raw.size() + len);
	memcpy(&dl_dg_raw[dl_dg_raw.size() - len], data, len);

	// try to decode Data Group
	return DecodeDataGroup();
}

bool DynamicLabelDecoder::DecodeDataGroup() {
	// at least prefix + CRC
	if(dl_dg_raw.size() < 2 + CRC_LEN)
		return false;

	bool toggle = dl_dg_raw[0] & 0x80;
	bool first = dl_dg_raw[0] & 0x40;
	bool last = dl_dg_raw[0] & 0x20;
	bool command = dl_dg_raw[0] & 0x10;

	size_t field_len = 0;
	bool cmd_remove_label = false;

	// handle command/segment
	if(command) {
		switch(dl_dg_raw[0] & 0x0F) {
		case DL_CMD_REMOVE_LABEL:
			cmd_remove_label = true;
			break;
		default:
			// ignore command
			dl_dg_raw.clear();
			return false;
		}
	} else {
		field_len = (dl_dg_raw[0] & 0x0F) + 1;
	}

	size_t real_len = 2 + field_len + CRC_LEN;

	if(dl_dg_raw.size() < real_len)
		return false;

	// abort on invalid CRC
	uint16_t crc_stored = dl_dg_raw[real_len-2] << 8 | dl_dg_raw[real_len-1];
	uint16_t crc_calced = CalcCRC::CalcCRC_CRC16_CCITT.Calc(&dl_dg_raw[0], real_len - 2);
	if(crc_stored != crc_calced) {
		dl_dg_raw.clear();
		return false;
	}

	// on Remove Label command, display empty label
	if(cmd_remove_label) {
		label_len = 0;
		label_charset = 0;	// EBU Latin based (though it doesn't matter)
		return true;
	}

	// create new segment
	DL_SEG dl_seg;
	dl_seg.toggle = toggle;

	dl_seg.segnum = first ? 0 : ((dl_dg_raw[1] & 0x70) >> 4);
	dl_seg.last = last;

	dl_seg.charset = first ? (dl_dg_raw[1] >> 4) : -1;

	memcpy(dl_seg.chars, &dl_dg_raw[2], field_len);
	dl_seg.chars_len = field_len;

//	fprintf(stderr, "DynamicLabelDecoder: segnum %d, toggle: %s, chars_len: %2d%s\n", dl_seg.segnum, dl_seg.toggle ? "Y" : "N", dl_seg.chars_len, dl_seg.last ? " [LAST]" : "");

	// try to add segment
	return AddSegment(dl_seg);
}

bool DynamicLabelDecoder::AddSegment(DL_SEG &dl_seg) {
	dl_segs_t::iterator it;

	// if there are already segments with other toggle value in cache, first clear it
	it = dl_segs.begin();
	if(it != dl_segs.end() && it->second.toggle != dl_seg.toggle)
		dl_segs.clear();

	// if the segment is already there, abort
	it = dl_segs.find(dl_seg.segnum);
	if(it != dl_segs.end())
		return false;

	// add segment
	dl_segs[dl_seg.segnum] = dl_seg;

	// check for complete label
	return CheckForCompleteLabel();
}

bool DynamicLabelDecoder::CheckForCompleteLabel() {
	dl_segs_t::iterator it;

	// check if all segments are in cache
	int segs;
	size_t dl_len = 0;
	for(int i = 0; i < 8; i++) {
		it = dl_segs.find(i);
		if(it == dl_segs.end())
			return false;

		segs++;
		dl_len += it->second.chars_len;

		if(it->second.last)
			break;

		if(i == 7)
			return false;
	}

	// append complete label
	for(int i = 0; i < segs; i++)
		memcpy(label_raw + i * DL_SEG_MAX_LEN, dl_segs[i].chars, dl_segs[i].chars_len);
	label_len = dl_len;
	label_charset = dl_segs[0].charset;

//	std::string label((const char*) label_raw, label_len);
//	fprintf(stderr, "DynamicLabelDecoder: new label: '%s'\n", label.c_str());
	return true;
}
