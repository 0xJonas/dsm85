#ifndef PARSER_H
#define PARSER_H

#include <istream>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include "Lexer.h"

class parse_error : public std::exception {

public:
	virtual char *what() {
		return (char *) "Parsing error!";
	}
};

class SymbolTable {

	std::vector<std::string> source_files;
	std::unordered_map<std::string, int> symbols;

public:
	int get_symbol_value(std::string identifier);
	void add_symbol(std::string symbol, int value);

	void add_source_file(std::string source) {
		source_files.push_back(source);
	}

	bool is_source_file_loaded(std::string source) {
		for (std::string s : source_files) {
			if (s == source)
				return true;
		}
		return false;
	}
};

class Parser {

	std::string source;
	Lexer lexer;
	Token peek = Token(EOI, "");

	SymbolTable &symbol_table;
	std::unordered_map<unsigned int, std::string> *label_output;

	void file();
	void section();
	void include_section();
	void segments_section();
	void labels_section();
	void data_type();
	void comments_section();
	void label_target();
	int address_expr();
	int address_product();
	int single_address();

	void error(std::string error_message);
	Token match(int token_type, std::string error = "");
	Token consume();
	void skip_blank_lines();

	Parser(std::istream &in, std::string source, SymbolTable &symbol_table, std::unordered_map<unsigned int, std::string> *label_output)
		: lexer(Lexer(in)),
		  source(source),
		  symbol_table(symbol_table),
		  label_output(label_output)
	{
		peek = lexer.next_token();
		symbol_table.add_source_file(source);
	}

public:
	static void parse(std::istream &in,
		std::string source,
		std::unordered_map<unsigned int, std::string> *label_output);
};

#endif