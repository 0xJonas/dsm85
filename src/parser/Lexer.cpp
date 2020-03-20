#include "Lexer.h"
#include <string>

std::string Token::to_string() {
	std::string str = "<";
	str += std::to_string(token_type);
	str += ", ";
	str += lexem;
	str += ">";
	return str;
}

Lexer::Lexer(std::istream &in, size_t initial_capacity) : in(in), capacity(initial_capacity) {
	lexem_buffer = new char[initial_capacity];
	reset();
}

Lexer::~Lexer() {
	delete[] lexem_buffer;
}

void Lexer::increase_capacity(size_t new_capacity) {
	if (new_capacity <= capacity)
		return;

	char *new_buffer = new char[new_capacity];
	for (unsigned int i = 0; i < lexem_length; i++) {
		new_buffer[i] = lexem_buffer[i];
	}
	delete[] lexem_buffer;
	lexem_buffer = new_buffer;
}

static bool is_whitespace(char c) {
	switch (c) {
	case ' ':
	case '\t':
	case '\r':
		return true;
	}
	return false;
}

static bool is_valid_identifier_start(char c) {
	if (c >= 'a' && c <= 'z')
		return true;
	if (c >= 'A' && c <= 'Z')
		return true;
	if (c == '_')
		return true;
	return false;
}

static bool is_valid_identifier_body(char c) {
	if (c >= '0' && c <= '9')
		return true;
	return is_valid_identifier_start(c);
}

static bool is_valid_literal_start(char c) {
	switch (c) {
	case '$':
	case '&':
	case '@':
	case '%':
		return true;
	}
	return c >= '0' && c <= '9';
}

static bool is_valid_literal_body(char c) {
	switch (c) {
	case 'h':
	case 'H':
	case 'd':
	case 'D':
	case 'o':
	case 'O':
	case 'q':
	case 'Q':
	case 'b':
	case 'B':
	case 'x':
	case 'X':
		return true;
	}
	return c >= '0' && c <= '9'
		|| c >= 'a' && c <= 'f'
		|| c >= 'A' && c <= 'F';
}

enum states {
	S_START,
	S_COMMENT,
	S_IDENTIFIER,
	S_LITERAL,
	S_STRING,
	S_STRING_MASK,
	S_STRING_COMPLETE,
	S_NEWLINE,
	S_RANGE,
	S_RANGE_CONT,
	S_LEFT_PARENTHESES,
	S_RIGHT_PARENTHESES,
	S_LEFT_BRACKET,
	S_RIGHT_BRACKET,
	S_ADD,
	S_SUBTRACT,
	S_MULTIPLY,
	S_DIVIDE,
	S_MODULO,
	S_EOI,

	S_BYTES,
	S_CODE,
	S_COMMENTS,
	S_DWORDS,
	S_DWORDS_BE,
	S_DWORDS_LE,
	S_TEXT,
	S_RET,
	S_INCLUDE,
	S_LABELS,
	S_SEGMENTS,
	S_WORDS,

	//Keep this the last value:
	NUM_STATES
};

#define MATCH(c) \
	if(peek==c) \
		state+=NUM_STATES; \
	else if(is_whitespace(peek)) \
		return IDENTIFIER; \
	else \
		state=S_IDENTIFIER; \
		return PUSH;

//Defines a state where the corresponding reserved word has been matched up to and including the b-th character
#define MATCHED_UP_TO(a,b) a+b*NUM_STATES

#define PUSH -1
#define SKIP -2

int Lexer::process_character() {
	switch (state) {
	case S_START:
		if (is_whitespace(peek))
			return SKIP;

		if (is_valid_identifier_start(peek))
			state = S_IDENTIFIER;

		if (is_valid_literal_start(peek))
			state = S_LITERAL;

		switch (peek) {
		case '\n': state = S_NEWLINE; return PUSH;
		case '+': state = S_ADD; return PUSH;
		case '-': state = S_SUBTRACT; return PUSH;
		case '*': state = S_MULTIPLY; return PUSH;
		case '/': state = S_DIVIDE; return PUSH;
		case '%': state = S_MODULO; return PUSH;
		case '(': state = S_LEFT_PARENTHESES; return PUSH;
		case ')': state = S_RIGHT_PARENTHESES; return PUSH;
		case '[': state = S_LEFT_BRACKET; return PUSH;
		case ']': state = S_RIGHT_BRACKET; return PUSH;
		case '.': state = S_RANGE; return PUSH;
		case '\"': state = S_STRING; return SKIP;
		case 'b': state = MATCHED_UP_TO(S_BYTES, 0); return PUSH;
		case 'c': state = MATCHED_UP_TO(S_CODE, 0); return PUSH;
		case 'd': state = MATCHED_UP_TO(S_DWORDS, 0); return PUSH;
		case 'i': state = MATCHED_UP_TO(S_INCLUDE, 0); return PUSH;
		case 'l': state = MATCHED_UP_TO(S_LABELS, 0); return PUSH;
		case 'r': state = MATCHED_UP_TO(S_RET, 0); return PUSH;
		case 's': state = MATCHED_UP_TO(S_SEGMENTS, 0); return PUSH;
		case 't': state = MATCHED_UP_TO(S_TEXT, 0); return PUSH;
		case 'w': state = MATCHED_UP_TO(S_WORDS, 0); return PUSH;
		case '#': state = MATCHED_UP_TO(S_COMMENT, 0); return SKIP;
		case -1: state = S_EOI; return PUSH;
		}
		return PUSH;
	case S_COMMENT:
		if (peek == '\n') {
			state = S_NEWLINE;
			return PUSH;
		}else
			return SKIP;
	case S_IDENTIFIER:
		if (!is_valid_identifier_body(peek))
			return IDENTIFIER;
		else
			return PUSH;
	case S_LITERAL:
		//Literals are checked in the parser, not in the lexer
		if (is_valid_literal_body(peek))
			return PUSH;
		else
			return LITERAL;
	case S_STRING:
		if (peek == '\\') {
			state = S_STRING_MASK;
			return SKIP;
		}
		else if (peek == '\"') {
			state = S_STRING_COMPLETE;
			return SKIP;
		}
		else
			return PUSH;
	case S_STRING_MASK:
		//TODO add special characters e.g. newline, tab
		state = S_STRING;
		return PUSH;
	case S_STRING_COMPLETE: return STRING;
	case S_NEWLINE: return NEWLINE;
	case S_RANGE:
		if (peek == '.') {
			state = S_RANGE_CONT;
			return PUSH;
		}
		else
			return ERROR;
	case S_RANGE_CONT: return RANGE;
	case S_ADD: return ADD;
	case S_SUBTRACT: return SUBTRACT;
	case S_MULTIPLY: return MULTIPLY;
	case S_DIVIDE: return DIVIDE;
	case S_MODULO:
		if (is_whitespace(peek))
			return MODULO;
		else {
			state = S_LITERAL;
			return PUSH;
		}
	case S_LEFT_PARENTHESES: return LEFT_PARENTHESES;
	case S_RIGHT_PARENTHESES: return RIGHT_PARENTHESES;
	case S_LEFT_BRACKET: return LEFT_BRACKET;
	case S_RIGHT_BRACKET: return RIGHT_BRACKET;
	case S_EOI: return EOI;

	case MATCHED_UP_TO(S_BYTES, 0): MATCH('y')
	case MATCHED_UP_TO(S_BYTES, 1): MATCH('t')
	case MATCHED_UP_TO(S_BYTES, 2): MATCH('e')
	case MATCHED_UP_TO(S_BYTES, 3): MATCH('s')
	case MATCHED_UP_TO(S_BYTES, 4):
		if (is_whitespace(peek))
			return BYTES;
		else {
			state = S_IDENTIFIER;
			return PUSH;
		}

	case MATCHED_UP_TO(S_CODE, 0): MATCH('o')
	case MATCHED_UP_TO(S_CODE, 1):
		if (peek == 'd')
			state++;
		else if (peek == 'm')
			state = MATCHED_UP_TO(S_COMMENTS, 2);
		else
			state = S_IDENTIFIER;
		return PUSH;
	case MATCHED_UP_TO(S_CODE, 2): MATCH('e')
	case MATCHED_UP_TO(S_CODE, 3):
		if(is_whitespace(peek))
			return CODE;
		else {
			state = S_IDENTIFIER;
			return PUSH;
		}

	case MATCHED_UP_TO(S_COMMENTS, 2): MATCH('m')
	case MATCHED_UP_TO(S_COMMENTS, 3): MATCH('e')
	case MATCHED_UP_TO(S_COMMENTS, 4): MATCH('n')
	case MATCHED_UP_TO(S_COMMENTS, 5): MATCH('t')
	case MATCHED_UP_TO(S_COMMENTS, 6): MATCH('s')
	case MATCHED_UP_TO(S_COMMENTS, 7): MATCH(':')
	case MATCHED_UP_TO(S_COMMENTS, 8): return COMMENTS;

	case MATCHED_UP_TO(S_DWORDS, 0): MATCH('w')
	case MATCHED_UP_TO(S_DWORDS, 1): MATCH('o')
	case MATCHED_UP_TO(S_DWORDS, 2): MATCH('r')
	case MATCHED_UP_TO(S_DWORDS, 3): MATCH('d')
	case MATCHED_UP_TO(S_DWORDS, 4): MATCH('s')
	case MATCHED_UP_TO(S_DWORDS, 5):
		if (peek == '_') {
			state = MATCHED_UP_TO(S_DWORDS_BE, 6);
			return PUSH;
		}
		else if(is_whitespace(peek))
			return DWORDS;
		else {
			state = S_IDENTIFIER;
			return PUSH;
		}
	case MATCHED_UP_TO(S_DWORDS_BE, 6):
		if (peek == 'b')
			state = MATCHED_UP_TO(S_DWORDS_BE, 7);
		else if (peek == 'l')
			state = MATCHED_UP_TO(S_DWORDS_LE, 7);
		else if (is_whitespace(peek))
			return IDENTIFIER;
		else
			state = S_IDENTIFIER;
		return PUSH;
	case MATCHED_UP_TO(S_DWORDS_BE, 7): MATCH('e')
	case MATCHED_UP_TO(S_DWORDS_BE, 8):
		if (is_whitespace(peek))
			return DWORDS_BE;
		else{
			state = S_IDENTIFIER;
			return PUSH;
		}
	case MATCHED_UP_TO(S_DWORDS_LE, 7): MATCH('e')
	case MATCHED_UP_TO(S_DWORDS_LE, 8):
		if (is_whitespace(peek))
			return DWORDS_LE;
		else {
			state = S_IDENTIFIER;
			return PUSH;
		}

	case MATCHED_UP_TO(S_INCLUDE, 0): MATCH('n')
	case MATCHED_UP_TO(S_INCLUDE, 1): MATCH('c')
	case MATCHED_UP_TO(S_INCLUDE, 2): MATCH('l')
	case MATCHED_UP_TO(S_INCLUDE, 3): MATCH('u')
	case MATCHED_UP_TO(S_INCLUDE, 4): MATCH('d')
	case MATCHED_UP_TO(S_INCLUDE, 5): MATCH('e')
	case MATCHED_UP_TO(S_INCLUDE, 6): MATCH(':')
	case MATCHED_UP_TO(S_INCLUDE, 7): return INCLUDE;

	case MATCHED_UP_TO(S_LABELS, 0): MATCH('a')
	case MATCHED_UP_TO(S_LABELS, 1): MATCH('b')
	case MATCHED_UP_TO(S_LABELS, 2): MATCH('e')
	case MATCHED_UP_TO(S_LABELS, 3): MATCH('l')
	case MATCHED_UP_TO(S_LABELS, 4): MATCH('s')
	case MATCHED_UP_TO(S_LABELS, 5): MATCH(':')
	case MATCHED_UP_TO(S_LABELS, 6): return LABELS;

	case MATCHED_UP_TO(S_RET, 0): MATCH('e')
	case MATCHED_UP_TO(S_RET, 1): MATCH('t')
	case MATCHED_UP_TO(S_RET, 2):
		if (is_whitespace(peek))
			return RET;
		else {
			state = S_IDENTIFIER;
			return PUSH;
		}

	case MATCHED_UP_TO(S_SEGMENTS, 0): MATCH('e')
	case MATCHED_UP_TO(S_SEGMENTS, 1): MATCH('g')
	case MATCHED_UP_TO(S_SEGMENTS, 2): MATCH('m')
	case MATCHED_UP_TO(S_SEGMENTS, 3): MATCH('e')
	case MATCHED_UP_TO(S_SEGMENTS, 4): MATCH('n')
	case MATCHED_UP_TO(S_SEGMENTS, 5): MATCH('t')
	case MATCHED_UP_TO(S_SEGMENTS, 6): MATCH('s')
	case MATCHED_UP_TO(S_SEGMENTS, 7): MATCH(':')
	case MATCHED_UP_TO(S_SEGMENTS, 8): return SEGMENTS;

	case MATCHED_UP_TO(S_TEXT, 0): MATCH('e')
	case MATCHED_UP_TO(S_TEXT, 1): MATCH('x')
	case MATCHED_UP_TO(S_TEXT, 2): MATCH('t')
	case MATCHED_UP_TO(S_TEXT, 3):
		if (is_whitespace(peek))
			return TEXT;
		else {
			state = S_IDENTIFIER;
			return PUSH;
		}

	case MATCHED_UP_TO(S_WORDS, 0): MATCH('o')
	case MATCHED_UP_TO(S_WORDS, 1): MATCH('r')
	case MATCHED_UP_TO(S_WORDS, 2): MATCH('d')
	case MATCHED_UP_TO(S_WORDS, 3): MATCH('s')
	case MATCHED_UP_TO(S_WORDS, 4): 
		if(is_whitespace(peek))
			return WORDS;
		else {
			state = S_IDENTIFIER;
			return PUSH;
		}
	}
	
	//Error
	state = S_START;
	return PUSH;
}

Token Lexer::next_token() {
	state = S_START;
	lexem_length = 0;

	int token_type = process_character();
	while (true) {
		if (token_type == PUSH) {
			lexem_buffer[lexem_length++] = peek;
			if (lexem_length >= capacity)
				increase_capacity(capacity + 128);
			peek = (char)in.get();
			token_type = process_character();
		}
		else if (token_type == SKIP) {
			peek = (char)in.get();
			token_type = process_character();
		}
		else {
			if (token_type == NEWLINE)
				line++;
			break;
		}
	}
	return Token(token_type, std::string(lexem_buffer, lexem_length));
}

int Lexer::get_line_number() {
	return line;
}

void Lexer::reset() {
	line = 1;
	in.clear();
	in.seekg(0);
	peek = (char)in.get();
}