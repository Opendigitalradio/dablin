/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2017 Stefan Pöschel

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

#include <stdio.h>
#include <stdint.h>
#include <mutex>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <iconv.h>

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

struct FIC_LABEL {
	int charset;
	uint8_t label[16];
	uint16_t short_label_mask;

	static const int charset_none = -1;
	bool IsNone() const {return charset == charset_none;}

	FIC_LABEL() : charset(charset_none), short_label_mask(0x0000) {
		memset(label, 0x00, sizeof(label));
	}

	bool operator==(const FIC_LABEL & fic_label) const {
		return charset == fic_label.charset && !memcmp(label, fic_label.label, sizeof(label)) && short_label_mask == fic_label.short_label_mask;
	}
	bool operator!=(const FIC_LABEL & fic_label) const {
		return !(*this == fic_label);
	}
};

struct FIC_SUBCHANNEL {
	size_t start;
	size_t size;
	std::string pl;
	int bitrate;
	int language;

	static const int language_none = -1;
	bool IsNone() const {return pl.empty() && language == language_none;}

	FIC_SUBCHANNEL() : start(0), size(0), bitrate(-1), language(language_none) {}

	bool operator==(const FIC_SUBCHANNEL & fic_subchannel) const {
		return
				start == fic_subchannel.start &&
				size == fic_subchannel.size &&
				pl == fic_subchannel.pl &&
				bitrate == fic_subchannel.bitrate &&
				language == fic_subchannel.language;
	}
	bool operator!=(const FIC_SUBCHANNEL & fic_subchannel) const {
		return !(*this == fic_subchannel);
	}
};

struct FIC_ENSEMBLE {
	int eid;
	FIC_LABEL label;

	static const int eid_none = -1;
	bool IsNone() const {return eid == eid_none;}

	FIC_ENSEMBLE() : eid(eid_none) {}

	bool operator==(const FIC_ENSEMBLE & ensemble) const {
		return eid == ensemble.eid && label == ensemble.label;
	}
	bool operator!=(const FIC_ENSEMBLE & ensemble) const {
		return !(*this == ensemble);
	}
};

typedef std::map<int,AUDIO_SERVICE> sec_comps_t;
typedef std::map<int,int> comp_defs_t;
typedef std::map<int,FIC_LABEL> comp_labels_t;

struct FIC_SERVICE {
	int sid;

	// primary component
	AUDIO_SERVICE audio_service;
	FIC_LABEL label;

	// secondary components (if present)
	sec_comps_t sec_comps;		// from FIG 0/2: SubChId -> AUDIO_SERVICE
	comp_defs_t comp_defs;		// from FIG 0/8: SCIdS -> SubChId
	comp_labels_t comp_labels;	// from FIG 1/4: SCIdS -> FIC_LABEL

	static const int sid_none = -1;
	bool IsNone() const {return sid == sid_none;}

	FIC_SERVICE() : sid(sid_none) {}
};

struct LISTED_SERVICE {
	int sid;
	int scids;
	FIC_SUBCHANNEL subchannel;
	AUDIO_SERVICE audio_service;
	FIC_LABEL label;

	int pri_comp_subchid;	// only used for sorting
	bool multi_comps;

	static const int sid_none = -1;
	bool IsNone() const {return sid == sid_none;}

	static const int scids_none = -1;
	bool IsPrimary() const {return scids == scids_none;}

	LISTED_SERVICE() :
		sid(sid_none),
		scids(scids_none),
		pri_comp_subchid(AUDIO_SERVICE::subchid_none),
		multi_comps(false)
	{}

	bool operator<(const LISTED_SERVICE & service) const {
		if(pri_comp_subchid != service.pri_comp_subchid)
			return pri_comp_subchid < service.pri_comp_subchid;
		if(sid != service.sid)
			return sid < service.sid;
		return scids < service.scids;
	}
};

typedef std::map<uint16_t, FIC_SERVICE> fic_services_t;
typedef std::map<int, FIC_SUBCHANNEL> fic_subchannels_t;

// --- FICDecoderObserver -----------------------------------------------------------------
class FICDecoderObserver {
public:
	virtual ~FICDecoderObserver() {};

	virtual void FICChangeEnsemble(const FIC_ENSEMBLE& /*ensemble*/) {};
	virtual void FICChangeService(const LISTED_SERVICE& /*service*/) {};
};


// --- FICDecoder -----------------------------------------------------------------
class FICDecoder {
private:
	FICDecoderObserver *observer;

	void ProcessFIB(const uint8_t *data);

	void ProcessFIG0(const uint8_t *data, size_t len);
	void ProcessFIG0_1(const uint8_t *data, size_t len);
	void ProcessFIG0_2(const uint8_t *data, size_t len);
	void ProcessFIG0_5(const uint8_t *data, size_t len);
	void ProcessFIG0_8(const uint8_t *data, size_t len);

	void ProcessFIG1(const uint8_t *data, size_t len);
	void ProcessFIG1_0(uint16_t eid, const FIC_LABEL& label);
	void ProcessFIG1_1(uint16_t sid, const FIC_LABEL& label);
	void ProcessFIG1_4(uint16_t sid, int scids, const FIC_LABEL& label);

	FIC_SUBCHANNEL& GetSubchannel(int subchid);
	void UpdateSubchannel(int subchid);
	FIC_SERVICE& GetService(uint16_t sid);
	void UpdateService(const FIC_SERVICE& service);
	void UpdateListedService(const FIC_SERVICE& service, int scids, bool multi_comps);

	FIC_ENSEMBLE ensemble;
	fic_services_t services;
	fic_subchannels_t subchannels;	// from FIG 0/1: SubChId -> FIC_SUBCHANNEL

	static const char* no_char;
	static const char* ebu_values_0x00_to_0x1F[];
	static const char* ebu_values_0x7B_to_0xFF[];
	static std::string ConvertCharEBUToUTF8(const uint8_t value);
	static std::string ConvertStringIconvToUTF8(const std::vector<uint8_t>& cleaned_data, std::string* charset_name, const std::string& src_charset);

	static const size_t uep_sizes[];
	static const int uep_pls[];
	static const int uep_bitrates[];
	static const int eep_a_size_factors[];
	static const int eep_b_size_factors[];

	static const char* languages_0x00_to_0x2B[];
	static const char* languages_0x7F_downto_0x45[];
public:
	FICDecoder(FICDecoderObserver *observer) : observer(observer) {}

	void Process(const uint8_t *data, size_t len);
	void Reset();

	static std::string ConvertTextToUTF8(const uint8_t *data, size_t len, int charset, bool mot, std::string* charset_name);
	static std::string ConvertLabelToUTF8(const FIC_LABEL& label);
	static std::string ConvertLanguageToString(const int value);
};



#endif /* FIC_DECODER_H_ */
