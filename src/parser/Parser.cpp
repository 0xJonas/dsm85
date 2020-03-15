#include "Parser.h"
#include <iostream>
#include <fstream>
#include <string>
#include "../util.h"

int SymbolTable::get_symbol_value(std::string symbol) {
	if (symbols.find(symbol) != symbols.end())
		return symbols[symbol];
	else
		throw std::invalid_argument("Cannot find symbol: " + symbol);
}

void SymbolTable::add_symbol(std::string symbol, int value) {
	symbols[symbol] = value;
}

void Parser::error(std::string error_message) {
	std::cerr << "Error in file " << source << ", at line " << lexer.get_line_number() << ":" << std::endl;
	std::cerr << "\t" << error_message << std::endl;
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

void Parser::match_newline() {
	if (peek.token_type == NEWLINE)
		match(NEWLINE);
	else if (peek.token_type == EOI)
		return;
	else
		error("Line break expected.");
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
	symbol_table.enter_source_file(source);

	skip_blank_lines();
	while (peek.token_type != EOI) {
		section();
	}

	symbol_table.leave_source_file();
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
		std::string filename = match(STRING).lexem;
		if (symbol_table.is_source_file_loaded(filename))
			error("Recursive file inclusion: " + filename);
		else {
			std::ifstream in(filename, std::ios_base::in | std::ios_base::binary);
			Parser sub_parser(in, filename, symbol_table, info);
			sub_parser.file();
			in.close();
		}
		match_newline();
		skip_blank_lines();
	}
}

void Parser::segments_section() {
	while (peek.token_type == LITERAL 
		|| peek.token_type == LEFT_PARENTHESES 
		|| peek.token_type == IDENTIFIER)
	{
		std::pair<unsigned int,unsigned int> target = label_target();
		if (target.first == target.second)
			error("Segments can not be defined by a single address.");

		data_type type = CODE_T;
		if (peek.token_type != IDENTIFIER)
			type = read_data_type();

		std::string identifier = consume().lexem;

		match_newline();
		skip_blank_lines();

		info.add_segment(identifier, type, target.first, target.second);
		symbol_table.add_symbol(identifier, target.first);
	}
}

void Parser::labels_section() {
	while (peek.token_type == LITERAL
		|| peek.token_type == LEFT_PARENTHESES
		|| peek.token_type == IDENTIFIER)
	{
		std::pair<unsigned int, unsigned int> target = label_target();

		data_type type = UNDEFINED_T;
		if (peek.token_type != IDENTIFIER)
			type = read_data_type();

		std::string identifier = consume().lexem;

		match_newline();
		skip_blank_lines();

		if (target.first != target.second)
			info.add_range_label(identifier, target.first, target.second, type);
		else
			info.add_label(identifier, target.first, type);

		symbol_table.add_symbol(identifier, target.first);
	}
}

void Parser::comments_section() {
	while (peek.token_type == LITERAL
		|| peek.token_type == LEFT_PARENTHESES
		|| peek.token_type == IDENTIFIER)
	{
		int address = address_expr();
		std::string comment = match(STRING).lexem;

		match_newline();
		skip_blank_lines();

		info.add_comment(comment, address);
	}
}

std::pair<unsigned int,unsigned int> Parser::label_target() {
	int start = address_expr();
	int end = start;

	if (peek.token_type == RANGE) {
		consume();
		end = address_expr();
	}
	else if (peek.token_type == LEFT_PARENTHESES) {
		consume();
		int length = address_expr();
		if (length < 0)
			error("Range length is negative.");

		end = start + length - 1;
		match(RIGHT_PARENTHESES, "Unbalanced parentheses.");
	}

	if (end < start) {
		unsigned int temp = start;
		start = end;
		end = temp;
	}

	if (start < 0) {
		if (start == end)
			error("Address is negative.");
		else
			error("Start address is negative.");
	}
	else if (end < 0)
		error("End address is negative.");

	return std::pair<unsigned int, unsigned int>((unsigned int) start, (unsigned int) end);
}

data_type Parser::read_data_type() {
	switch (consume().token_type) {
	case CODE:
		return CODE_T;
	case BYTES:
	case WORDS:
		return BYTES_T;
	case DWORDS:
		return DWORDS_LE_T;
	case DWORDS_BE:
		return DWORDS_BE_T;
	case DWORDS_LE:
		return DWORDS_LE_T;
	case TEXT:
		return TEXT_T;
	case RET:
		return RET_T;
	}
	error("Identifier expected.");
	return UNDEFINED_T;
}

int Parser::address_expr() {
	int sum = address_product();
	do {
		switch (peek.token_type) {
		case ADD:
			consume();
			sum += address_product();
			break;
		case SUBTRACT:
			consume();
			sum -= address_product();
			break;
		default:
			return sum;
		}
	} while (true);
}

int Parser::address_product() {
	int product = single_address();
	do {
		int divisor = 0;
		switch (peek.token_type) {
		case MULTIPLY:
			consume();
			product *= single_address();
			break;
		case DIVIDE:
			consume();
			divisor = single_address();
			if (divisor == 0)
				error("Division by zero.");
			product /= divisor;
			break;
		case MODULO:
			consume();
			divisor = single_address();
			if (divisor == 0)
				error("Division by zero.");
			product %= divisor;
			break;
		default:
			return product;
		}
	} while (true);
}

int Parser::single_address() {
	std::string lexem = peek.lexem;
	int val = 0;
	switch (consume().token_type) {
	case SUBTRACT:
		val = -address_expr();
		return val;
	case LEFT_PARENTHESES:
		val = address_expr();
		match(RIGHT_PARENTHESES, "Unbalanced parentheses.");
		return val;
	case LITERAL:
		try {
			return parse_int_literal(lexem);
		}
		catch (std::invalid_argument) {
			error("Invalid integer literal: " + lexem);
			return 0;
		}
	case IDENTIFIER:
		try {
			return symbol_table.get_symbol_value(lexem);
		}
		catch (std::invalid_argument) {
			error("Cannot find symbol: " + lexem);
			return 0;
		}
	default:
		error("Address literal or identifier expected.");
		return 0;
	}
}

void Parser::parse(std::istream &in, std::string source, DSMInfo &info) {
	SymbolTable symbol_table;
	Parser parser(in, source, symbol_table, info);
	parser.file();
}