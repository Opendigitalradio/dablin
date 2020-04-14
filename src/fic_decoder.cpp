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

#include "fic_decoder.h"


// --- FICDecoder -----------------------------------------------------------------
void FICDecoder::Reset() {
	ensemble = FIC_ENSEMBLE();
	services.clear();
	subchannels.clear();
	utc_dt = FIC_DAB_DT();
}

void FICDecoder::Process(const uint8_t *data, size_t len) {
	// check for integer FIB count
	if(len % 32) {
		fprintf(stderr, "FICDecoder: Ignoring non-integer FIB count FIC data with %zu bytes\n", len);
		return;
	}

	for(size_t i = 0; i < len; i += 32)
		ProcessFIB(data + i);
}


void FICDecoder::ProcessFIB(const uint8_t *data) {
	// check CRC
	uint16_t crc_stored = data[30] << 8 | data[31];
	uint16_t crc_calced = CalcCRC::CalcCRC_CRC16_CCITT.Calc(data, 30);
	if(crc_stored != crc_calced) {
		observer->FICDiscardedFIB();
		return;
	}

	// iterate over all FIGs
	for(size_t offset = 0; offset < 30 && data[offset] != 0xFF;) {
		int type = data[offset] >> 5;
		size_t len = data[offset] & 0x1F;
		offset++;

		switch(type) {
		case 0:
			ProcessFIG0(data + offset, len);
			break;
		case 1:
			ProcessFIG1(data + offset, len);
			break;
//		default:
//			fprintf(stderr, "FICDecoder: received unsupported FIG %d with %zu bytes\n", type, len);
		}
		offset += len;
	}
}


void FICDecoder::ProcessFIG0(const uint8_t *data, size_t len) {
	if(len < 1) {
		fprintf(stderr, "FICDecoder: received empty FIG 0\n");
		return;
	}

	// read/skip FIG 0 header
	FIG0_HEADER header(data[0]);
	data++;
	len--;

	// ignore next config/other ensembles/data services
	if(header.cn || header.oe || header.pd)
		return;


	// handle extension
	switch(header.extension) {
	case 1:
		ProcessFIG0_1(data, len);
		break;
	case 2:
		ProcessFIG0_2(data, len);
		break;
	case 5:
		ProcessFIG0_5(data, len);
		break;
	case 8:
		ProcessFIG0_8(data, len);
		break;
	case 9:
		ProcessFIG0_9(data, len);
		break;
	case 10:
		ProcessFIG0_10(data, len);
		break;
	case 13:
		ProcessFIG0_13(data, len);
		break;
	case 17:
		ProcessFIG0_17(data, len);
		break;
	case 18:
		ProcessFIG0_18(data, len);
		break;
	case 19:
		ProcessFIG0_19(data, len);
		break;
//	default:
//		fprintf(stderr, "FICDecoder: received unsupported FIG 0/%d with %zu field bytes\n", header.extension, len);
	}
}

void FICDecoder::ProcessFIG0_1(const uint8_t *data, size_t len) {
	// FIG 0/1 - Basic sub-channel organization

	// iterate through all sub-channels
	for(size_t offset = 0; offset < len;) {
		int subchid = data[offset] >> 2;
		size_t start_address = (data[offset] & 0x03) << 8 | data[offset + 1];
		offset += 2;

		FIC_SUBCHANNEL sc;
		sc.start = start_address;

		bool short_long_form = data[offset] & 0x80;
		if(short_long_form) {
			// long form
			int option = (data[offset] & 0x70) >> 4;
			int pl = (data[offset] & 0x0C) >> 2;
			size_t subch_size = (data[offset] & 0x03) << 8 | data[offset + 1];

			switch(option) {
			case 0b000:
				sc.size = subch_size;
				sc.pl = "EEP " + std::to_string(pl + 1) + "-A";
				sc.bitrate = subch_size / eep_a_size_factors[pl] *  8;
				break;
			case 0b001:
				sc.size = subch_size;
				sc.pl = "EEP " + std::to_string(pl + 1) + "-B";
				sc.bitrate = subch_size / eep_b_size_factors[pl] * 32;
				break;
			}
			offset += 2;
		} else {
			// short form

			bool table_switch = data[offset] & 0x40;
			if(!table_switch) {
				int table_index = data[offset] & 0x3F;
				sc.size = uep_sizes[table_index];
				sc.pl = "UEP " + std::to_string(uep_pls[table_index]);
				sc.bitrate = uep_bitrates[table_index];
			}
			offset++;
		}

		if(!sc.IsNone()) {
			FIC_SUBCHANNEL& current_sc = GetSubchannel(subchid);
			sc.language = current_sc.language;	// ignored for comparison
			if(current_sc != sc) {
				current_sc = sc;

				fprintf(stderr, "FICDecoder: SubChId %2d: start %3zu CUs, size %3zu CUs, PL %-7s = %3d kBit/s\n", subchid, sc.start, sc.size, sc.pl.c_str(), sc.bitrate);

				UpdateSubchannel(subchid);
			}
		}
	}
}

void FICDecoder::ProcessFIG0_2(const uint8_t *data, size_t len) {
	// FIG 0/2 - Basic service and service component definition
	// programme services only

	// iterate through all services
	for(size_t offset = 0; offset < len;) {
		uint16_t sid = data[offset] << 8 | data[offset + 1];
		offset += 2;

		size_t num_service_comps = data[offset++] & 0x0F;

		// iterate through all service components
		for(size_t comp = 0; comp < num_service_comps; comp++) {
			int tmid = data[offset] >> 6;

			switch(tmid) {
			case 0b00:	// MSC stream audio
				int ascty = data[offset] & 0x3F;
				int subchid = data[offset + 1] >> 2;
				bool ps = data[offset + 1] & 0x02;
				bool ca = data[offset + 1] & 0x01;

				if(!ca) {
					switch(ascty) {
					case 0:		// DAB
					case 63:	// DAB+
						bool dab_plus = ascty == 63;

						AUDIO_SERVICE audio_service(subchid, dab_plus);

						FIC_SERVICE& service = GetService(sid);
						AUDIO_SERVICE& current_audio_service = service.audio_comps[subchid];
						if(current_audio_service != audio_service || ps != (service.pri_comp_subchid == subchid)) {
							current_audio_service = audio_service;
							if(ps)
								service.pri_comp_subchid = subchid;

							fprintf(stderr, "FICDecoder: SId 0x%04X: audio service (SubChId %2d, %-4s, %s)\n", sid, subchid, dab_plus ? "DAB+" : "DAB", ps ? "primary" : "secondary");

							UpdateService(service);
						}

						break;
					}
				}
			}

			offset += 2;
		}
	}
}

void FICDecoder::ProcessFIG0_5(const uint8_t *data, size_t len) {
	// FIG 0/5 - Service component language
	// programme services only

	// iterate through all components
	for(size_t offset = 0; offset < len;) {
		bool ls_flag = data[offset] & 0x80;
		if(ls_flag) {
			// long form - skipped, as not relevant
			offset += 3;
		} else {
			// short form
			bool msc_fic_flag = data[offset] & 0x40;

			// handle only MSC components
			if(!msc_fic_flag) {
				int subchid = data[offset] & 0x3F;
				int language = data[offset + 1];

				FIC_SUBCHANNEL& current_sc = GetSubchannel(subchid);
				if(current_sc.language != language) {
					current_sc.language = language;

					fprintf(stderr, "FICDecoder: SubChId %2d: language '%s'\n", subchid, ConvertLanguageToString(language).c_str());

					UpdateSubchannel(subchid);
				}
			}

			offset += 2;
		}
	}
}

void FICDecoder::ProcessFIG0_8(const uint8_t *data, size_t len) {
	// FIG 0/8 - Service component global definition
	// programme services only

	// iterate through all service components
	for(size_t offset = 0; offset < len;) {
		uint16_t sid = data[offset] << 8 | data[offset + 1];
		offset += 2;

		bool ext_flag = data[offset] & 0x80;
		int scids = data[offset] & 0x0F;
		offset++;

		bool ls_flag = data[offset] & 0x80;
		if(ls_flag) {
			// long form - skipped, as not relevant
			offset += 2;
		} else {
			// short form
			bool msc_fic_flag = data[offset] & 0x40;

			// handle only MSC components
			if(!msc_fic_flag) {
				int subchid = data[offset] & 0x3F;

				FIC_SERVICE& service = GetService(sid);
				bool new_comp = service.comp_defs.find(scids) == service.comp_defs.end();
				int& current_subchid = service.comp_defs[scids];
				if(new_comp || current_subchid != subchid) {
					current_subchid = subchid;

					fprintf(stderr, "FICDecoder: SId 0x%04X, SCIdS %2d: MSC service component (SubChId %2d)\n", sid, scids, subchid);

					UpdateService(service);
				}
			}

			offset++;
		}

		// skip Rfa field, if needed
		if(ext_flag)
			offset++;
	}
}

void FICDecoder::ProcessFIG0_9(const uint8_t *data, size_t len) {
	// FIG 0/9 - Time and country identifier - Country, LTO and International table
	// ensemble ECC/LTO and international table ID only

	if(len < 3)
		return;

	FIC_ENSEMBLE new_ensemble = ensemble;
	new_ensemble.lto = (data[0] & 0x20 ? -1 : 1) * (data[0] & 0x1F);
	new_ensemble.ecc = data[1];
	new_ensemble.inter_table_id = data[2];

	if(ensemble != new_ensemble) {
		ensemble = new_ensemble;

		fprintf(stderr, "FICDecoder: ECC: 0x%02X, LTO: %s, international table ID: 0x%02X (%s)\n",
				ensemble.ecc, ConvertLTOToString(ensemble.lto).c_str(), ensemble.inter_table_id, ConvertInterTableIDToString(ensemble.inter_table_id).c_str());

		UpdateEnsemble();

		// update services that changes may affect
		for(const fic_services_t::value_type& service : services) {
			const FIC_SERVICE& s = service.second;
			if(s.pty_static != FIC_SERVICE::pty_none || s.pty_dynamic != FIC_SERVICE::pty_none)
				UpdateService(s);
		}
	}
}

void FICDecoder::ProcessFIG0_10(const uint8_t *data, size_t len) {
	// FIG 0/10 - Date and time (d&t)

	if(len < 4)
		return;

	FIC_DAB_DT new_utc_dt;

	// retrieve date
	int mjd = (data[0] & 0x7F) << 10 | data[1] << 2 | data[2] >> 6;

	int y0 = floor((mjd - 15078.2) / 365.25);
	int m0 = floor((mjd - 14956.1 - floor(y0 * 365.25)) / 30.6001);
	int d = mjd - 14956 - floor(y0 * 365.25) - floor(m0 * 30.6001);
	int k = (m0 == 14 || m0 == 15) ? 1 : 0;
	int y = y0 + k;
	int m = m0 - 1 - k * 12;

	new_utc_dt.dt.tm_year = y;		// from 1900
	new_utc_dt.dt.tm_mon = m - 1;	// 0-based
	new_utc_dt.dt.tm_mday = d;

	// retrieve time
	bool utc_flag = data[2] & 0x08;
	new_utc_dt.dt.tm_hour = (data[2] & 0x07) << 2 | data[3] >> 6;
	new_utc_dt.dt.tm_min = data[3] & 0x3F;
	new_utc_dt.dt.tm_isdst = -1;	// ignore DST
	if(utc_flag) {
		// long form
		if(len < 6)
			return;
		new_utc_dt.dt.tm_sec = data[4] >> 2;
		new_utc_dt.ms = (data[4] & 0x03) << 8 | data[5];
	} else {
		// short form
		new_utc_dt.dt.tm_sec = 0;
		new_utc_dt.ms = FIC_DAB_DT::ms_none;
	}

	if(utc_dt != new_utc_dt) {
		// print only once
		if(utc_dt.IsNone())
			fprintf(stderr, "FICDecoder: UTC date/time: %s\n", ConvertDateTimeToString(new_utc_dt, 0, true).c_str());

		utc_dt = new_utc_dt;

		observer->FICChangeUTCDateTime(utc_dt);
	}
}

void FICDecoder::ProcessFIG0_13(const uint8_t *data, size_t len) {
	// FIG 0/13 - User application information
	// programme services only

	// iterate through all service components
	for(size_t offset = 0; offset < len;) {
		uint16_t sid = data[offset] << 8 | data[offset + 1];
		offset += 2;

		int scids = data[offset] >> 4;
		size_t num_scids_uas = data[offset] & 0x0F;
		offset++;

		// iterate through all user applications
		for(size_t scids_ua = 0; scids_ua < num_scids_uas; scids_ua++) {
			int ua_type = data[offset] << 3 | data[offset + 1] >> 5;
			size_t ua_data_length = data[offset + 1] & 0x1F;
			offset += 2;

			// handle only Slideshow
			if(ua_type == 0x002) {
				FIC_SERVICE& service = GetService(sid);
				if(service.comp_sls_uas.find(scids) == service.comp_sls_uas.end()) {
					ua_data_t& sls_ua_data = service.comp_sls_uas[scids];

					sls_ua_data.resize(ua_data_length);
					if(ua_data_length)
						memcpy(&sls_ua_data[0], data + offset, ua_data_length);

					fprintf(stderr, "FICDecoder: SId 0x%04X, SCIdS %2d: Slideshow (%zu bytes UA data)\n", sid, scids, ua_data_length);

					UpdateService(service);
				}
			}

			offset += ua_data_length;
		}
	}
}

void FICDecoder::ProcessFIG0_17(const uint8_t *data, size_t len) {
	// FIG 0/17 - Programme Type
	// programme type only

	// iterate through all services
	for(size_t offset = 0; offset < len;) {
		uint16_t sid = data[offset] << 8 | data[offset + 1];
		bool sd = data[offset + 2] & 0x80;
		bool l_flag = data[offset + 2] & 0x20;
		bool cc_flag = data[offset + 2] & 0x10;
		offset += 3;

		// skip language, if present
		if(l_flag)
			offset++;

		// programme type (international code)
		int pty = data[offset] & 0x1F;
		offset++;

		// skip CC part, if present
		if(cc_flag)
			offset++;

		FIC_SERVICE& service = GetService(sid);
		int& current_pty = sd ? service.pty_dynamic : service.pty_static;
		if(current_pty != pty) {
			// suppress message, if dynamic FIC messages disabled and dynamic PTY not initally be set
			bool show_msg = !(disable_dyn_msgs && sd && current_pty != FIC_SERVICE::pty_none);

			current_pty = pty;

			if(show_msg) {
				// assuming international table ID 0x01 here!
				fprintf(stderr, "FICDecoder: SId 0x%04X: programme type (%s): '%s'\n",
						sid, sd ? "dynamic" : "static", ConvertPTYToString(pty, 0x01).c_str());
			}

			UpdateService(service);
		}
	}
}

void FICDecoder::ProcessFIG0_18(const uint8_t *data, size_t len) {
	// FIG 0/18 - Announcement support

	// iterate through all services
	for(size_t offset = 0; offset < len;) {
		uint16_t sid = data[offset] << 8 | data[offset + 1];
		uint16_t asu_flags = data[offset + 2] << 8 | data[offset + 3];
		size_t number_of_clusters = data[offset + 4] & 0x1F;
		offset += 5;

		cids_t cids;
		for(size_t i = 0; i < number_of_clusters; i++)
			cids.emplace(data[offset++]);

		FIC_SERVICE& service = GetService(sid);
		uint16_t& current_asu_flags = service.asu_flags;
		cids_t& current_cids = service.cids;
		if(current_asu_flags != asu_flags || current_cids != cids) {
			current_asu_flags = asu_flags;
			current_cids = cids;

			std::string cids_str;
			char cid_string[5];
			for(const cids_t::value_type& cid : cids) {
				if(!cids_str.empty())
					cids_str += "/";
				snprintf(cid_string, sizeof(cid_string), "0x%02X", cid);
				cids_str += std::string(cid_string);
			}

			fprintf(stderr, "FICDecoder: SId 0x%04X: ASu flags 0x%04X, cluster(s) %s\n",
					sid, asu_flags, cids_str.c_str());

			UpdateService(service);
		}
	}
}

void FICDecoder::ProcessFIG0_19(const uint8_t *data, size_t len) {
	// FIG 0/19 - Announcement switching

	// iterate through all announcement clusters
	for(size_t offset = 0; offset < len;) {
		uint8_t cid = data[offset];
		uint16_t asw_flags = data[offset + 1] << 8 | data[offset + 2];
		bool region_flag = data[offset + 3] & 0x40;
		int subchid = data[offset + 3] & 0x3F;
		offset += region_flag ? 5 : 4;

		FIC_ASW_CLUSTER ac;
		ac.asw_flags = asw_flags;
		ac.subchid = subchid;

		FIC_ASW_CLUSTER& current_ac = ensemble.asw_clusters[cid];
		if(current_ac != ac) {
			current_ac = ac;

			if(!disable_dyn_msgs) {
				fprintf(stderr, "FICDecoder: ASw cluster 0x%02X: flags 0x%04X, SubChId %2d\n",
						cid, asw_flags, subchid);
			}

			UpdateEnsemble();

			// update services that changes may affect
			for(const fic_services_t::value_type& service : services) {
				const FIC_SERVICE& s = service.second;
				if(s.cids.find(cid) != s.cids.cend())
					UpdateService(s);
			}
		}
	}
}

void FICDecoder::ProcessFIG1(const uint8_t *data, size_t len) {
	if(len < 1) {
		fprintf(stderr, "FICDecoder: received empty FIG 1\n");
		return;
	}

	// read/skip FIG 1 header
	FIG1_HEADER header(data[0]);
	data++;
	len--;

	// ignore other ensembles
	if(header.oe)
		return;

	// check for (un)supported extension + set ID field len
	size_t len_id = -1;
	switch(header.extension) {
	case 0:	// ensemble
	case 1:	// programme service
		len_id = 2;
		break;
	case 4:	// service component
		// programme services only (P/D = 0)
		if(data[0] & 0x80)
			return;
		len_id = 3;
		break;
	default:
//		fprintf(stderr, "FICDecoder: received unsupported FIG 1/%d with %zu field bytes\n", header.extension, len);
		return;
	}

	// check length
	size_t len_calced = len_id + 16 + 2;
	if(len != len_calced) {
		fprintf(stderr, "FICDecoder: received FIG 1/%d having %zu field bytes (expected: %zu)\n", header.extension, len, len_calced);
		return;
	}

	// parse actual label data
	FIC_LABEL label;
	label.charset = header.charset;
	memcpy(label.label, data + len_id, 16);
	label.short_label_mask = data[len_id + 16] << 8 | data[len_id + 17];


	// handle extension
	switch(header.extension) {
	case 0: {	// ensemble
		uint16_t eid = data[0] << 8 | data[1];
		ProcessFIG1_0(eid, label);
		break; }
	case 1: {	// programme service
		uint16_t sid = data[0] << 8 | data[1];
		ProcessFIG1_1(sid, label);
		break; }
	case 4: {	// service component
		int scids = data[0] & 0x0F;
		uint16_t sid = data[1] << 8 | data[2];
		ProcessFIG1_4(sid, scids, label);
		break; }
	}
}

void FICDecoder::ProcessFIG1_0(uint16_t eid, const FIC_LABEL& label) {
	if(ensemble.eid != eid || ensemble.label != label) {
		ensemble.eid = eid;
		ensemble.label = label;

		std::string label_str = ConvertLabelToUTF8(label, nullptr);
		std::string short_label_str = DeriveShortLabelUTF8(label_str, label.short_label_mask);
		fprintf(stderr, "FICDecoder: EId 0x%04X: ensemble label '" "\x1B[32m" "%s" "\x1B[0m" "' ('" "\x1B[32m" "%s" "\x1B[0m" "')\n",
				eid, label_str.c_str(), short_label_str.c_str());

		UpdateEnsemble();
	}
}

void FICDecoder::ProcessFIG1_1(uint16_t sid, const FIC_LABEL& label) {
	FIC_SERVICE& service = GetService(sid);
	if(service.label != label) {
		service.label = label;

		std::string label_str = ConvertLabelToUTF8(label, nullptr);
		std::string short_label_str = DeriveShortLabelUTF8(label_str, label.short_label_mask);
		fprintf(stderr, "FICDecoder: SId 0x%04X: programme service label '" "\x1B[32m" "%s" "\x1B[0m" "' ('" "\x1B[32m" "%s" "\x1B[0m" "')\n",
				sid, label_str.c_str(), short_label_str.c_str());

		UpdateService(service);
	}
}

void FICDecoder::ProcessFIG1_4(uint16_t sid, int scids, const FIC_LABEL& label) {
	// programme services only

	FIC_SERVICE& service = GetService(sid);
	FIC_LABEL& comp_label = service.comp_labels[scids];
	if(comp_label != label) {
		comp_label = label;

		std::string label_str = ConvertLabelToUTF8(label, nullptr);
		std::string short_label_str = DeriveShortLabelUTF8(label_str, label.short_label_mask);
		fprintf(stderr, "FICDecoder: SId 0x%04X, SCIdS %2d: service component label '" "\x1B[32m" "%s" "\x1B[0m" "' ('" "\x1B[32m" "%s" "\x1B[0m" "')\n",
				sid, scids, label_str.c_str(), short_label_str.c_str());

		UpdateService(service);
	}
}

FIC_SUBCHANNEL& FICDecoder::GetSubchannel(int subchid) {
	// created automatically, if not yet existing
	return subchannels[subchid];
}

void FICDecoder::UpdateSubchannel(int subchid) {
	// update services that consist of this sub-channel
	for(const fic_services_t::value_type& service : services) {
		const FIC_SERVICE& s = service.second;
		if(s.audio_comps.find(subchid) != s.audio_comps.end())
			UpdateService(s);
	}
}

FIC_SERVICE& FICDecoder::GetService(uint16_t sid) {
	FIC_SERVICE& result = services[sid];	// created, if not yet existing

	// if new service, set SID
	if(result.IsNone())
		result.sid = sid;
	return result;
}

void FICDecoder::UpdateService(const FIC_SERVICE& service) {
	// abort update, if primary component or label not yet present
	if(service.HasNoPriCompSubchid() || service.label.IsNone())
		return;

	// secondary components (if both component and definition are present)
	bool multi_comps = false;
	for(const comp_defs_t::value_type& comp_def : service.comp_defs) {
		if(comp_def.second == service.pri_comp_subchid || service.audio_comps.find(comp_def.second) == service.audio_comps.end())
			continue;
		UpdateListedService(service, comp_def.first, true);
		multi_comps = true;
	}

	// primary component
	UpdateListedService(service, LISTED_SERVICE::scids_none, multi_comps);
}

void FICDecoder::UpdateListedService(const FIC_SERVICE& service, int scids, bool multi_comps) {
	// assemble listed service
	LISTED_SERVICE ls;
	ls.sid = service.sid;
	ls.scids = scids;
	ls.label = service.label;
	ls.pty_static = service.pty_static;
	ls.pty_dynamic = service.pty_dynamic;
	ls.asu_flags = service.asu_flags;
	ls.cids = service.cids;
	ls.pri_comp_subchid = service.pri_comp_subchid;
	ls.multi_comps = multi_comps;

	if(scids == LISTED_SERVICE::scids_none) {	// primary component
		ls.audio_service = service.audio_comps.at(service.pri_comp_subchid);
	} else {									// secondary component
		ls.audio_service = service.audio_comps.at(service.comp_defs.at(scids));

		// use component label, if available
		comp_labels_t::const_iterator cl_it = service.comp_labels.find(scids);
		if(cl_it != service.comp_labels.end())
			ls.label = cl_it->second;
	}

	// use sub-channel information, if available
	fic_subchannels_t::const_iterator sc_it = subchannels.find(ls.audio_service.subchid);
	if(sc_it != subchannels.end())
		ls.subchannel = sc_it->second;

	/* check (for) Slideshow; currently only supported in X-PAD
	 * - derive the required SCIdS (if not yet known)
	 * - derive app type from UA data (if present)
	 */
	int sls_scids = scids;
	if(sls_scids == LISTED_SERVICE::scids_none) {
		for(const comp_defs_t::value_type& comp_def : service.comp_defs) {
			if(comp_def.second == ls.audio_service.subchid) {
				sls_scids = comp_def.first;
				break;
			}
		}
	}
	if(sls_scids != LISTED_SERVICE::scids_none && service.comp_sls_uas.find(sls_scids) != service.comp_sls_uas.end())
		ls.sls_app_type = GetSLSAppType(service.comp_sls_uas.at(sls_scids));

	// forward to observer
	observer->FICChangeService(ls);
}

int FICDecoder::GetSLSAppType(const ua_data_t& ua_data) {
	// default values, if no UA data present
	bool ca_flag = false;
	int xpad_app_type = 12;
	bool dg_flag = false;
	int dscty = 60;	// MOT

	// if UA data present, parse X-PAD data
	if(ua_data.size() >= 2) {
		ca_flag = ua_data[0] & 0x80;
		xpad_app_type = ua_data[0] & 0x1F;
		dg_flag = ua_data[1] & 0x80;
		dscty = ua_data[1] & 0x3F;
	}

	// if no CA is used, but DGs and MOT, enable Slideshow
	if(!ca_flag && !dg_flag && dscty == 60)
		return xpad_app_type;
	else
		return LISTED_SERVICE::sls_app_type_none;
}

void FICDecoder::UpdateEnsemble() {
	// abort update, if label not yet present
	if(ensemble.label.IsNone())
		return;

	// forward to observer
	observer->FICChangeEnsemble(ensemble);
}

std::string FICDecoder::ConvertLabelToUTF8(const FIC_LABEL& label, std::string* charset_name) {
	std::string result = CharsetTools::ConvertTextToUTF8(label.label, sizeof(label.label), label.charset, false, charset_name);

	// discard trailing spaces
	size_t last_pos = result.find_last_not_of(' ');
	if(last_pos != std::string::npos)
		result.resize(last_pos + 1);

	return result;
}

const size_t FICDecoder::uep_sizes[] = {
		 16,  21,  24,  29,  35,  24,  29,  35,  42,  52,  29,  35,  42,  52,  32,  42,
		 48,  58,  70,  40,  52,  58,  70,  84,  48,  58,  70,  84, 104,  58,  70,  84,
		104,  64,  84,  96, 116, 140,  80, 104, 116, 140, 168,  96, 116, 140, 168, 208,
		116, 140, 168, 208, 232, 128, 168, 192, 232, 280, 160, 208, 280, 192, 280, 416
};
const int FICDecoder::uep_pls[] = {
		5, 4, 3, 2, 1, 5, 4, 3, 2, 1, 5, 4, 3, 2, 5, 4,
		3, 2, 1, 5, 4, 3, 2, 1, 5, 4, 3, 2, 1, 5, 4, 3,
		2, 5, 4, 3, 2, 1, 5, 4, 3, 2, 1, 5, 4, 3, 2, 1,
		5, 4, 3, 2, 1, 5, 4, 3, 2, 1, 5, 4, 2, 5, 3, 1
};
const int FICDecoder::uep_bitrates[] = {
		 32,  32,  32,  32,  32,  48,  48,  48,  48,  48,  56,  56,  56,  56,  64,  64,
		 64,  64,  64,  80,  80,  80,  80,  80,  96,  96,  96,  96,  96, 112, 112, 112,
		112, 128, 128, 128, 128, 128, 160, 160, 160, 160, 160, 192, 192, 192, 192, 192,
		224, 224, 224, 224, 224, 256, 256, 256, 256, 256, 320, 320, 320, 384, 384, 384
};
const int FICDecoder::eep_a_size_factors[] = {12,  8,  6,  4};
const int FICDecoder::eep_b_size_factors[] = {27, 21, 18, 15};

const char* FICDecoder::languages_0x00_to_0x2B[] = {
		"unknown/not applicable", "Albanian", "Breton", "Catalan", "Croatian", "Welsh", "Czech", "Danish",
		"German", "English", "Spanish", "Esperanto", "Estonian", "Basque", "Faroese", "French",
		"Frisian", "Irish", "Gaelic", "Galician", "Icelandic", "Italian", "Sami", "Latin",
		"Latvian", "Luxembourgian", "Lithuanian", "Hungarian", "Maltese", "Dutch", "Norwegian", "Occitan",
		"Polish", "Portuguese", "Romanian", "Romansh", "Serbian", "Slovak", "Slovene", "Finnish",
		"Swedish", "Turkish", "Flemish", "Walloon"
};
const char* FICDecoder::languages_0x7F_downto_0x45[] = {
		"Amharic", "Arabic", "Armenian", "Assamese", "Azerbaijani", "Bambora", "Belorussian", "Bengali",
		"Bulgarian", "Burmese", "Chinese", "Chuvash", "Dari", "Fulani", "Georgian", "Greek",
		"Gujurati", "Gurani", "Hausa", "Hebrew", "Hindi", "Indonesian", "Japanese", "Kannada",
		"Kazakh", "Khmer", "Korean", "Laotian", "Macedonian", "Malagasay", "Malaysian", "Moldavian",
		"Marathi", "Ndebele", "Nepali", "Oriya", "Papiamento", "Persian", "Punjabi", "Pushtu",
		"Quechua", "Russian", "Rusyn", "Serbo-Croat", "Shona", "Sinhalese", "Somali", "Sranan Tongo",
		"Swahili", "Tadzhik", "Tamil", "Tatar", "Telugu", "Thai", "Ukranian", "Urdu",
		"Uzbek", "Vietnamese", "Zulu"
};

const char* FICDecoder::ptys_rds_0x00_to_0x1D[] = {
		"No programme type", "News", "Current Affairs", "Information",
		"Sport", "Education", "Drama", "Culture",
		"Science", "Varied", "Pop Music", "Rock Music",
		"Easy Listening Music", "Light Classical", "Serious Classical", "Other Music",
		"Weather/meteorology", "Finance/Business", "Children's programmes", "Social Affairs",
		"Religion", "Phone In", "Travel", "Leisure",
		"Jazz Music", "Country Music", "National Music", "Oldies Music",
		"Folk Music", "Documentary"
};
const char* FICDecoder::ptys_rbds_0x00_to_0x1D[] = {
		"No program type", "News", "Information", "Sports",
		"Talk", "Rock", "Classic Rock", "Adult Hits",
		"Soft Rock", "Top 40", "Country", "Oldies",
		"Soft", "Nostalgia", "Jazz", "Classical",
		"Rhythm and Blues", "Soft Rhythm and Blues", "Foreign Language", "Religious Music",
		"Religious Talk", "Personality", "Public", "College",
		"(rfu)", "(rfu)", "(rfu)", "(rfu)",
		"(rfu)", "Weather"
};

const char* FICDecoder::asu_types_0_to_10[] = {
		"Alarm", "Road Traffic flash", "Transport flash", "Warning/Service",
		"News flash", "Area weather flash", "Event announcement", "Special event",
		"Programme Information", "Sport report", "Financial report"
};

std::string FICDecoder::ConvertLanguageToString(const int value) {
	if(value >= 0x00 && value <= 0x2B)
		return languages_0x00_to_0x2B[value];
	if(value == 0x40)
		return "background sound/clean feed";
	if(value >= 0x45 && value <= 0x7F)
		return languages_0x7F_downto_0x45[0x7F - value];
	return "unknown (" + std::to_string(value) + ")";
}

std::string FICDecoder::ConvertLTOToString(const int value) {
	char lto_string[7];
	snprintf(lto_string, sizeof(lto_string), "%+03d:%02d", value / 2, (value % 2) ? 30 : 0);
	return lto_string;
}

std::string FICDecoder::ConvertInterTableIDToString(const int value) {
	switch(value) {
	case 0x01:
		return "RDS PTY";
	case 0x02:
		return "RBDS PTY";
	default:
		return "unknown";
	}
}

std::string FICDecoder::ConvertDateTimeToString(FIC_DAB_DT utc_dt, const int lto, bool output_ms) {
	const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

	// if desired, apply LTO
	if(lto)
		utc_dt.dt.tm_min += lto * 30;

	// normalize time (apply LTO, set day of week)
	if(mktime(&utc_dt.dt) == (time_t) -1)
		throw std::runtime_error("FICDecoder: error while normalizing date/time");

	std::string result;
	char s[11];

	strftime(s, sizeof(s), "%F", &utc_dt.dt);
	result += std::string(s) + ", " + weekdays[utc_dt.dt.tm_wday] + " - ";

	if(!utc_dt.IsMsNone()) {
		// long form
		strftime(s, sizeof(s), "%T", &utc_dt.dt);
		result += s;
		if(output_ms) {
			snprintf(s, sizeof(s), ".%03d", utc_dt.ms);
			result += s;
		}
	} else {
		// short form
		strftime(s, sizeof(s), "%R", &utc_dt.dt);
		result += s;
	}

	return result;
}

std::string FICDecoder::ConvertPTYToString(const int value, const int inter_table_id) {
	switch(inter_table_id) {
	case 0x01:
		return value <= 0x1D ? ptys_rds_0x00_to_0x1D[value]  : "(not used)";
	case 0x02:
		return value <= 0x1D ? ptys_rbds_0x00_to_0x1D[value] : "(not used)";
	default:
		return "(unknown)";
	}
}

std::string FICDecoder::ConvertASuTypeToString(const int value) {
	if(value >= 0 && value <= 10)
		return asu_types_0_to_10[value];
	return "unknown (" + std::to_string(value) + ")";
}

std::string FICDecoder::DeriveShortLabelUTF8(const std::string& long_label, uint16_t short_label_mask) {
	std::string short_label;

	for(size_t i = 0; i < long_label.length(); i++)		// consider discarded trailing spaces
		if(short_label_mask & (0x8000 >> i))
			short_label += StringTools::UTF8Substr(long_label, i, 1);

	return short_label;
}
