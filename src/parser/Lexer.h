#ifndef LEXER_H
#define LEXER_H

#include <istream>

enum TokenType {
	EOI, ERROR,
	IDENTIFIER, LITERAL, STRING, NEWLINE,
	INCLUDE, SEGMENTS, LABELS, COMMENTS,
	CODE, BYTES, WORDS, DWORDS, DWORDS_BE, DWORDS_LE, TEXT, RET,
	RANGE, LEFT_PARENTHESES, RIGHT_PARENTHESES, LEFT_BRACKET, RIGHT_BRACKET,
	ADD, SUBTRACT,
	MULTIPLY, DIVIDE, MODULO
};

struct Token {
	int token_type;
	std::string lexem;

	Token(int token_type, std::string lexem) : token_type(token_type), lexem(lexem) {}
	std::string to_string();
};

class Lexer {

	std::istream &in;
	int line;
	int state;
	char peek;
	char *lexem_buffer;
	size_t lexem_length;
	size_t capacity;

	void increase_capacity(size_t new_capacity);

	int process_character();
public:
	Lexer(std::istream &in, size_t initial_capacity=128);
	~Lexer();

	struct Token next_token();

	int get_line_number();

	void reset();
};

#endif