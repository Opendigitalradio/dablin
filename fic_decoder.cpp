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

#include "fic_decoder.h"


const SERVICE SERVICE::no_service(-1);

// --- FICDecoder -----------------------------------------------------------------
FICDecoder::FICDecoder(FICDecoderObserver *observer) {
	this->observer = observer;
	ensemble_label = NULL;
	Reset();
}

void FICDecoder::Reset() {
	audio_services.clear();
	labels.clear();
	known_services.clear();

	{
		std::lock_guard<std::mutex> lock(data_mutex);

		ensemble_id = 0;
		if(ensemble_label) {
			delete ensemble_label;
			ensemble_label = NULL;
		}
		new_services.clear();
	}
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
		fprintf(stderr, "FICDecoder: FIB dropped due to invalid CRC\n");
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

	// ignore other ensembles
	if(header.oe)
		return;


	// handle extension
	switch(header.extension) {
	case 2:
		ProcessFIG0_2(data, len, header);
		break;
//	default:
//		fprintf(stderr, "FICDecoder: received unsupported FIG 0/%d with %zu field bytes\n", header.extension, len);
	}
}



void FICDecoder::ProcessFIG0_2(const uint8_t *data, size_t len, FIG0_HEADER &header) {
	// FIG 0/2 - Basic service and service component definition

	// ignore next config
	if(header.cn)
		return;

	for(size_t offset = 0; offset < len;) {
		uint16_t sid_prog = 0;
//		uint32_t sid_data;

		if(header.pd) {
//			sid_data = data[offset] << 24 | data[offset + 1] << 16 | data[offset + 2] << 8 | data[offset + 3];
			offset += 4;
		} else {
			sid_prog = data[offset] << 8 | data[offset + 1];
			offset += 2;
		}

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

				if(ps && !ca) {
					switch(ascty) {
					case 0:		// DAB
					case 63:	// DAB+
						bool dab_plus = ascty == 63;

						if(audio_services.find(sid_prog) == audio_services.end()) {
							AUDIO_SERVICE service;
							service.subchid = subchid;
							service.dab_plus = dab_plus;

							audio_services[sid_prog] = service;

							fprintf(stderr, "FICDecoder: found new audio service: SId 0x%04X, subchannel %2d, %s\n", sid_prog, subchid, dab_plus ? "DAB+" : "DAB");

							CheckService(sid_prog);
						}

						break;
					}
				}
			}

			offset += 2;
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

	// check for (un)supported extension
	switch(header.extension) {
	case 0:
	case 1:
		break;
	default:
//		fprintf(stderr, "FICDecoder: received unsupported FIG 1/%d with %zu field bytes\n", header.extension, len);
		return;
	}

	// check length
	size_t len_calced = 2 + 16 + 2;
	if(len != len_calced) {
		fprintf(stderr, "FICDecoder: received FIG 1/%d having %zu field bytes (expected: %zu)\n", header.extension, len, len_calced);
		return;
	}

	// parse data
	uint16_t id = data[0] << 8 | data[1];
	FIC_LABEL *label = new FIC_LABEL;
	label->charset = header.charset;
	memcpy(label->label, data + 2, 16);
	label->short_label_mask = data[18] << 8 | data[19];
//	fprintf(stderr, "FICDecoder: label: '%s'\n", label.c_str());


	// handle extension
	switch(header.extension) {
	case 0:
		ProcessFIG1_0(id, label);
		break;
	case 1:
		ProcessFIG1_1(id, label);
		break;
	}
}

void FICDecoder::ProcessFIG1_0(uint16_t id, FIC_LABEL *label) {
	std::lock_guard<std::mutex> lock(data_mutex);

	if(id != ensemble_id || !ensemble_label || *ensemble_label != *label) {
		ensemble_id = id;

		delete ensemble_label;
		ensemble_label = label;

		std::string label_str = ConvertLabelToUTF8(*label);
		fprintf(stderr, "FICDecoder: found new ensemble label: EId 0x%04X, '%s'\n", id, label_str.c_str());

		observer->FICChangeEnsemble();
	} else {
		delete label;
	}
}

void FICDecoder::ProcessFIG1_1(uint16_t id, FIC_LABEL *label) {
	if(labels.find(id) == labels.end()) {
		labels[id] = *label;

		std::string label_str = ConvertLabelToUTF8(*label);
		fprintf(stderr, "FICDecoder: found new programme service label: SId 0x%04X, '%s'\n", id, label_str.c_str());

		CheckService(id);
	}
	delete label;
}


void FICDecoder::GetEnsembleData(uint16_t *id, FIC_LABEL *label) {
	std::lock_guard<std::mutex> lock(data_mutex);

	if(!ensemble_label)
		return;

	if(id)
		*id = ensemble_id;
	if(label)
		memcpy(label, ensemble_label, sizeof(FIC_LABEL));
}

services_t FICDecoder::GetNewServices() {
	std::lock_guard<std::mutex> lock(data_mutex);

	services_t result = new_services;
	new_services.clear();
	return result;
}

void FICDecoder::CheckService(uint16_t sid) {
	// abort, if already known
	if(known_services.find(sid) != known_services.end())
		return;

	// abort, if service or label not found
	audio_services_t::const_iterator services_it = audio_services.find(sid);
	if(services_it == audio_services.cend())
		return;
	labels_t::const_iterator labels_it = labels.find(sid);
	if(labels_it == labels.cend())
		return;

	known_services.insert(sid);

	SERVICE new_service;
	new_service.sid = sid;
	new_service.service = services_it->second;
	new_service.label = labels_it->second;

	{
		std::lock_guard<std::mutex> lock(data_mutex);
		new_services.insert(new_service);
	}

	observer->FICChangeServices();
}

std::string FICDecoder::ConvertLabelToUTF8(const FIC_LABEL& label) {
	return ConvertTextToUTF8(label.label, sizeof(label.label), label.charset);
}

std::string FICDecoder::ConvertTextToUTF8(const uint8_t *data, size_t len, int charset) {
	std::string result;

	// ignore trailing zero bytes (despite they're not allowed)
	while(len > 0 && data[len - 1] == 0x00)
		len--;

	switch(charset) {
	case 0:		// EBU Latin based
		for(size_t i = 0; i < len; i++)
			result += ConvertCharEBUToUTF8(data[i]);
		break;
	case 15:	// UTF-8
	default:	// on unsupported charset, forward untouched
		result = std::string((char*) data, len);
		break;
	}

	// remove leading/trailing spaces
	while(result.size() > 0 && result[0] == ' ')
		result.erase(0, 1);
	while(result.size() > 0 && result[result.size() - 1] == ' ')
		result.erase(result.size() - 1, 1);

	return result;
}

const char* FICDecoder::no_char = "";
const char* FICDecoder::ebu_values_0x00_to_0x1F[] = {
		no_char , "\u0118", "\u012E", "\u0172", "\u0102", "\u0116", "\u010E", "\u0218", "\u021A", "\u010A", no_char , no_char , "\u0120", "\u0139" , "\u017B", "\u0143",
		"\u0105", "\u0119", "\u012F", "\u0173", "\u0103", "\u0117", "\u010F", "\u0219", "\u021B", "\u010B", "\u0147", "\u011A", "\u0121", "\u013A", "\u017C", "\u002D"
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
