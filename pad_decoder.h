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

#ifndef PAD_DECODER_H_
#define PAD_DECODER_H_

#include <cstdint>
#include <cstring>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "tools.h"

#define CRC_LEN 2
#define DL_SEG_MAX_LEN 16
#define DL_MAX_LEN 128
#define DL_CMD_REMOVE_LABEL 0x01



struct DL_SEG {
	bool toggle;
	int segnum;
	bool last;

	int charset;

	uint8_t chars[DL_SEG_MAX_LEN];
	size_t chars_len;
};


// --- DataGroup -----------------------------------------------------------------
class DataGroup {
protected:
	std::vector<uint8_t> dg_raw;
	size_t dg_size_needed;

	virtual bool DecodeDataGroup() = 0;
	bool EnsureDataGroupSize(size_t dg_size);
	void Reset();
public:
	DataGroup() {Reset();}
	virtual ~DataGroup() {}

	bool ProcessDataSubfield(bool start, const uint8_t *data, size_t len);
};


typedef std::map<int,DL_SEG> dl_segs_t;

// --- DynamicLabelDecoder -----------------------------------------------------------------
class DynamicLabelDecoder : public DataGroup {
private:
	dl_segs_t dl_segs;

	uint8_t label_raw[DL_MAX_LEN];
	size_t label_len;
	int label_charset;

	bool DecodeDataGroup();
	bool AddSegment(DL_SEG &dl_seg);
	bool CheckForCompleteLabel();
public:
	DynamicLabelDecoder() {Reset();}

	void Reset();

	size_t GetLabel(uint8_t *data, int *charset);
};



// --- XPAD_CI -----------------------------------------------------------------
struct XPAD_CI {
	size_t len;
	int type;

	static const size_t lens[];
	static int GetContinuedLastCIType(int last_ci_type);

	XPAD_CI() {Reset();}
	XPAD_CI(uint8_t ci_raw) {
		len = lens[ci_raw >> 5];
		type = ci_raw & 0x1F;
	}
	XPAD_CI(size_t len, int type) : len(len), type(type) {}

	void Reset() {
		len = 0;
		type = -1;
	}
};

typedef std::list<XPAD_CI> xpad_cis_t;


// --- PADDecoderObserver -----------------------------------------------------------------
class PADDecoderObserver {
public:
	virtual ~PADDecoderObserver() {};

	virtual void PADChangeDynamicLabel() {};
};


// --- PADDecoder -----------------------------------------------------------------
class PADDecoder {
private:
	PADDecoderObserver *observer;

	XPAD_CI last_xpad_ci;

	std::mutex data_mutex;
	uint8_t dl_raw[DL_MAX_LEN];
	size_t dl_len;
	int dl_charset;

	DynamicLabelDecoder dl_decoder;
public:
	PADDecoder(PADDecoderObserver *observer);

	void Process(const uint8_t *xpad_data, size_t xpad_len, uint16_t fpad);
	void Reset();

	size_t GetDynamicLabel(uint8_t *data, int *charset);
};

#endif /* PAD_DECODER_H_ */
