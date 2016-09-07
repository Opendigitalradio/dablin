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

#ifndef MOT_MANAGER_H_
#define MOT_MANAGER_H_

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "tools.h"


typedef std::vector<uint8_t> seg_t;
typedef std::map<int,seg_t> segs_t;

// --- MOTEntity -----------------------------------------------------------------
class MOTEntity {
private:
	segs_t segs;
	int last_seg_number;
	size_t size;
public:
	MOTEntity() : last_seg_number(-1), size(0) {}

	void AddSeg(int seg_number, bool last_seg, const uint8_t* data, size_t len);
	bool IsFinished();
	std::vector<uint8_t> GetData();
};


// --- MOTTransport -----------------------------------------------------------------
class MOTTransport {
private:
	MOTEntity header;
	MOTEntity body;
	bool shown;
public:
	MOTTransport(): shown(false) {}

	void AddSeg(bool dg_type_header, int seg_number, bool last_seg, const uint8_t* data, size_t len);
	bool IsToBeShown();
	std::vector<uint8_t> GetData() {return body.GetData();}
};


// --- MOTManager -----------------------------------------------------------------
class MOTManager {
private:
	MOTTransport transport;
	int current_transport_id;

	bool ParseCheckDataGroupHeader(const std::vector<uint8_t>& dg, size_t& offset, int& dg_type);
	bool ParseCheckSessionHeader(const std::vector<uint8_t>& dg, size_t& offset, bool& last_seg, int& seg_number, int& transport_id);
	bool ParseCheckSegmentationHeader(const std::vector<uint8_t>& dg, size_t& offset, size_t& seg_size);
public:
	MOTManager();

	void Reset();
	bool HandleMOTDataGroup(const std::vector<uint8_t>& dg);
	std::vector<uint8_t> GetSlide() {return transport.GetData();}
};

#endif /* MOT_MANAGER_H_ */
