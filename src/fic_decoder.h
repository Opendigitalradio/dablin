/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2020 Stefan Pöschel

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
#include <string>
#include <map>
#include <set>
#include <vector>
#include <math.h>
#include <time.h>

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

struct FIC_ASW_CLUSTER {
	uint16_t asw_flags;
	int subchid;

	static const int asw_flags_none = 0x0000;

	static const int subchid_none = -1;
	bool IsNone() const {return subchid == subchid_none;}

	FIC_ASW_CLUSTER() : asw_flags(asw_flags_none), subchid(subchid_none) {}

	bool operator==(const FIC_ASW_CLUSTER & fic_asw_cluster) const {
		return asw_flags == fic_asw_cluster.asw_flags && subchid == fic_asw_cluster.subchid;
	}
	bool operator!=(const FIC_ASW_CLUSTER & fic_asw_cluster) const {
		return !(*this == fic_asw_cluster);
	}
};

typedef std::map<uint8_t,FIC_ASW_CLUSTER> asw_clusters_t;

struct FIC_DAB_DT {
	struct tm dt;
	int ms;

	static const int none = -1;
	bool IsNone() const {return dt.tm_year == none;}

	static const int ms_none = -1;
	bool IsMsNone() const {return ms == ms_none;}

	FIC_DAB_DT() : ms(ms_none) {
		dt.tm_year = none;
	}

	bool operator==(const FIC_DAB_DT & fic_dab_dt) const {
		return
				ms == fic_dab_dt.ms &&
				dt.tm_sec == fic_dab_dt.dt.tm_sec &&
				dt.tm_min == fic_dab_dt.dt.tm_min &&
				dt.tm_hour == fic_dab_dt.dt.tm_hour &&
				dt.tm_mday == fic_dab_dt.dt.tm_mday &&
				dt.tm_mon == fic_dab_dt.dt.tm_mon &&
				dt.tm_year == fic_dab_dt.dt.tm_year;
	}
	bool operator!=(const FIC_DAB_DT & fic_dab_dt) const {
		return !(*this == fic_dab_dt);
	}
};

struct FIC_ENSEMBLE {
	int eid;
	FIC_LABEL label;
	int ecc;
	int lto;
	int inter_table_id;
	asw_clusters_t asw_clusters;

	static const int eid_none = -1;
	bool IsNone() const {return eid == eid_none;}

	static const int ecc_none = -1;
	static const int lto_none = -100;
	static const int inter_table_id_none = -1;

	FIC_ENSEMBLE() :
		eid(eid_none),
		ecc(ecc_none),
		lto(lto_none),
		inter_table_id(inter_table_id_none)
	{}

	bool operator==(const FIC_ENSEMBLE & ensemble) const {
		return
				eid == ensemble.eid &&
				label == ensemble.label &&
				ecc == ensemble.ecc &&
				lto == ensemble.lto &&
				inter_table_id == ensemble.inter_table_id &&
				asw_clusters == ensemble.asw_clusters;
	}
	bool operator!=(const FIC_ENSEMBLE & ensemble) const {
		return !(*this == ensemble);
	}
};

typedef std::map<int,AUDIO_SERVICE> audio_comps_t;
typedef std::map<int,int> comp_defs_t;
typedef std::map<int,FIC_LABEL> comp_labels_t;
typedef std::vector<uint8_t> ua_data_t;
typedef std::map<int,ua_data_t> comp_sls_uas_t;
typedef std::set<uint8_t> cids_t;

struct FIC_SERVICE {
	int sid;
	int pri_comp_subchid;
	FIC_LABEL label;
	int pty_static;
	int pty_dynamic;
	uint16_t asu_flags;
	cids_t cids;

	// components
	audio_comps_t audio_comps;		// from FIG 0/2 : SubChId -> AUDIO_SERVICE
	comp_defs_t comp_defs;			// from FIG 0/8 : SCIdS -> SubChId
	comp_labels_t comp_labels;		// from FIG 1/4 : SCIdS -> FIC_LABEL
	comp_sls_uas_t comp_sls_uas;	// from FIG 0/13: SCIdS -> UA data

	static const int sid_none = -1;
	bool IsNone() const {return sid == sid_none;}

	static const int pri_comp_subchid_none = -1;
	bool HasNoPriCompSubchid() const {return pri_comp_subchid == pri_comp_subchid_none;}

	static const int pty_none = -1;

	static const int asu_flags_none = 0x0000;

	FIC_SERVICE() : sid(sid_none), pri_comp_subchid(pri_comp_subchid_none), pty_static(pty_none), pty_dynamic(pty_none), asu_flags(asu_flags_none) {}
};

struct LISTED_SERVICE {
	int sid;
	int scids;
	FIC_SUBCHANNEL subchannel;
	AUDIO_SERVICE audio_service;
	FIC_LABEL label;
	int pty_static;
	int pty_dynamic;
	int sls_app_type;
	uint16_t asu_flags;
	cids_t cids;

	int pri_comp_subchid;	// only used for sorting
	bool multi_comps;

	static const int sid_none = -1;
	bool IsNone() const {return sid == sid_none;}

	static const int scids_none = -1;
	bool IsPrimary() const {return scids == scids_none;}

	static const int pty_none = -1;

	static const int asu_flags_none = 0x0000;

	static const int sls_app_type_none = -1;
	bool HasSLS() const {return sls_app_type != sls_app_type_none;}

	LISTED_SERVICE() :
		sid(sid_none),
		scids(scids_none),
		pty_static(pty_none),
		pty_dynamic(pty_none),
		sls_app_type(sls_app_type_none),
		asu_flags(asu_flags_none),
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
	virtual ~FICDecoderObserver() {}

	virtual void FICChangeEnsemble(const FIC_ENSEMBLE& /*ensemble*/) {}
	virtual void FICChangeService(const LISTED_SERVICE& /*service*/) {}
	virtual void FICChangeUTCDateTime(const FIC_DAB_DT& /*utc_dt*/) {}

	virtual void FICDiscardedFIB() {}
};


// --- FICDecoder -----------------------------------------------------------------
class FICDecoder {
private:
	FICDecoderObserver *observer;
	bool disable_dyn_msgs;

	void ProcessFIB(const uint8_t *data);

	void ProcessFIG0(const uint8_t *data, size_t len);
	void ProcessFIG0_1(const uint8_t *data, size_t len);
	void ProcessFIG0_2(const uint8_t *data, size_t len);
	void ProcessFIG0_5(const uint8_t *data, size_t len);
	void ProcessFIG0_8(const uint8_t *data, size_t len);
	void ProcessFIG0_9(const uint8_t *data, size_t len);
	void ProcessFIG0_10(const uint8_t *data, size_t len);
	void ProcessFIG0_13(const uint8_t *data, size_t len);
	void ProcessFIG0_17(const uint8_t *data, size_t len);
	void ProcessFIG0_18(const uint8_t *data, size_t len);
	void ProcessFIG0_19(const uint8_t *data, size_t len);

	void ProcessFIG1(const uint8_t *data, size_t len);
	void ProcessFIG1_0(uint16_t eid, const FIC_LABEL& label);
	void ProcessFIG1_1(uint16_t sid, const FIC_LABEL& label);
	void ProcessFIG1_4(uint16_t sid, int scids, const FIC_LABEL& label);

	FIC_SUBCHANNEL& GetSubchannel(int subchid);
	void UpdateSubchannel(int subchid);
	FIC_SERVICE& GetService(uint16_t sid);
	void UpdateService(const FIC_SERVICE& service);
	void UpdateListedService(const FIC_SERVICE& service, int scids, bool multi_comps);
	int GetSLSAppType(const ua_data_t& ua_data);

	FIC_ENSEMBLE ensemble;
	void UpdateEnsemble();

	fic_services_t services;
	fic_subchannels_t subchannels;	// from FIG 0/1: SubChId -> FIC_SUBCHANNEL

	FIC_DAB_DT utc_dt;

	static const size_t uep_sizes[];
	static const int uep_pls[];
	static const int uep_bitrates[];
	static const int eep_a_size_factors[];
	static const int eep_b_size_factors[];

	static const char* languages_0x00_to_0x2B[];
	static const char* languages_0x7F_downto_0x45[];

	static const char* ptys_rds_0x00_to_0x1D[];
	static const char* ptys_rbds_0x00_to_0x1D[];

	static const char* asu_types_0_to_10[];
public:
	FICDecoder(FICDecoderObserver *observer, bool disable_dyn_msgs) :
		observer(observer),
		disable_dyn_msgs(disable_dyn_msgs)
	{}

	void Process(const uint8_t *data, size_t len);
	void Reset();

	static std::string ConvertLabelToUTF8(const FIC_LABEL& label, std::string* charset_name);
	static std::string ConvertLanguageToString(const int value);
	static std::string ConvertLTOToString(const int value);
	static std::string ConvertInterTableIDToString(const int value);
	static std::string ConvertDateTimeToString(FIC_DAB_DT utc_dt, const int lto, bool output_ms);
	static std::string ConvertPTYToString(const int value, const int inter_table_id);
	static std::string ConvertASuTypeToString(const int value);
	static std::string DeriveShortLabelUTF8(const std::string& long_label, uint16_t short_label_mask);
};



#endif /* FIC_DECODER_H_ */
