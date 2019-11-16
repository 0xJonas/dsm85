#include <fstream>
#include <iomanip>
#include <ostream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

#include "Instructions.h"
#include "ArgumentParser.h"
#include "util.h"

#define VERSION_MAJOR 1
#define VERSION_MINOR 0

#define MAX_ADDRESS 0xffff

#define INDENT "    "
#define LABEL_LIMIT 7

//Error codes
#define NO_ERROR 0
#define ERROR_FILE_NOT_FOUND 1
#define ERROR_BAD_ARGUMENTS 2

/*
A single Assembly line, consisting of an address, an instruction and an operand.
*/
struct AssemblyLine {
	const unsigned int address;
	const Instruction *instruction;
	const int operand;

	AssemblyLine(const unsigned int address, const Instruction *instruction, const int operand) :
		address(address), instruction(instruction), operand(operand) {}
};

std::unordered_map<unsigned int, std::string> first_pass_labels(256);
std::unordered_map<unsigned int, std::string> final_labels(256);

std::unordered_map<unsigned int, std::string> *label_output;

std::vector<AssemblyLine> instructions;

// Command line parameters
unsigned int start_address = 0;
unsigned int base_address = MAX_ADDRESS;
unsigned int end_address = MAX_ADDRESS;
unsigned int input_length = MAX_ADDRESS;
bool add_address_column = false;
bool print_help = false;
bool hw_labels = false;
std::string output_file = "";
std::string labels_file = "";

unsigned int current_address = 0;

/*
================================
           READ INPUT
================================
*/

/*
Checks if there is a label pointing to the given address. Since the disassembler uses two lists, both have to be checked.
*/
static bool label_at(unsigned int address) {
	if (first_pass_labels.find(address) != first_pass_labels.end())
		return true;
	else if (final_labels.find(address) != final_labels.end())
		return true;
	else
		return false;
}

/*
Creates a new label to the target address if the given AssemblyLine is BRANCH type instruction.
*/
static void create_label_if_needed(const AssemblyLine &line) {
	if (line.instruction->instruction_type == BRANCH && line.instruction->operand_length > 0) {
		std::string label = "j" + hex16bit(line.operand);
		(*label_output)[line.operand] = label;
	}
}

/*
Checks whether the byte at a given address can be read in as an operand. A byte cannot be an operand if
1) it is outside the range that should be read (as specified by start_address and end_address)
2) there is a label pointing to that address, which means that a new instruction has to start there.
*/
static bool can_read_as_operand(unsigned int address) {
	//start_address and end_address are relative to the input file, while the address parameter is based on base_address.
	if (address<base_address || address>base_address + end_address - start_address)
		return false;
	if (label_at(address))
		return false;
	return true;
}

/*
Adds a data byte pseudo-instruction. 
*/
static void add_data_byte(int byte) {
	AssemblyLine data_byte(current_address, &(instructions8085[DATA_BYTE]), byte);
	instructions.push_back(std::move(data_byte));
	current_address++;
}

/*
Reads a single instruction from the input stream. The instruction may be multiple bytes long, depending on the opcode.
Advances the current_address counter accordingly.
*/
static void read_instruction(std::istream &rom_stream) {
	int address = current_address;

	int opcode = rom_stream.get();
	current_address++;
	const Instruction *ins = &(instructions8085[opcode]);
	int operand = 0;

	if (ins->operand_length == 1) {
		if (!can_read_as_operand(current_address)) {
			//Output incomplete instruction (data byte) if the next byte could not be read as an operand
			add_data_byte(opcode);
			return;
		}
		else {
			operand = rom_stream.get();
			current_address++;
		}
	}
	else if (ins->operand_length == 2) {
		//Read first operand byte
		if (!can_read_as_operand(current_address)) {
			//Output incomplete instruction
			add_data_byte(opcode);
			return;
		}
		else {
			//first (least significant) byte of a two byte operand
			operand = rom_stream.get();
			current_address++;
		}

		//Read second operand byte
		if (!can_read_as_operand(current_address)) {
			//Output two incomplete instructions 
			add_data_byte(opcode);
			add_data_byte(operand);
			return;
		}
		else {
			//second (most significant) byte of a two byte operand
			operand |= rom_stream.get() << 8;
			current_address++;
		}
	}

	//Create AssemblyLine and add label
	AssemblyLine line(address, ins, operand);
	create_label_if_needed(line);

	instructions.push_back(std::move(line));
}

/*
Do a single pass over the input file, creating AssemblyLines and labels.
*/
static void single_pass(std::istream &rom_stream) {
	instructions.clear();

	//Set stream pointer to start address
	rom_stream.clear();
	rom_stream.seekg(start_address);

	//Read instructions
	current_address = base_address;
	while (!rom_stream.eof() && rom_stream.tellg()<=end_address) {
		read_instruction(rom_stream);
	}
}

/*
================================
		  WRITE OUTPUT
================================
*/

/*
Writes the operand of an AssemblyLine. If the operand is an address for which a label exist, the label is printed.
*/
static void write_operand(const AssemblyLine &line, std::ostream &listing_stream) {
	if (line.instruction->operand_type == ADDRESS) {
		if (final_labels.find(line.operand) != final_labels.end())	//Print label
			listing_stream << final_labels[line.operand];
		else
			listing_stream << "$" << hex16bit(line.operand);
	}
	if (line.instruction->operand_type == IMMEDIATE) {
		if (line.instruction->operand_length == 2)
			listing_stream << "#" << hex16bit(line.operand);
		else
			listing_stream << "#" << hex8bit(line.operand);
	}

}

/*
Writes a single AssemblyLine to the output stream.
*/
static void write_assembly_line(const AssemblyLine &line, std::ostream &listing_stream) {
	//Write address column
	if (add_address_column)
		listing_stream << '$' << hex16bit(line.address) << INDENT;

	//Write label
	if (final_labels.find(line.address) != final_labels.end()) {
		std::string label = final_labels[line.address];
		listing_stream << label << ":";
		if (label.length() > LABEL_LIMIT) {	//put assembly directive on the next line if label is too long
			listing_stream << std::endl;
			listing_stream << (add_address_column ? "     " : "") << INDENT << INDENT << INDENT;
		}
		else {
			listing_stream << std::string(LABEL_LIMIT-label.length(),' ');
		}
	}
	else
		listing_stream << INDENT << INDENT;

	//Write instruction mnemonic
	listing_stream << line.instruction->mnemonic;

	//Write operand
	if (line.instruction->operand_length > 0) {
		write_operand(line, listing_stream);
	}

	listing_stream << std::endl;

	//Add extra newline after RET instruction
	if (line.instruction->opcode == 0xc9)
		listing_stream << std::endl;
}

/*
Writes the output assembly listing to the stream.
*/
static void write_listing(std::ostream &listing_stream) {
	for (unsigned int i = 0; i < instructions.size(); i++) {
		AssemblyLine line = instructions[i];
		write_assembly_line(line, listing_stream);
	}
}

/*
=================================
              MAIN
=================================
*/

/*
Creates labels for 8085 interrupt vectors.
*/
static void add_interrupt_labels() {
	final_labels[0x00] = "rst0";
	final_labels[0x08] = "rst1";
	final_labels[0x10] = "rst2";
	final_labels[0x18] = "rst3";
	final_labels[0x20] = "rst4";
	final_labels[0x24] = "trap";
	final_labels[0x28] = "rst5";
	final_labels[0x2c] = "rst55";
	final_labels[0x30] = "rst6";
	final_labels[0x34] = "rst65";
	final_labels[0x38] = "rst7";
	final_labels[0x3c] = "rst75";
}

static void print_version() {
	std::cout << "8085 DSM version " << VERSION_MAJOR << "." << VERSION_MINOR << std::endl;
}

/*
Helper function to set an integer argument with a literal. Basically a wrapper to invalidate the argument if an excpetion occured during conversion.
*/
static inline bool set_int_argument(unsigned int &arg, std::string value) {
	try {
		arg = parse_int_literal(value);
		return true;
	}
	catch (std::invalid_argument) {
		return false;
	}
}

int main(int argc, char *argv[]) {
	//Setup ArgumentParser
	ArgumentParser parser;
	parser.create_argument(
		"-h", "--help",
		"Display this text.",
		{},
		[](std::string *params) -> bool {(void)params; print_help = true; return true; }
	);
	parser.create_argument(
		"-o", "--output",
		"Name of the file to write the disassembly to. If neither an output file nor --stdout is given,\nthe output will be written to [input file name].lst",
		{"file"},
		[](std::string *params) -> bool {output_file = params[0];  return true; }
	);
	/*parser.create_argument(
		"-l", "--labels",
		"Load labels from file.",
		{ "file" },
		[](std::string *params) -> bool {label_file = params[0];  return true; }
	);*/
	parser.create_argument(
		"-a", "--address",
		"Add an address column to the disassembly.",
		{},
		[](std::string *params) -> bool {(void)params; add_address_column = true; return true; }
	);
	parser.create_argument(
		"-s", "--start",
		"Sets the starting address for the disassembly. Defaults to 0000h.",
		{ "address" },
		[](std::string *params) -> bool { return set_int_argument(start_address, params[0]); }
	);
	parser.create_argument(
		"-n", "--length",
		"Sets the number of bytes to be read from the input file. If both -n and -e are given,\n-e takes priority.",
		{ "address" },
		[](std::string *params) -> bool { return set_int_argument(input_length, params[0]); }
	);
	parser.create_argument(
		"-b", "--base",
		"Sets the base address for the disassembly. Defaults to start address.",
		{ "address" },
		[](std::string *params) -> bool { return set_int_argument(base_address, params[0]); }
	);
	parser.create_argument(
		"-e", "--end",
		"Sets the ending address for the disassembly. Defaults to the length of the input file.",
		{ "address" },
		[](std::string *params) -> bool { return set_int_argument(end_address, params[0]); }
	);
	parser.create_argument(
		"-hw", "--hwlabels",
		"Create labels for 8085 interrupt vectors. These labels take precedence over user-defined labels.",
		{},
		[](std::string *params) -> bool {(void)params; hw_labels = true; return true; }
	);

	//Read arguments
	bool successfully_parsed = parser.parse(argc, argv);
	
	//Print help & exit if something went wrong or -h was used.
	if (argc <= 1 
		|| print_help
		|| !successfully_parsed
		|| parser.files.size()!=1) {
		print_version();
		parser.print_descriptions(std::cout);
		return print_help ? NO_ERROR : ERROR_BAD_ARGUMENTS;
	}

	//Resolve dependencies between arguments.
	//the default values of base_address and end_address depend on other arguments
	if (base_address == MAX_ADDRESS)
		base_address = start_address;
	if (end_address == MAX_ADDRESS && input_length != MAX_ADDRESS)
		end_address = start_address + input_length - 1;
	
	//Create input & output streams
	std::string input_file = parser.files[0];
	std::ifstream rom_stream(input_file,std::ios_base::in | std::ios_base::binary);
	if (rom_stream.fail()) {
		std::cerr << "Error: File not found: " << input_file << std::endl;
		return ERROR_FILE_NOT_FOUND;
	}

	std::ofstream listing_stream(output_file, std::ios_base::out);

	//add labels for interrupt vectors
	if(hw_labels)
		add_interrupt_labels();

	//load user labels into final labels

	//First pass
	label_output = &first_pass_labels;
	single_pass(rom_stream);

	//Second pass
	label_output = &final_labels;
	single_pass(rom_stream);

	//Write final listing
	write_listing(listing_stream);

	//Clean up
	rom_stream.close();
	listing_stream.close();
}
