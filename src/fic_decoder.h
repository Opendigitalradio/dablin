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

#ifndef FIC_DECODER_H_
#define FIC_DECODER_H_

#include <cstdio>
#include <cstdint>
#include <mutex>
#include <string>
#include <map>
#include <set>

#include "tools.h"


struct FIG0_HEADER {
	bool cn;
	bool oe;
	bool pd;
	int extension;

	FIG0_HEADER(uint8_t data) : cn(data & 0x80), oe(data & 0x40), pd(data & 0x20), extension(data & 0x1F) {}
};

struct FIG1_HEADER {
	int charset;
	bool oe;
	int extension;

	FIG1_HEADER(uint8_t data) : charset(data >> 4), oe(data & 0x08), extension(data & 0x07) {}
};

struct AUDIO_SERVICE {
	int subchid;
	bool dab_plus;
};

struct FIC_LABEL {
	int charset;
	uint8_t label[16];
	uint16_t short_label_mask;

	FIC_LABEL() : charset(-1), short_label_mask(0x0000) {}

    bool operator==(const FIC_LABEL & fic_label) const {
        return charset == fic_label.charset && !memcmp(label, fic_label.label, sizeof(label)) && short_label_mask == fic_label.short_label_mask;
    }
    bool operator!=(const FIC_LABEL & fic_label) const {
    	return !(*this == fic_label);
    }
};

struct ENSEMBLE {
	uint16_t eid;
	FIC_LABEL label;

	ENSEMBLE() : eid(0) {}
};

struct SERVICE {
	int sid;
	AUDIO_SERVICE service;
	FIC_LABEL label;

	static const SERVICE no_service;

	SERVICE() : sid(0) {}
	SERVICE(int sid) : sid(sid) {}

	bool operator<(const SERVICE & complete_service) const {
		return sid < complete_service.sid;
	}
};

typedef std::map<uint16_t, AUDIO_SERVICE> audio_services_t;
typedef std::map<uint16_t, FIC_LABEL> labels_t;

// --- FICDecoderObserver -----------------------------------------------------------------
class FICDecoderObserver {
public:
	virtual ~FICDecoderObserver() {};

	virtual void FICChangeEnsemble(const ENSEMBLE& ensemble) {};
	virtual void FICChangeService(const SERVICE& service) {};
};


// --- FICDecoder -----------------------------------------------------------------
class FICDecoder {
private:
	FICDecoderObserver *observer;

	void ProcessFIB(const uint8_t *data);

	void ProcessFIG0(const uint8_t *data, size_t len);
	void ProcessFIG0_2(const uint8_t *data, size_t len, const FIG0_HEADER& header);

	void ProcessFIG1(const uint8_t *data, size_t len);
	void ProcessFIG1_0(uint16_t id, const FIC_LABEL& label);
	void ProcessFIG1_1(uint16_t id, const FIC_LABEL& label);

	void CheckService(uint16_t sid);

	ENSEMBLE ensemble;

	audio_services_t audio_services;
	labels_t labels;
	std::set<uint16_t> known_services;

	static const char* no_char;
	static const char* ebu_values_0x00_to_0x1F[];
	static const char* ebu_values_0x7B_to_0xFF[];
	static std::string ConvertCharEBUToUTF8(const uint8_t value);
public:
	FICDecoder(FICDecoderObserver *observer);

	void Process(const uint8_t *data, size_t len);
	void Reset();

	static std::string ConvertTextToUTF8(const uint8_t *data, size_t len, int charset);
	static std::string ConvertLabelToUTF8(const FIC_LABEL& label);
};



#endif /* FIC_DECODER_H_ */
