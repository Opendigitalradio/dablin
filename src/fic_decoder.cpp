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

#include "fic_decoder.h"


// --- FICDecoder -----------------------------------------------------------------
void FICDecoder::Reset() {
	ensemble = FIC_ENSEMBLE();
	services.clear();
	subchannels.clear();
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
		fprintf(stderr, "\x1B[33m" "(FIB)" "\x1B[0m" " ");
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
	case 8:
		ProcessFIG0_8(data, len);
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
			FIC_SUBCHANNEL& current_sc = subchannels[subchid];
			if(current_sc != sc) {
				current_sc = sc;

				fprintf(stderr, "FICDecoder: SubChId %2d: start %3zu CUs, size %3zu CUs, PL %-7s = %3d kBit/s\n", subchid, sc.start, sc.size, sc.pl.c_str(), sc.bitrate);

				// update services that consist of this sub-channel
				for(fic_services_t::const_iterator it = services.cbegin(); it != services.cend(); it++) {
					const FIC_SERVICE& s = it->second;
					if(s.audio_service.subchid == subchid || s.sec_comps.find(subchid) != s.sec_comps.end())
						UpdateService(s);
				}
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
						AUDIO_SERVICE& current_audio_service = ps ? service.audio_service : service.sec_comps[subchid];
						if(current_audio_service != audio_service) {
							current_audio_service = audio_service;

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

		std::string label_str = ConvertLabelToUTF8(label);
		fprintf(stderr, "FICDecoder: EId 0x%04X: ensemble label '%s'\n", eid, label_str.c_str());

		observer->FICChangeEnsemble(ensemble);
	}
}

void FICDecoder::ProcessFIG1_1(uint16_t sid, const FIC_LABEL& label) {
	FIC_SERVICE& service = GetService(sid);
	if(service.label != label) {
		service.label = label;

		std::string label_str = ConvertLabelToUTF8(label);
		fprintf(stderr, "FICDecoder: SId 0x%04X: programme service label '%s'\n", sid, label_str.c_str());

		UpdateService(service);
	}
}

void FICDecoder::ProcessFIG1_4(uint16_t sid, int scids, const FIC_LABEL& label) {
	// programme services only

	FIC_SERVICE& service = GetService(sid);
	FIC_LABEL& comp_label = service.comp_labels[scids];
	if(comp_label != label) {
		comp_label = label;

		std::string label_str = ConvertLabelToUTF8(label);
		fprintf(stderr, "FICDecoder: SId 0x%04X, SCIdS %2d: service component label '%s'\n", sid, scids, label_str.c_str());

		UpdateService(service);
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
	// abort update, if audio service or label not yet present
	if(service.audio_service.IsNone() || service.label.IsNone())
		return;

	// secondary components (if both component and definition are present)
	bool multi_comps = false;
	for(comp_defs_t::const_iterator it = service.comp_defs.cbegin(); it != service.comp_defs.cend(); it++) {
		if(service.sec_comps.find(it->second) == service.sec_comps.end())
			continue;
		UpdateListedService(service, it->first, true);
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
	ls.pri_comp_subchid = service.audio_service.subchid;
	ls.multi_comps = multi_comps;

	if(scids == LISTED_SERVICE::scids_none) {	// primary component
		ls.audio_service = service.audio_service;
	} else {									// secondary component
		ls.audio_service = service.sec_comps.at(service.comp_defs.at(scids));

		// use component label, if available
		comp_labels_t::const_iterator cl_it = service.comp_labels.find(scids);
		if(cl_it != service.comp_labels.end())
			ls.label = cl_it->second;
	}

	// use sub-channel information, if available
	fic_subchannels_t::const_iterator sc_it = subchannels.find(ls.audio_service.subchid);
	if(sc_it != subchannels.end())
		ls.subchannel = sc_it->second;

	// forward to observer
	observer->FICChangeService(ls);
}

std::string FICDecoder::ConvertLabelToUTF8(const FIC_LABEL& label) {
	std::string result = ConvertTextToUTF8(label.label, sizeof(label.label), label.charset);

	// discard trailing spaces
	size_t last_pos = result.find_last_not_of(' ');
	if(last_pos != std::string::npos)
		result.resize(last_pos + 1);

	return result;
}

std::string FICDecoder::ConvertTextToUTF8(const uint8_t *data, size_t len, int charset) {
	// remove undesired chars
	std::vector<uint8_t> cleaned_data;
	for(size_t i = 0; i < len; i++) {
		switch(data[i]) {
		case 0x00:	// NULL
		case 0x0A:	// PLB
		case 0x0B:	// EoH
		case 0x1F:	// PWB
			continue;
		default:
			cleaned_data.push_back(data[i]);
		}
	}

	std::string result;

	// convert characters
	switch(charset) {
	case 0:		// EBU Latin based
		for(size_t i = 0; i < cleaned_data.size(); i++)
			result += ConvertCharEBUToUTF8(cleaned_data[i]);
		break;
	case 15:	// UTF-8
	default:	// on unsupported charset, forward untouched
		result = std::string((char*) &cleaned_data[0], cleaned_data.size());
		break;
	}

	return result;
}

const char* FICDecoder::no_char = "";
const char* FICDecoder::ebu_values_0x00_to_0x1F[] = {
		no_char , "\u0118", "\u012E", "\u0172", "\u0102", "\u0116", "\u010E", "\u0218", "\u021A", "\u010A", no_char , no_char , "\u0120", "\u0139" , "\u017B", "\u0143",
		"\u0105", "\u0119", "\u012F", "\u0173", "\u0103", "\u0117", "\u010F", "\u0219", "\u021B", "\u010B", "\u0147", "\u011A", "\u0121", "\u013A", "\u017C", no_char
};
const char* FICDecoder::ebu_values_0x7B_to_0xFF[] = {
		/* starting some chars earlier than 0x80 -----> */                                                            "\u00AB", "\u016F", "\u00BB", "\u013D", "\u0126",
		"\u00E1", "\u00E0", "\u00E9", "\u00E8", "\u00ED", "\u00EC", "\u00F3", "\u00F2", "\u00FA", "\u00F9", "\u00D1", "\u00C7", "\u015E", "\u00DF", "\u00A1", "\u0178",
		"\u00E2", "\u00E4", "\u00EA", "\u00EB", "\u00EE", "\u00EF", "\u00F4", "\u00F6", "\u00FB", "\u00FC", "\u00F1", "\u00E7", "\u015F", "\u011F", "\u0131", "\u00FF",
		"\u0136", "\u0145", "\u00A9", "\u0122", "\u011E", "\u011B", "\u0148", "\u0151", "\u0150", "\u20AC", "\u00A3", "\u0024", "\u0100", "\u0112", "\u012A", "\u016A",
		"\u0137", "\u0146", "\u013B", "\u0123", "\u013C", "\u0130", "\u0144", "\u0171", "\u0170", "\u00BF", "\u013E", "\u00B0", "\u0101", "\u0113", "\u012B", "\u016B",
		"\u00C1", "\u00C0", "\u00C9", "\u00C8", "\u00CD", "\u00CC", "\u00D3", "\u00D2", "\u00DA", "\u00D9", "\u0158", "\u010C", "\u0160", "\u017D", "\u00D0", "\u013F",
		"\u00C2", "\u00C4", "\u00CA", "\u00CB", "\u00CE", "\u00CF", "\u00D4", "\u00D6", "\u00DB", "\u00DC", "\u0159", "\u010D", "\u0161", "\u017E", "\u0111", "\u0140",
		"\u00C3", "\u00C5", "\u00C6", "\u0152", "\u0177", "\u00DD", "\u00D5", "\u00D8", "\u00DE", "\u014A", "\u0154", "\u0106", "\u015A", "\u0179", "\u0164", "\u00F0",
		"\u00E3", "\u00E5", "\u00E6", "\u0153", "\u0175", "\u00FD", "\u00F5", "\u00F8", "\u00FE", "\u014B", "\u0155", "\u0107", "\u015B", "\u017A", "\u0165", "\u0127"
};

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

std::string FICDecoder::ConvertCharEBUToUTF8(const uint8_t value) {
	// convert via LUT
	if(value <= 0x1F)
		return ebu_values_0x00_to_0x1F[value];
	if(value >= 0x7B)
		return ebu_values_0x7B_to_0xFF[value - 0x7B];

	// convert by hand (avoiding a LUT with mostly 1:1 mapping)
	switch(value) {
	case 0x24:
		return "\u0142";
	case 0x5C:
		return "\u016E";
	case 0x5E:
		return "\u0141";
	case 0x60:
		return "\u0104";
	}

	// leave untouched
	return std::string((char*) &value, 1);
}
