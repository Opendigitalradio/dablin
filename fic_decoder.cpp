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


// --- FICDecoder -----------------------------------------------------------------
FICDecoder::FICDecoder(FICDecoderObserver *observer) {
	this->observer = observer;
	ensemble_id = 0;
	ensemble_label = NULL;
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

	bool cn = data[0] & 0x80;
	bool oe = data[0] & 0x40;
	bool pd = data[0] & 0x20;
	int extension = data[0] & 0x1F;

	// TODO
	if(cn || oe)
		return;

	switch(extension) {
	case 2: {

		// FIG 0/2 - Basic service and service component definition
		for(size_t offset = 1; offset < len;) {
			uint16_t sid_prog = 0;
//			uint32_t sid_data;

			if(pd) {
//				sid_data = data[offset] << 24 | data[offset + 1] << 16 | data[offset + 2] << 8 | data[offset + 3];
//				offset += 4;
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

								fprintf(stderr, "FICDecoder: found new audio service: SID 0x%04X, subchannel %2d, %s\n", sid_prog, subchid, dab_plus ? "DAB+" : "DAB");

								CheckService(sid_prog);
							}

							break;
						}
					}
				}

				offset += 2;
			}
		}

		break; }
	default:
//		fprintf(stderr, "FICDecoder: received unsupported FIG 0/%d with %zu bytes\n", extension, len);
		return;
	}



}


void FICDecoder::ProcessFIG1(const uint8_t *data, size_t len) {
	if(len < 1) {
		fprintf(stderr, "FICDecoder: received empty FIG 1\n");
		return;
	}

	int charset = data[0] >> 4;
	bool oe = data[0] & 0x08;
	int extension = data[0] & 0x07;

	switch(extension) {
	case 0:
		break;
	case 1:
		break;
	default:
//		fprintf(stderr, "FICDecoder: received unsupported FIG 1/%d with %zu bytes\n", extension, len);
		return;
	}

	size_t len_calced = 1 + 2 + 16 + 2;
	if(len != len_calced) {
		fprintf(stderr, "FICDecoder: received FIG 1/%d having %zu bytes (expected: %zu)\n", extension, len, len_calced);
		return;
	}

	// parse data
	uint16_t id = data[1] << 8 | data[2];
	FIC_LABEL *label = new FIC_LABEL;
	label->charset = charset;
	memcpy(label->label, data + 1 + 2, 16);
	label->short_label_mask = data[19] << 8 | data[20];

	// ignore OE labels
	if(oe)
		return;



//	fprintf(stderr, "FICDecoder: label: '%s'\n", label.c_str());

	switch(extension) {
	case 0: {
			std::lock_guard<std::mutex> lock(data_mutex);

			if(id != ensemble_id || !ensemble_label || *ensemble_label != *label) {
				ensemble_id = id;

				delete ensemble_label;
				ensemble_label = label;
				label = NULL;

				std::string label_str((char*) ensemble_label->label, 16);
				fprintf(stderr, "FICDecoder: found new ensemble ID/label: 0x%04X '%s'\n", id, label_str.c_str());

				observer->FICChangeEnsemble();
			}
		break; }
	case 1: {
		if(labels.find(id) == labels.end()) {
			labels[id] = *label;

			std::string label_str((char*) label->label, 16);
			fprintf(stderr, "FICDecoder: found new service ID/label: 0x%04X '%s'\n", id, label_str.c_str());

			CheckService(id);
		}

		break; }
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
	audio_services_t::iterator services_it = audio_services.find(sid);
	if(services_it == audio_services.end())
		return;
	labels_t::iterator labels_it = labels.find(sid);
	if(labels_it == labels.end())
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


std::string FICDecoder::ConvertTextToUTF8(const uint8_t *data, size_t len, int charset, bool dynamic_label) {
	std::string result;

	// ignore trailing zero bytes (despite they're not allowed)
	while(len > 0 && data[len - 1] == 0x00)
		len--;

	switch(charset) {
	case 0:		// EBU Latin based
		for(size_t i = 0; i < len; i++)
			result += ConvertCharEBUToUTF8(data[i], dynamic_label);
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

const char* FICDecoder::ebu_values_0x80[] = {
		"\u00E1", "\u00E0", "\u00E9", "\u00E8", "\u00ED", "\u00EC", "\u00F3", "\u00F2", "\u00FA", "\u00F9", "\u00D1", "\u00C7", "\u015E", "\u00DF", "\u00A1", "\u0132",
		"\u00E2", "\u00E4", "\u00EA", "\u00EB", "\u00EE", "\u00EF", "\u00F4", "\u00F6", "\u00FB", "\u00FC", "\u00F1", "\u00E7", "\u015F", "\u011F", "\u0131", "\u0133",
		"\u00AA", "\u03B1", "\u00A9", "\u2030", "\u011E", "\u011B", "\u0148", "\u0151", "\u03C0", "\u20AC", "\u00A3", "\u0024", "\u2190", "\u2191", "\u2192", "\u2193",
		"\u00BA", "\u00B9", "\u00B2", "\u00B3", "\u00B1", "\u0130", "\u0144", "\u0171", "\u00B5", "\u00BF", "\u00F7", "\u00B0", "\u00BC", "\u00BD", "\u00BE", "\u00A7",
		"\u00C1", "\u00C0", "\u00C9", "\u00C8", "\u00CD", "\u00CC", "\u00D3", "\u00D2", "\u00DA", "\u00D9", "\u0158", "\u010C", "\u0160", "\u017D", "\u00D0", "\u013F",
		"\u00C2", "\u00C4", "\u00CA", "\u00CB", "\u00CE", "\u00CF", "\u00D4", "\u00D6", "\u00DB", "\u00DC", "\u0159", "\u010D", "\u0161", "\u017E", "\u0111", "\u0140",
		"\u00C3", "\u00C5", "\u00C6", "\u0152", "\u0177", "\u00DD", "\u00D5", "\u00D8", "\u00DE", "\u014A", "\u0154", "\u0106", "\u015A", "\u0179", "\u0166", "\u00F0",
		"\u00E3", "\u00E5", "\u00E6", "\u0153", "\u0175", "\u00FD", "\u00F5", "\u00F8", "\u00FE", "\u014B", "\u0155", "\u0107", "\u015B", "\u017A", "\u0167"
};

std::string FICDecoder::ConvertCharEBUToUTF8(const uint8_t value, bool dynamic_label) {
	const std::string unknown_char = "?";

	if(dynamic_label) {
		// ignore special control chars
		switch(value) {
		case 0x0A:	// preferred line break
		case 0x0B:	// end of headline
		case 0x1F:	// preferred word break
			return " ";
		}
	}

	// handle all regions
	switch(value) {
	case 0x7F:
	case 0xFF:
		return unknown_char;
	}

	if(value < 0x20)
		return unknown_char;

	if(value >= 0x20 && value < 0x80) {
		switch(value) {
		case 0x24:
			return "\u00A4";
		case 0x5E:
			return "\u2015";
		case 0x60:
			return "\u2551";
		case 0x7E:
			return "\u00AF";
		default:
			return std::string((char*) &value, 1);
		}
	}

	if(value >= 0x80 && value < 0x100)
		return ebu_values_0x80[value - 0x80];

	return unknown_char;
}
