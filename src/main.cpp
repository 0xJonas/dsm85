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
#include "parser/Parser.h"
#include "DSMInfo.h"

#define VERSION_MAJOR 1
#define VERSION_MINOR 0

#define MAX_ADDRESS 0xffff

#define INDENT "    "
#define LABEL_LIMIT 7

//Error codes
#define NO_ERROR 0
#define ERROR_FILE_NOT_FOUND 1
#define ERROR_BAD_ARGUMENTS 2
#define ERROR_BAD_LABEL_FILE 3

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

std::unordered_map<unsigned int, std::string> first_pass_labels;
std::unordered_map<unsigned int, std::string> second_pass_labels;

std::unordered_map<unsigned int, std::string> *label_output;

DSMInfo info;

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

unsigned int data_instruction_streak = 0;

/*
================================
           READ INPUT
================================
*/

/*
Checks if there is a label pointing to the given address. Since the disassembler uses two lists, both have to be checked.
*/
static bool jump_label_at(unsigned int address) {
	return (info.label_at(address) && info.get_label(address)->jump_label)
		|| first_pass_labels.find(address) != first_pass_labels.end()
		|| second_pass_labels.find(address) != second_pass_labels.end();
}

/*
Creates a new label to the target address if the given AssemblyLine is BRANCH type instruction.
*/
static void create_label_if_needed(const AssemblyLine &line) {
	if (line.instruction->instruction_type == BRANCH 
		&& line.instruction->operand_length > 0
		&& label_output->find(line.operand) == label_output->end())
	{
		std::string label = "j" + hex16bit(line.operand);
		(*label_output)[line.operand] = label;
	}
}

/*
Checks whether the byte at a given address can be read in as an operand. A byte cannot be an operand if
1) it is outside the range that should be read (as specified by start_address and end_address)
2) there is a jump label pointing to that address, which means that a new instruction has to start there.
3) a new segment starts at that address
4) the instruction has a comment
*/
static bool can_read_as_operand(unsigned int address) {
	//start_address and end_address are relative to the input file, while the address parameter is based on base_address.
	if (address<base_address || address>base_address + end_address - start_address)
		return false;
	if (jump_label_at(address))
		return false;
	if (info.is_segment_start())
		return false;
	if (info.has_comment())
		return false;
	return true;
}

/*
Adds a pseudo-instruction. 
*/
static void add_data_instruction(int instruction, int address, int data) {
	AssemblyLine pseudo(address, &(instructions8085[instruction]), data);
	instructions.push_back(std::move(pseudo));
}

/*
Fetches a single byte from the input stream, increments address counter, advances final_labels DSMInfo instance.
*/
static int fetch_byte(std::istream &rom_stream) {
	current_address++;
	info.advance();
	return rom_stream.get();
}

/*
Reads a single instruction from the input stream. The instruction may be multiple bytes long, depending on the opcode.
Advances the current_address counter accordingly.
*/
static void read_code_instruction(std::istream &rom_stream) {
	int address = current_address;
	bool segment_end = info.is_segment_end();

	int opcode = fetch_byte(rom_stream);
	const Instruction *ins = &(instructions8085[opcode]);
	int operand = 0;

	if (ins->operand_length == 1) {
		if (!can_read_as_operand(current_address) || segment_end) {
			//Output incomplete instruction (data byte) if the next byte could not be read as an operand
			add_data_instruction(DATA_BYTE, address, opcode);
			return;
		}
		else {
			operand = fetch_byte(rom_stream);
		}
	}
	else if (ins->operand_length == 2) {
		//Read first operand byte
		if (!can_read_as_operand(current_address) || segment_end) {
			//Output incomplete instruction
			add_data_instruction(DATA_BYTE, address, opcode);
			return;
		}
		else {
			//Check if segment ends on the first of the two operand bytes
			segment_end = info.is_segment_end();

			//first (least significant) byte of a two byte operand
			operand = fetch_byte(rom_stream);
		}

		//Read second operand byte
		if (!can_read_as_operand(current_address) || segment_end) {
			//Output two incomplete instructions 
			add_data_instruction(DATA_BYTE, address, opcode);
			add_data_instruction(DATA_BYTE, address + 1, opcode);
			return;
		}
		else {
			//second (most significant) byte of a two byte operand
			operand |= fetch_byte(rom_stream) << 8;
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
	info.reset(base_address);

	//Set stream pointer to start address
	rom_stream.clear();
	rom_stream.seekg(start_address);

	//Read instructions
	current_address = base_address;
	while (!rom_stream.eof() && rom_stream.tellg() <= end_address) {
		int address = current_address;
		int data = 0;

		switch (info.get_data_type()) {
		case CODE_T:
			read_code_instruction(rom_stream);
			break;
		case BYTES_T:
			data = fetch_byte(rom_stream);
			add_data_instruction(DATA_BYTE, address, data);
			break;
		case DWORDS_BE_T:
			data = fetch_byte(rom_stream);
			if (info.get_data_type() == DWORDS_BE_T) {
				data = (data << 8) | (fetch_byte(rom_stream) & 0xff);
				add_data_instruction(DATA_WORD, address, data);
			}
			else {
				add_data_instruction(DATA_BYTE, address, data);
			}
			break;
		case DWORDS_LE_T:
			data = fetch_byte(rom_stream);
			if (info.get_data_type() == DWORDS_LE_T) {
				data = (fetch_byte(rom_stream) << 8) | (data & 0xff);
				add_data_instruction(DATA_WORD, address, data);
			}
			else {
				add_data_instruction(DATA_BYTE, address, data);
			}
			break;
		case TEXT_T:
			data = fetch_byte(rom_stream);
			add_data_instruction(DATA_TEXT, address, data);
			break;
		case RET_T:
			data = fetch_byte(rom_stream);
			if (info.get_data_type() == RET_T) {
				data = (fetch_byte(rom_stream) << 8) | (data & 0xff);
				add_data_instruction(DATA_RET, address, data);
				IndirectLabel *il = (IndirectLabel *) info.get_label(address);
				(*label_output)[data] = il->get_jump_target_name(address) + "[" + std::to_string(il->get_offset()) + "]";
				(*label_output)[il->start_address] = il->get_jump_target_name(address);
			}
			else {
				add_data_instruction(DATA_BYTE, address, data);
			}
			break;
		default:
			//Should never happen
			std::cerr << "Error: info.get_data_type() returned undefined type (UNDEFINED_T)" << std::endl;
		}
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
	Label *label = nullptr;
	switch (line.instruction->operand_type) {
	case ADDRESS: 
		label = info.get_label(line.operand);
		if (label)	//Print label
			listing_stream << label->get_operand_name(line.operand);
		else
			listing_stream << "$" << hex16bit(line.operand);
		break;
	case IMMEDIATE_HYBRID:
		label = info.get_label(line.operand);
		if (label)	//Print label
			listing_stream << label->get_operand_name(line.operand) << '(';

		if (line.instruction->operand_length == 2)
			listing_stream << "#" << hex16bit(line.operand);
		else
			listing_stream << "#" << hex8bit(line.operand);

		if (label)
			listing_stream << ')';
		break;
	case IMMEDIATE:
		if (line.instruction->operand_length == 2)
			listing_stream << "#" << hex16bit(line.operand);
		else
			listing_stream << "#" << hex8bit(line.operand);
		break;
	case CHARACTER:
		listing_stream << (char)line.operand;
	}
}

/*
Writes the start of a segment.
*/
static void write_segment_start(Segment *segment, std::ostream &listing_stream) {
	listing_stream << std::endl << std::endl;
	listing_stream << "=== Start of " << segment->name << " ===";
}

/*
Writes the end of a segment.
*/
static void write_segment_end(Segment *segment, std::ostream &listing_stream) {
	listing_stream << std::endl;
	listing_stream << "=== End of " << segment->name << " ===" << std::endl;
}

/*
Writes the address column
*/
static void write_address_column(const AssemblyLine &line, std::ostream &listing_stream) {
	listing_stream << '$' << hex16bit(line.address) << INDENT;
}

/*
Writes a jump label.
*/
static void write_jump_label(std::string name, std::ostream &listing_stream) {
	listing_stream << name << ":";
	if (name.length() > LABEL_LIMIT) {	//put assembly directive on the next line if label is too long
		listing_stream << std::endl;
		listing_stream << (add_address_column ? "     " : "") << INDENT << INDENT << INDENT;
	}
	else {
		listing_stream << std::string(LABEL_LIMIT - name.length(), ' ');
	}
}

/*
Writes a single AssemblyLine to the output stream.
*/
static void write_code_line(const AssemblyLine &line, std::ostream &listing_stream) {
	//start new line
	listing_stream << std::endl;

	if (add_address_column)
		write_address_column(line, listing_stream);

	//Write label
	Label *label = info.get_label(line.address);
	if (label && label->jump_label) {
		std::string name = label->get_jump_target_name(line.address);
		write_jump_label(name, listing_stream);
	}
	else
		listing_stream << INDENT << INDENT;

	//Write instruction mnemonic
	listing_stream << line.instruction->mnemonic;

	//Write operand
	if (line.instruction->operand_length > 0) {
		write_operand(line, listing_stream);
	}

	//Write comment
	if (info.has_comment())
		listing_stream << INDENT << ";" << info.get_comment()->text;

	//Add extra newline after RET instruction
	if (line.instruction->opcode == 0xc9)
		listing_stream << std::endl;
}

/*
Successive pseudo instructions (DATA_BYTE, DATA_WORD and TEXT) are merged to aid readibility and to preserve space. This function writes
the first part of a pseudo instruction to the output stream.
*/
static void start_data_instruction(const AssemblyLine &line, std::ostream &listing_stream) {
	//Create a new line
	listing_stream << std::endl;

	//add address collumn
	if (add_address_column)
		write_address_column(line, listing_stream);

	//Write label
	Label *label = info.get_label(line.address);
	if (label && label->jump_label) {
		std::string name = label->get_jump_target_name(line.address);
		write_jump_label(name, listing_stream);
	}
	else
		listing_stream << INDENT << INDENT;

	//Write instruction mnemonic
	listing_stream << line.instruction->mnemonic;

	//Write operand
	if (line.instruction->opcode == DATA_RET)	//DATA_RET should always print an address
		listing_stream << "$" << hex16bit(line.operand);
	else
		write_operand(line, listing_stream);
}

/*
This function writes a data instruction that is not the first data instruction of the current line.
*/
static void continue_data_instruction(const AssemblyLine &line, std::ostream &listing_stream) {
	if(line.instruction->opcode != DATA_TEXT)
		listing_stream << ",";

	if(line.instruction->opcode == DATA_RET)
		listing_stream << "$" << hex16bit(line.operand);
	else
		write_operand(line, listing_stream);
}

/*
Writes a data instruction to the output stream. Data instructions are handled differently from code instructions, in 
that successive data instructions are merged together. This function transparently takes care of that.
*/
static void write_data_instruction(const AssemblyLine &line, std::ostream &listing_stream) {
	static int prev_opcode = -1;

	//Start a new line if the type of data instruction switched (e.g. from DATA_BYTE to DATA_WORD)
	if (line.instruction->opcode != prev_opcode)
		data_instruction_streak = 0;

	//Start a new line if the current instruction has a label pointing to it
	if (jump_label_at(line.address))
		data_instruction_streak = 0;

	//Start a new line if the next instruction is the start of a new segment
	if (info.is_segment_start())
		data_instruction_streak = 0;

	//Start a new line or continue an existing one depending on the previous instructions
	if (data_instruction_streak == 0) {
		start_data_instruction(line, listing_stream);
	}
	else {
		continue_data_instruction(line, listing_stream);
	}

	data_instruction_streak++;

	//Only write a maximum of 8 data instructions on a single line, unless its text in which case we never stop
	if (data_instruction_streak >= 8 && line.instruction->opcode != DATA_TEXT)
		data_instruction_streak = 0;

	//If the current line has a comment, the line has to end prematurely
	if (info.has_comment()) {
		listing_stream << INDENT << ";" << info.get_comment()->text;
		data_instruction_streak = 0;
	}

	//End the current line if it is the last instruction of a segment
	if (info.is_segment_end())
		data_instruction_streak = 0;

	prev_opcode = line.instruction->opcode;
}

/*
Writes the output assembly listing to the stream.
*/
static void write_listing(std::ostream &listing_stream) {
	info.reset(base_address);
	for (unsigned int i = 0; i < instructions.size(); i++) {
		//Write segment header
		if (info.is_segment_start())
			write_segment_start(info.get_segment(), listing_stream);

		AssemblyLine line = instructions[i];

		switch(line.instruction->opcode){
		case DATA_BYTE:		//Write data byte
			write_data_instruction(line, listing_stream);
			break;
		case DATA_WORD:		//Write data word
			write_data_instruction(line, listing_stream);
			info.advance();
			break;
		case DATA_TEXT:		//Write text
			write_data_instruction(line, listing_stream);
			break;
		case DATA_RET:
			write_data_instruction(line, listing_stream);
			info.advance();
			break;
		default: 
			write_code_line(line, listing_stream);	//Write code
			for (int j = 0; j < line.instruction->operand_length; j++)
				info.advance();
			data_instruction_streak = 0;
		}

		//Write segment trailer
		if (info.is_segment_end())
			write_segment_end(info.get_segment(), listing_stream);

		info.advance();
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
	info.add_label("rst0", 0x00, CODE_T);
	info.add_label("rst1", 0x08, CODE_T);
	info.add_label("rst2", 0x10, CODE_T);
	info.add_label("rst3", 0x18, CODE_T);
	info.add_label("rst4", 0x20, CODE_T);
	info.add_label("trap", 0x24, CODE_T);
	info.add_label("rst5", 0x28, CODE_T);
	info.add_label("rst55", 0x2c, CODE_T);
	info.add_label("rst6", 0x30, CODE_T);
	info.add_label("rst65", 0x34, CODE_T);
	info.add_label("rst7", 0x38, CODE_T);
	info.add_label("rst75", 0x3c, CODE_T);
}

/*
Copies the jummp labels from to the DSMInfo instance
*/
static void copy_labels_to_info(std::unordered_map<unsigned int, std::string> &labels) {
	for (auto label : labels) {
		if (!info.label_at(label.first))
			info.add_label(label.second, label.first, CODE_T);
	}
}

static void print_version() {
	std::cout << "=== dsm85 version " << VERSION_MAJOR << "." << VERSION_MINOR << " ===" << std::endl;
	std::cout << "An intel 8080 and 8085 disassembler" << std::endl;
	std::cout << "Written in 2020 by Delphi1024" << std::endl << std::endl;
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
		"Name of the file to write the disassembly to. If no output file is given,\nthe output will be written to [input file name].lst",
		{"file"},
		[](std::string *params) -> bool {output_file = params[0];  return true; }
	);
	parser.create_argument(
		"-l", "--labels",
		"Load labels from file.",
		{ "file" },
		[](std::string *params) -> bool {labels_file = params[0];  return true; }
	);
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
		{ "integer" },
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
		std::cout << std::endl << "Please refer to the wiki for further information:" << std::endl << "  https://github.com/0xJonas/dsm85/wiki" << std::endl << std::endl;
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

	//Set output file to default if not given
	if (output_file.length() == 0) {
		size_t ending = input_file.rfind(".");
		std::string input_name = input_file.substr(0, ending);
		output_file = input_name + ".lst";
	}

	//add labels for interrupt vectors
	if(hw_labels)
		add_interrupt_labels();

	//load user labels
	if (!labels_file.empty()) {
		std::ifstream labels_stream(labels_file, std::ios_base::in | std::ios_base::binary);
		if (labels_stream.fail()) {
			std::cerr << "Error: File not found: " << labels_file << std::endl;
			return ERROR_FILE_NOT_FOUND;
		}

		try {
			Parser::parse(labels_stream, labels_file, info);
		}
		catch (parse_error) {
			return ERROR_BAD_LABEL_FILE;
		}

		labels_stream.close();
	}

	std::ofstream listing_stream(output_file, std::ios_base::out);
	if (!listing_stream) {
		std::cerr << "Error: File could net be opened: " << output_file << std::endl;
		return ERROR_FILE_NOT_FOUND;
	}

	//First pass
	label_output = &first_pass_labels;
	single_pass(rom_stream);

	//Second pass
	label_output = &second_pass_labels;
	single_pass(rom_stream);

	copy_labels_to_info(second_pass_labels);

	//Write final listing
	write_listing(listing_stream);

	//Clean up
	rom_stream.close();
	listing_stream.close();
}
