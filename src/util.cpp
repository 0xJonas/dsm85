#include "util.h"
#include <stdexcept>

#define BINARY 2
#define OCTAL 8
#define DECIMAL 10
#define HEXADECIMAL 16

const char hex_digits[] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };

/*
Converts an int literal to an int. The base is selected based on the form of the input string.
Base 2: %nnnn, 0bnnnn, 0Bnnnn nnnnb
Base 8: @nnnn, 0nnnn, nnnno, nnnnO, nnnnq, nnnnQ
Base 10: nnnn, &nnnn, nnnnd, nnnnD
Base 16: $nnnn, 0xnnnn, 0Xnnnn, nnnnx, nnnnX
*/
int parse_int_literal(std::string str) {
	if (str.length() == 0)
		throw std::invalid_argument("Integer literal is empty.");

	int base = DECIMAL;
	std::string raw_literal = str;
	switch (str[0]) {
	case '$':
		base = HEXADECIMAL;
		raw_literal = str.substr(1,str.length()-1);
		break;
	case '&':
		base = DECIMAL;
		raw_literal = str.substr(1, str.length() - 1);
		break;
	case '@':
		base = OCTAL;
		raw_literal = str.substr(1, str.length() - 1);
		break;
	case '%':
		base = BINARY;
		raw_literal = str.substr(1, str.length() - 1);
		break;
	case '0':
		if (str.length() < 2)
			return 0;	//literal "0"
		if (str[1] == 'x' || str[1] == 'X') {
			base = HEXADECIMAL;
			raw_literal = str.substr(2, str.length() - 2);
		}
		else if (str[1] == 'b' || str[1] == 'B') {
			base = BINARY;
			raw_literal = str.substr(2, str.length() - 2);
		}
		else {
			base = OCTAL;
			raw_literal = str.substr(1, str.length() - 1);
		}
	}

	if (base == DECIMAL) {
		switch (str[str.length() - 1]) {
		case 'h':
		case 'H':
			base = HEXADECIMAL;
			raw_literal = str.substr(0, str.length() - 1);
			break;
		case 'd':
		case 'D':
			base = DECIMAL;
			raw_literal = str.substr(0, str.length() - 1);
			break;
		case 'o':
		case 'O':
		case 'q':
		case 'Q':
			base = OCTAL;
			raw_literal = str.substr(0, str.length() - 1);
			break;
		case 'b':
		case 'B':
			base = BINARY;
			raw_literal = str.substr(0, str.length() - 1);
			break;
		}
	}

	return std::stoi(raw_literal, nullptr, base);
}

/*
Converts an integer to a 4-character wide hex string
*/
std::string hex16bit(const int v) {
	std::string str = "";
	str += hex_digits[(v >> 12) & 0xf];
	str += hex_digits[(v >> 8) & 0xf];
	str += hex_digits[(v >> 4) & 0xf];
	str += hex_digits[(v) & 0xf];
	return str;
}

/*
Converts an integer to a 2-character wide hex string
*/
std::string hex8bit(const int v) {
	std::string str = "";
	str += hex_digits[(v >> 4) & 0xf];
	str += hex_digits[(v) & 0xf];
	return str;
}