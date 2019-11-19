#ifndef __LEXER
#define __LEXER

#include <istream>

enum TokenType {
	EOI_T, ERROR_T,
	IDENTIFIER_T, LITERAL_T, STRING_T, NEWLINE_T,
	INCLUDE_T, SEGMENTS_T, LABELS_T, COMMENTS_T,
	CODE_T, BYTES_T, WORDS_T, DWORDS_T, DWORDS_BE_T, DWORDS_LE_T,
	RANGE_T, LEFT_PARENTHESES_T, RIGHT_PARENTHESES_T, LEFT_BRACKET_T, RIGHT_BRACKET_T,
	ADD_T, SUBTRACT_T,
	MULTIPLY_T, DIVIDE_T, MODULO_T
};

struct Token {
	int token_type;
	std::string lexem;

	Token(int token_type, std::string lexem) : token_type(token_type), lexem(lexem) {}
	std::string to_string();
};

const Token EOI(EOI_T, "");
const Token INCLUDE(INCLUDE_T, "include:");
const Token SEGMENTS(SEGMENTS_T, "segments:");
const Token LABELS(LABELS_T, "labels:");
const Token COMMENTS(COMMENTS_T, "comments");
const Token CODE(CODE_T, "code");
const Token BYTES(BYTES_T, "bytes");
const Token WORDS(WORDS_T, "words");
const Token DWORDS(DWORDS_T, "dwords");
const Token DWORDS_BE(DWORDS_BE_T, "dwords_be");
const Token DWORDS_LE(DWORDS_LE_T, "dwords_le");
const Token RANGE(RANGE_T, "..");
const Token LEFT_PARENTHESES(LEFT_PARENTHESES_T, "(");
const Token RIGHT_PARENTHESES(RIGHT_PARENTHESES_T, ")");
const Token LEFT_BRACKET(LEFT_BRACKET_T, "[");
const Token RIGHT_BRACKET(RIGHT_BRACKET_T, "]");
const Token ADD(ADD_T, "+");
const Token SUBTRACT(SUBTRACT_T, "-");
const Token MULTIPLY(MULTIPLY_T, "*");
const Token DIVIDE(DIVIDE_T, "/");
const Token MODULO(MODULO_T, "%");

class Lexer {

	std::istream &in;
	int line;
	int state;
	char peek;
	char *lexem_buffer;
	int lexem_length;
	size_t capacity;

	void increase_capacity(size_t new_capacity);

	int process_character(char peek);
public:
	Lexer(std::istream &in, size_t initial_capacity=128);
	~Lexer();

	struct Token next_token();

	int get_line_number();

	void reset();
};

#endif