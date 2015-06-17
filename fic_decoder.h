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


struct AUDIO_SERVICE {
	int subchid;
	bool dab_plus;
};

struct FIC_LABEL {
	int charset;
	uint8_t label[16];
	uint16_t short_label_mask;

    bool operator==(const FIC_LABEL & fic_label) const {
        return charset == fic_label.charset && !memcmp(label, fic_label.label, sizeof(label)) && short_label_mask == fic_label.short_label_mask;
    }
    bool operator!=(const FIC_LABEL & fic_label) const {
    	return !(*this == fic_label);
    }
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
typedef std::set<SERVICE> services_t;

// --- FICDecoderObserver -----------------------------------------------------------------
class FICDecoderObserver {
public:
	virtual ~FICDecoderObserver() {};

	virtual void FICChangeEnsemble() {};
	virtual void FICChangeServices() {};
};


// --- FICDecoder -----------------------------------------------------------------
class FICDecoder {
private:
	FICDecoderObserver *observer;

	void ProcessFIB(const uint8_t *data);
	void ProcessFIG0(const uint8_t *data, size_t len);
	void ProcessFIG1(const uint8_t *data, size_t len);

	void CheckService(uint16_t sid);

	audio_services_t audio_services;
	labels_t labels;
	std::set<uint16_t> known_services;

	std::mutex data_mutex;
	uint16_t ensemble_id;
	FIC_LABEL *ensemble_label;
	services_t new_services;

	static const char* ebu_values_0x80[];
	static std::string ConvertCharEBUToUTF8(const uint8_t value, bool dynamic_label);
public:
	FICDecoder(FICDecoderObserver *observer);

	void Process(const uint8_t *data, size_t len);
	void Reset();

	void GetEnsembleData(uint16_t *id, FIC_LABEL *label);
	services_t GetNewServices();

	static std::string ConvertTextToUTF8(const uint8_t *data, size_t len, int charset, bool dynamic_label);
};



#endif /* FIC_DECODER_H_ */
