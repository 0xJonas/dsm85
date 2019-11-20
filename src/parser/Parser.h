#ifndef __PARSER
#define __PARSER

#include <istream>
#include <stdexcept>
#include "Lexer.h"

class parse_error : public std::exception {

public:
	virtual char *what() {
		return (char *) "Parsing error!";
	}
};

class Parser {

	Lexer lexer;
	Token peek;

	void file();
	void section();
	void include_section();
	void segments_section();
	void labels_section();
	void data_type();
	void comments_section();
	void label_target();
	void address_expr();
	void address_product();
	void single_address();

	void error(std::string error_message);
	Token match(int token_type, std::string error = "");
	Token consume();
	void skip_blank_lines();

public:
	Parser(std::istream in) : lexer(Lexer(in)), peek(Token(EOI, "")){
		peek = lexer.next_token();
	}

	void parse();
};

#endif