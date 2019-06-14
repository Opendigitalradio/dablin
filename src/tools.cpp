/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2019 Stefan Pöschel

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

#include "tools.h"


// --- StringTools -----------------------------------------------------------------
string_vector_t StringTools::SplitString(const std::string &s, const char delimiter) {
	string_vector_t result;
	std::stringstream ss(s);
	std::string part;

	while(std::getline(ss, part, delimiter))
		result.push_back(part);
	return result;
}

std::string StringTools::MsToTimecode(long int value) {
	// ignore milliseconds
	long int tc_s = value / 1000;

	// split
	int h = tc_s / 3600;
	tc_s -= h * 3600;

	int m = tc_s / 60;
	tc_s -= m * 60;

	int s = tc_s;

	// generate output
	char digits[3];

	// just to silence recent GCC's truncation warnings
	m &= 0x3F;
	s &= 0x3F;

	std::string result = std::to_string(h);
	snprintf(digits, sizeof(digits), "%02d", m);
	result += ":" + std::string(digits);
	snprintf(digits, sizeof(digits), "%02d", s);
	result += ":" + std::string(digits);

	return result;
}

size_t StringTools::UTF8CharsLen(const std::string &s, size_t chars) {
	size_t result;
	for(result = 0; result < s.size(); result++) {
		// if not a continuation byte, handle counter
		if((s[result] & 0xC0) != 0x80) {
			if(chars == 0)
				break;
			chars--;
		}
	}
	return result;
}

std::string StringTools::UTF8Substr(const std::string &s, size_t pos, size_t count) {
	std::string result = s;
	result.erase(0, UTF8CharsLen(result, pos));
	result.erase(UTF8CharsLen(result, count));
	return result;
}

std::string StringTools::IntToHex(int value, size_t nibbles) {
	std::string format = "0x%0" + std::to_string(nibbles) + "X";
	char result[2 + nibbles + 1];
	snprintf(result, sizeof(result), format.c_str(), value);
	return result;
}


// --- CharsetTools -----------------------------------------------------------------
const char* CharsetTools::no_char = "";
const char* CharsetTools::ebu_values_0x00_to_0x1F[] = {
		no_char , "\u0118", "\u012E", "\u0172", "\u0102", "\u0116", "\u010E", "\u0218", "\u021A", "\u010A", no_char , no_char , "\u0120", "\u0139" , "\u017B", "\u0143",
		"\u0105", "\u0119", "\u012F", "\u0173", "\u0103", "\u0117", "\u010F", "\u0219", "\u021B", "\u010B", "\u0147", "\u011A", "\u0121", "\u013A", "\u017C", no_char
};
const char* CharsetTools::ebu_values_0x7B_to_0xFF[] = {
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

std::string CharsetTools::ConvertCharEBUToUTF8(const uint8_t value) {
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

std::string CharsetTools::ConvertStringIconvToUTF8(const std::vector<uint8_t>& cleaned_data, std::string* charset_name, const std::string& src_charset) {
	// prepare
	iconv_t conv = iconv_open("UTF-8", src_charset.c_str());
	if(conv == (iconv_t) -1) {
		perror("CharsetTools: error while iconv_open");
		return "";
	}

	size_t input_len = cleaned_data.size();
	char input_bytes[input_len];
	char* input = input_bytes;
	memcpy(input_bytes, &cleaned_data[0], cleaned_data.size());

	size_t output_len = input_len * 4; // theoretical worst case
	size_t output_len_orig = output_len;
	char output_bytes[output_len];
	char* output = output_bytes;

	// convert
	size_t count = iconv(conv, &input, &input_len, &output, &output_len);
	if(count == (size_t) -1) {
		perror("CharsetTools: error while iconv");
		return "";
	}
	if(input_len) {
		fprintf(stderr, "CharsetTools: Could not convert all chars to %s!\n", src_charset.c_str());
		return "";
	}

	if(iconv_close(conv)) {
		perror("CharsetTools: error while iconv_close");
		return "";
	}

	if(charset_name)
		*charset_name = src_charset;
	return std::string(output_bytes, output_len_orig - output_len);
}

std::string CharsetTools::ConvertTextToUTF8(const uint8_t *data, size_t len, int charset, bool mot, std::string* charset_name) {
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

	// convert characters
	if(charset == 0b0000) {			// EBU Latin based
		if(charset_name)
			*charset_name = "EBU Latin based";

		std::string result;
		for(const uint8_t& c : cleaned_data)
			result += ConvertCharEBUToUTF8(c);
		return result;
	}
	if(charset == 0b0100 && mot)	// ISO/IEC-8859-1 (MOT only)
		return ConvertStringIconvToUTF8(cleaned_data, charset_name, "ISO-8859-1");
	if(charset == 0b0110 && !mot)	// UCS-2 BE (DAB only)
		return ConvertStringIconvToUTF8(cleaned_data, charset_name, "UCS-2BE");
	if(charset == 0b1111) {			// UTF-8
		if(charset_name)
			*charset_name = "UTF-8";

		return std::string((char*) &cleaned_data[0], cleaned_data.size());
	}

	// ignore unsupported charset
	fprintf(stderr, "CharsetTools: The %s charset %d is not supported; ignoring!\n", mot ? "MOT" : "DAB", charset);
	return "";
}


// --- CalcCRC -----------------------------------------------------------------
CalcCRC CalcCRC::CalcCRC_CRC16_CCITT(true, true, 0x1021);	// 0001 0000 0010 0001 (16, 12, 5, 0)
CalcCRC CalcCRC::CalcCRC_CRC16_IBM(true, false, 0x8005);	// 1000 0000 0000 0101 (16, 15, 2, 0)
CalcCRC CalcCRC::CalcCRC_FIRE_CODE(false, false, 0x782F);	// 0111 1000 0010 1111 (16, 14, 13, 12, 11, 5, 3, 2, 1, 0)

size_t CalcCRC::CRCLen = 2;

CalcCRC::CalcCRC(bool initial_invert, bool final_invert, uint16_t gen_polynom) {
	this->initial_invert = initial_invert;
	this->final_invert = final_invert;
	this->gen_polynom = gen_polynom;

	FillLUT();
}

void CalcCRC::FillLUT() {
	for(int value = 0; value < 256; value++) {
		uint16_t crc = value << 8;

		for(int i = 0; i < 8; i++) {
			if(crc & 0x8000)
				crc = (crc << 1) ^ gen_polynom;
			else
				crc = crc << 1;
		}

		crc_lut[value] = crc;
	}
}

uint16_t CalcCRC::Calc(const uint8_t *data, size_t len) {
	uint16_t crc;
	Initialize(crc);

	for(size_t offset = 0; offset < len; offset++)
		ProcessByte(crc, data[offset]);

	Finalize(crc);
	return crc;
}

void CalcCRC::ProcessBits(uint16_t& crc, const uint8_t *data, size_t len) {
	// byte-aligned start only

	size_t bytes = len / 8;
	size_t bits = len % 8;

	for(size_t offset = 0; offset < bytes; offset++)
		ProcessByte(crc, data[offset]);
	for(size_t bit = 0; bit < bits; bit++)
		ProcessBit(crc, data[bytes] & (0x80 >> bit));
}


// --- CircularBuffer -----------------------------------------------------------------
CircularBuffer::CircularBuffer(size_t capacity) {
	buffer = new uint8_t[capacity];
	this->capacity = capacity;
	Clear();
}

CircularBuffer::~CircularBuffer() {
	delete[] buffer;
}

size_t CircularBuffer::Write(const uint8_t *data, size_t bytes) {
	size_t real_bytes = std::min(bytes, capacity - size);

	// split task on index rollover
	if(real_bytes <= capacity - index_end) {
		memcpy(buffer + index_end, data, real_bytes);
	} else {
		size_t first_bytes = capacity - index_end;
		memcpy(buffer + index_end, data, first_bytes);
		memcpy(buffer, data + first_bytes, real_bytes - first_bytes);
	}

	index_end = (index_end + real_bytes) % capacity;
	size += real_bytes;
	return real_bytes;
}

size_t CircularBuffer::Read(uint8_t *data, size_t bytes) {
	size_t real_bytes = std::min(bytes, size);

	if(data) {
		// split task on index rollover
		if(real_bytes <= capacity - index_start) {
			memcpy(data, buffer + index_start, real_bytes);
		} else {
			size_t first_bytes = capacity - index_start;
			memcpy(data, buffer + index_start, first_bytes);
			memcpy(data + first_bytes, buffer, real_bytes - first_bytes);
		}
	}

	index_start = (index_start + real_bytes) % capacity;
	size -= real_bytes;
	return real_bytes;
}


// --- BitReader -----------------------------------------------------------------
bool BitReader::GetBits(int& result, size_t count) {
	int result_value = 0;

	while(count) {
		if(data_bytes == 0)
			return false;

		size_t copy_bits = std::min(count, 8 - data_bits);

		result_value <<= copy_bits;
		result_value |= (*data & (0xFF >> data_bits)) >> (8 - data_bits - copy_bits);

		data_bits += copy_bits;
		count -= copy_bits;

		// switch to next byte
		if(data_bits == 8) {
			data++;
			data_bytes--;
			data_bits = 0;
		}
	}

	result = result_value;
	return true;
}


// --- BitWriter -----------------------------------------------------------------
void BitWriter::Reset() {
	data.clear();
	byte_bits = 0;
}

void BitWriter::AddBits(int data_new, size_t count) {
	while(count) {
		// add new byte, if needed
		if(byte_bits == 0)
			data.push_back(0x00);

		size_t copy_bits = std::min(count, 8 - byte_bits);
		uint8_t copy_data = (data_new >> (count - copy_bits)) & (0xFF >> (8 - copy_bits));
		data.back() |= copy_data << (8 - byte_bits - copy_bits);

//		fprintf(stderr, "data_new: 0x%04X, count: %zu / byte_bits: %zu, copy_bits: %zu, copy_data: 0x%02X\n", data_new, count, byte_bits, copy_bits, copy_data);

		byte_bits = (byte_bits + copy_bits) % 8;
		count -= copy_bits;
	}
}

void BitWriter::AddBytes(const uint8_t *data, size_t len) {
	for(size_t i = 0; i < len; i++)
		AddBits(data[i], 8);
}

void BitWriter::WriteAudioMuxLengthBytes() {
	size_t len = data.size() - 3;
	data[1] |= (len >> 8) & 0x1F;
	data[2] = len & 0xFF;
}

const dab_channels_t dab_channels {
	{ "5A",  174928},
	{ "5B",  176640},
	{ "5C",  178352},
	{ "5D",  180064},

	{ "6A",  181936},
	{ "6B",  183648},
	{ "6C",  185360},
	{ "6D",  187072},

	{ "7A",  188928},
	{ "7B",  190640},
	{ "7C",  192352},
	{ "7D",  194064},

	{ "8A",  195936},
	{ "8B",  197648},
	{ "8C",  199360},
	{ "8D",  201072},

	{ "9A",  202928},
	{ "9B",  204640},
	{ "9C",  206352},
	{ "9D",  208064},

	{"10A",  209936},
	{"10N",  210096},
	{"10B",  211648},
	{"10C",  213360},
	{"10D",  215072},

	{"11A",  216928},
	{"11N",  217088},
	{"11B",  218640},
	{"11C",  220352},
	{"11D",  222064},

	{"12A",  223936},
	{"12N",  224096},
	{"12B",  225648},
	{"12C",  227360},
	{"12D",  229072},

	{"13A",  230784},
	{"13B",  232496},
	{"13C",  234208},
	{"13D",  235776},
	{"13E",  237488},
	{"13F",  239200},


	{ "LA", 1452960},
	{ "LB", 1454672},
	{ "LC", 1456384},
	{ "LD", 1458096},

	{ "LE", 1459808},
	{ "LF", 1461520},
	{ "LG", 1463232},
	{ "LH", 1464944},

	{ "LI", 1466656},
	{ "LJ", 1468368},
	{ "LK", 1470080},
	{ "LL", 1471792},

	{ "LM", 1473504},
	{ "LN", 1475216},
	{ "LO", 1476928},
	{ "LP", 1478640},
};
