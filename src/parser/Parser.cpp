#include "Parser.h"
#include <iostream>
#include <string>

void Parser::error(std::string error_message) {
	std::cerr << "Error in line " << lexer.get_line_number() << ": " << error_message << std::endl;
	//TODO print line in question and column indicator
	throw parse_error();
}

Token Parser::match(int token_type, std::string error_message) {
	if (peek.token_type == token_type) {
		Token retval = peek;
		peek = lexer.next_token();
		return retval;
	}
	else {
		error(error_message);
	}
	return Token(EOI, "");
}

Token Parser::consume() {
	Token retval = peek;
	peek = lexer.next_token();
	return retval;
}

void Parser::skip_blank_lines() {
	while (peek.token_type == NEWLINE)
		consume();
}

void Parser::file() {
	while (peek.token_type != EOI) {
		section();
	}
}

void Parser::section() {
	switch (consume().token_type) {
	case INCLUDE:
		skip_blank_lines();
		include_section();
		break;
	case SEGMENTS:
		skip_blank_lines();
		segments_section();
		break;
	case LABELS:
		skip_blank_lines();
		labels_section();
		break;
	case COMMENTS:
		skip_blank_lines();
		comments_section();
		break;
	}
}

void Parser::include_section() {
	while (peek.token_type == STRING) {
		match(STRING);
		match(NEWLINE, "Line break expected.");
		skip_blank_lines();
	}
}

void Parser::segments_section() {
	while (peek.token_type == LITERAL 
		|| peek.token_type == LEFT_PARENTHESES 
		|| peek.token_type == IDENTIFIER) {
		label_target();
		if (peek.token_type != IDENTIFIER)
			data_type();
		match(IDENTIFIER);	//TODO: fitting error message
		match(NEWLINE, "Line break expected.");
		skip_blank_lines();
	}
}

void Parser::labels_section() {
	while (peek.token_type == LITERAL
		|| peek.token_type == LEFT_PARENTHESES
		|| peek.token_type == IDENTIFIER) {
		label_target();
		if (peek.token_type != IDENTIFIER)
			data_type();
		match(IDENTIFIER);	//TODO: fitting error message
		match(NEWLINE, "Line break expected.");
		skip_blank_lines();
	}
}

void Parser::comments_section() {
	while (peek.token_type == LITERAL
		|| peek.token_type == LEFT_PARENTHESES
		|| peek.token_type == IDENTIFIER) {
		address_expr();
		match(STRING);
		match(NEWLINE, "Line break expected.");
		skip_blank_lines();
	}
}

void Parser::label_target() {
	address_expr();

	if (peek.token_type == RANGE) {
		consume();
		address_expr();
	}
	else if (peek.token_type == LEFT_PARENTHESES) {
		consume();
		address_expr();
		match(RIGHT_PARENTHESES, "Unbalances parentheses.");
	}
}

void Parser::data_type() {
	switch (consume().token_type) {
	case CODE:

		break;
	case BYTES:
	case WORDS:

		break;
	case DWORDS:

		break;
	case DWORDS_BE:

		break;
	case DWORDS_LE:

		break;
	}
}

void Parser::address_expr() {
	if (peek.token_type == LEFT_PARENTHESES) {
		consume();
		address_expr();
		match(RIGHT_PARENTHESES, "Unbalances parentheses.");
		return;
	}

	do {
		address_product();
		switch (consume().token_type) {
		case ADD:

			break;
		case SUBTRACT:

			break;
		default:
			return;
		}
	} while (true);
}

void Parser::address_product() {
	do {
		single_address();
		switch (consume().token_type) {
		case MULTIPLY:
			
			break;
		case DIVIDE:
			
			break;
		case MODULO:
			
			break;
		default:
			return;
		}
	} while (true);
}

void Parser::single_address() {
	switch (consume().token_type) {
	case LITERAL:
		
		break;
	case IDENTIFIER:

		break;
	default:
		error("Address literal or identifier expected.");
	}
}