#ifndef __ARGUMENT_PARSER
#define __ARGUMENT_PARSER

#include <string>
#include <map>
#include <unordered_map>
#include <vector>

class Argument {

	std::string description;
	int num_parameters;
	std::string *parameter_names;
	bool(*set_value)(std::string *params);
	bool required;
	bool valid;
	bool seen;

public:
	Argument(
		std::string desc,
		int num_parameters,
		std::string *parameter_names,
		bool(*set_value)(std::string *params),
		bool required = false
	) : description(desc), num_parameters(num_parameters), parameter_names(parameter_names), set_value(set_value), required(required) {}

	~Argument() {
		delete[] parameter_names;
	}

	void reset() {
		seen = false;
		valid = false;
	}

	std::string get_description() {
		return description;
	}

	int get_num_parameters() {
		return num_parameters;
	}

	std::string get_paramter_name(int i) {
		return parameter_names[i];
	}

	bool is_valid();

	void parse(std::string values[]);
};

class ArgumentParser {

	std::map<std::string, Argument *> arguments;

	std::unordered_map<std::string, std::string> short_arguments;

public:

	std::vector<std::string> files;

	~ArgumentParser();

	void create_argument(
		std::string cmd_short, 
		std::string cmd_long, 
		std::string desc,
		std::initializer_list<std::string> parameter_names,
		bool(*set_value)(std::string *params),
		bool required=false
	);

	void print_descriptions(std::ostream &stream);

	bool parse(int argc, char *argv[]);
};

#endif