#ifndef PARSER_H
#define PARSER_H

#include <istream>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include "Lexer.h"
#include "../DSMInfo.h"

/*
Label file grammar:

<file>    ::= (<section>)*
<section> ::= 'include:' newline <include-section>
		    | 'segments:' newline <segments-section>
		    | 'labels:' newline <labels-section>
		    | 'comments:' newline <comments-setction>

<include-section>  ::= (string newline)*
<segments-section> ::= (<label-target> <data-type>? identifier newline)*
<labels-section>   ::= (<label-target> <data-type>? identifier newline)*
<comments-section> ::= (<address-expr string newline)*

<label-target> ::= <address-expr>
				 | <address-expr> '..' <address-expr>
			     | <address-expr> '(' <address-expr> ')'

<data-type> ::= 'code' | 'bytes' | 'words' | 'dwords' | 'dwords_le' | 'dwords_be' | 'text' | 'ret'

<address-expr>    ::= <address-product> (('+' | '-') <address-product>)*
<address-product> ::= <single-address> (('*' | '/' | '%') <single-address>)*
<single-address>  ::= '-' <address-expr>
				    | '(' <address-expr> ')'
				    | literal
				    | identifier
*/

class parse_error : public std::exception {

public:
   const char * what() const noexcept override {
		return (char *) "Parsing error!";
	}
};

class SymbolTable {

	std::vector<std::string> source_files;
	std::unordered_map<std::string, int> symbols;

public:
	int get_symbol_value(std::string identifier);
	void add_symbol(std::string symbol, int value);

	void enter_source_file(std::string source) {
		source_files.push_back(source);
	}

	void leave_source_file() {
		source_files.pop_back();
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
	DSMInfo &info;

	Lexer lexer;
	Token peek = Token(EOI, "");

	SymbolTable &symbol_table;

	void file();
	void section();
	void include_section();
	void segments_section();
	void labels_section();
	data_type read_data_type();
	void comments_section();
	std::pair<unsigned int, unsigned int> label_target();
	int address_expr();
	int address_product();
	int single_address();

	void error(std::string error_message);
	Token match(int token_type, std::string error = "");
	void match_newline();
	Token consume();
	void skip_blank_lines();

	Parser(std::istream &in, std::string source, SymbolTable &symbol_table, DSMInfo &info)
		: source(source),
		  info(info),
		  lexer(Lexer(in)),
		  symbol_table(symbol_table)
	{
		peek = lexer.next_token();
	}

public:
	static void parse(std::istream &in,
		std::string source,
		DSMInfo &info);
};

#endif
