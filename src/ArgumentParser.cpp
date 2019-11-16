#include "ArgumentParser.h"
#include <stdexcept>
#include <utility>

#define CONSOLE_WIDTH 120
#define ARGUMENT_INDENT "    "
#define ARGUMENT_INTENT_WIDTH 4
#define DESCRIPTION_INDENT "        "
#define DESCRIPTION_INDENT_WIDTH 8

void Argument::parse(std::string args[]) {
	seen = true;
	valid = set_value(args);
}

bool Argument::is_valid() {
	return (!required & !seen) || (seen && valid);
}

/*
Transforms the short argument name to ensure it starts with '-'
*/
std::string validate_short_name(std::string str) {
	if (str.length() > 0) {
		if (str[0] != '-')
			return "-" + str;
		else
			return str;
	}
	throw std::invalid_argument("Argument name cannot be empty");
}

/*
Transforms the long argument name to ensure it starts with '--'
*/
std::string validate_long_name(std::string str) {
	if (str.length() > 1) {
		if (str[0] != '-')
			return "--" + str;
		else if (str[1] != '-')
			return "-" + str;
		else
			return str;
	}
	throw std::invalid_argument("Argument name cannot be empty");
}

ArgumentParser::~ArgumentParser() {
	for (auto it = arguments.begin(); it != arguments.end(); it++) {
		delete it->second;
	}
}

/*
Creates a new argument.
*/
void ArgumentParser::create_argument(
	std::string cmd_short,
	std::string cmd_long,
	std::string desc,
	std::initializer_list<std::string> parameter_list,
	bool(*set_value)(std::string *params),
	bool required
) {
	//Copy parameter names
	int num_parameters = (int) parameter_list.size();
	std::string *parameter_names = new std::string[num_parameters];
	int i = 0;
	for (auto it = parameter_list.begin(); it != parameter_list.end(); it++) {
		parameter_names[i] = *it;
		i++;
	}

	//Create argument
	Argument *arg = new Argument(desc, num_parameters, parameter_names, set_value, required);

	//Validate argument names
	cmd_short = validate_short_name(cmd_short);
	cmd_long = validate_long_name(cmd_long);

	//Add argument names to parser
	short_arguments[cmd_short] = cmd_long;
	arguments[cmd_long] = arg;
}

/*
Print formatted descriptions of the arguments.
*/
void ArgumentParser::print_descriptions(std::ostream &out) {
	for (auto it = short_arguments.begin(); it != short_arguments.end(); it++) {
		std::string short_cmd = it->first;
		std::string long_cmd = it->second;
		Argument *arg = arguments[long_cmd];

		//Print argument names
		if (short_cmd == long_cmd)
			out << ARGUMENT_INDENT << short_cmd;
		else
			out << ARGUMENT_INDENT << short_cmd << ", " << long_cmd;

		//Print parameter names
		for (int i = 0; i < arg->get_num_parameters(); i++) {
			out << ' ' << arg->get_paramter_name(i);
		}
		out << std::endl;

		//Print description
		out << DESCRIPTION_INDENT;
		std::string description = arg->get_description();
		int column = DESCRIPTION_INDENT_WIDTH;
		for (unsigned int i = 0; i < description.length(); i++) {
			if (column == CONSOLE_WIDTH) {		//line wrap if text is too long
				out << std::endl << DESCRIPTION_INDENT;
				column = DESCRIPTION_INDENT_WIDTH;
			}
			if (description[i] == '\n') {		//line wrap if text contains a newline
				out << std::endl << DESCRIPTION_INDENT;
				column = DESCRIPTION_INDENT_WIDTH;
			}
			else {
				out << description[i];
				column++;
			}
		}
		out << std::endl;
	}
}

/*
Parse the argument list and populate Arguments with values.
*/
bool ArgumentParser::parse(int argc, char *argv[]) {
	//Reset 'valid' and 'seen' flags
	for (auto it = arguments.begin(); it != arguments.end(); it++) {
		it->second->reset();
	}

	int arg_index = 1;	//Parse arguments starting at index 1 (0 is the program name)
	while (arg_index < argc) {
		auto short_cmd = short_arguments.find(argv[arg_index]);
		auto long_cmd = arguments.find(argv[arg_index]);
		Argument *arg = nullptr;

		if (short_cmd != short_arguments.end())
			arg = arguments[short_arguments[argv[arg_index]]];
		else if (long_cmd != arguments.end())
			arg = arguments[argv[arg_index]];

		if (arg) {
			int num_parameters = arg->get_num_parameters();
			if (arg_index + num_parameters >= argc)	//Not enough parameters
				return false;

			arg_index++;
			std::string *parameters = new std::string[num_parameters];
			for (int i = 0; i < num_parameters; i++) {
				parameters[i] = argv[arg_index];
				arg_index++;
			}
			arg->parse(parameters);
			delete[] parameters;
		}
		else{	//Stop reading arguments and start reading filenames
			break;
		}
	}

	//add remaining arguments to files list
	files.clear();
	for (; arg_index < argc; arg_index++) {
		files.push_back(argv[arg_index]);
	}

	//Check if argument list was valid
	bool valid = true;
	for (auto it = arguments.begin(); it != arguments.end(); it++) {
		valid &= it->second->is_valid();
	}
	return valid;
}