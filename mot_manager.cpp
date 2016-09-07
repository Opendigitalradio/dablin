/*
    DABlin - capital DAB experience
    Copyright (C) 2016 Stefan Pöschel

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

#include "mot_manager.h"


// --- MOTEntity -----------------------------------------------------------------
void MOTEntity::AddSeg(int seg_number, bool last_seg, const uint8_t* data, size_t len) {
	if(last_seg)
		last_seg_number = seg_number;

	if(segs.find(seg_number) != segs.end())
		return;

	// copy data
	segs[seg_number] = seg_t(len);
	memcpy(&segs[seg_number][0], data, len);
	size += len;
}

bool MOTEntity::IsFinished() {
	if(last_seg_number == -1)
		return false;

	// check if all segments are available
	for(int i = 0; i <= last_seg_number; i++)
		if(segs.find(i) == segs.end())
			return false;
	return true;
}

std::vector<uint8_t> MOTEntity::GetData() {
	std::vector<uint8_t> result(size);
	size_t offset = 0;

	// concatenate all segments
	for(int i = 0; i <= last_seg_number; i++) {
		seg_t& seg = segs[i];
		memcpy(&result[offset], &seg[0], seg.size());
		offset += seg.size();
	}

	return result;
}


// --- MOTTransport -----------------------------------------------------------------
void MOTTransport::AddSeg(bool dg_type_header, int seg_number, bool last_seg, const uint8_t* data, size_t len) {
	(dg_type_header ? header : body).AddSeg(seg_number, last_seg, data, len);
}

bool MOTTransport::IsToBeShown() {
	// TODO: check MOT header
	if(!shown && header.IsFinished() && body.IsFinished()) {
		shown = true;
		return true;
	}
	return false;
}


// --- MOTManager -----------------------------------------------------------------
MOTManager::MOTManager() {
	Reset();
}

void MOTManager::Reset() {
	transport = MOTTransport();
	current_transport_id = -1;
}

bool MOTManager::ParseCheckDataGroupHeader(const std::vector<uint8_t>& dg, size_t& offset, int& dg_type) {
	// parse/check Data Group header
	if(dg.size() < (offset + 2))
		return false;

	bool extension_flag = dg[offset] & 0x80;
	bool crc_flag = dg[offset] & 0x40;
	bool segment_flag = dg[offset] & 0x20;
	bool user_access_flag = dg[offset] & 0x10;
	dg_type = dg[offset] & 0x0F;
	offset += 2 + (extension_flag ? 2 : 0);

	if(!crc_flag)
		return false;
	if(!segment_flag)
		return false;
	if(!user_access_flag)
		return false;
	if(dg_type != 3 && dg_type != 4)	// only accept MOT header/body
		return false;

	return true;
}

bool MOTManager::ParseCheckSessionHeader(const std::vector<uint8_t>& dg, size_t& offset, bool& last_seg, int& seg_number, int& transport_id) {
	// parse/check session header
	if(dg.size() < (offset + 3))
		return false;

	last_seg = dg[offset] & 0x80;
	seg_number = ((dg[offset] & 0x7F) << 8) | dg[offset + 1];
	bool transport_id_flag = dg[offset + 2] & 0x10;
	size_t len_indicator = dg[offset + 2] & 0x0F;
	offset += 3;

	if(!transport_id_flag)
		return false;
	if(len_indicator < 2)
		return false;

	// handle transport ID
	if(dg.size() < (offset + len_indicator))
		return false;

	transport_id = (dg[offset] << 8) | dg[offset + 1];
	offset += len_indicator;

	return true;
}

bool MOTManager::ParseCheckSegmentationHeader(const std::vector<uint8_t>& dg, size_t& offset, size_t& seg_size) {
	// parse/check segmentation header (MOT)
	if(dg.size() < (offset + 2))
		return false;

	seg_size = ((dg[offset] & 0x1F) << 8) | dg[offset + 1];
	offset += 2;

	// compare announced/actual segment size
	if(seg_size != dg.size() - offset - CalcCRC::CRCLen)
		return false;

	return true;
}

bool MOTManager::HandleMOTDataGroup(const std::vector<uint8_t>& dg) {
	size_t offset = 0;

	// parse/check headers
	int dg_type;
	bool last_seg;
	int seg_number;
	int transport_id;
	size_t seg_size;

	if(!ParseCheckDataGroupHeader(dg, offset, dg_type))
		return false;
	if(!ParseCheckSessionHeader(dg, offset, last_seg, seg_number, transport_id))
		return false;
	if(!ParseCheckSegmentationHeader(dg, offset, seg_size))
		return false;


	// add segment to transport (reset if necessary)
	if(current_transport_id != transport_id) {
		current_transport_id = transport_id;
		transport = MOTTransport();
	}
	transport.AddSeg(dg_type == 3, seg_number, last_seg, &dg[offset], seg_size);

	// check if slide shall be shown
	bool display = transport.IsToBeShown();
//	fprintf(stderr, "dg_type: %d, seg_number: %2d%s, transport_id: %5d, size: %4zu; display: %s\n",
//			dg_type, seg_number, last_seg ? " (LAST)" : "", transport_id, seg_size, display ? "true" : "false");

	// if slide shall be shown, update it
	return display;
}
