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
#include "parser/Lexer.h"
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

/*
================================
           READ INPUT
================================
*/

/*
Checks if there is a label pointing to the given address. Since the disassembler uses two lists, both have to be checked.
*/
static bool label_at(unsigned int address) {
	return info.label_at(address)
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
2) there is a label pointing to that address, which means that a new instruction has to start there.
3) a new segment starts at that address
*/
static bool can_read_as_operand(unsigned int address) {
	//start_address and end_address are relative to the input file, while the address parameter is based on base_address.
	if (address<base_address || address>base_address + end_address - start_address)
		return false;
	if (label_at(address))
		return false;
	if (info.is_segment_start())
		return false;
	return true;
}

/*
Adds a data byte pseudo-instruction. 
*/
static void add_data_byte(int address, int byte) {
	AssemblyLine data_byte(address, &(instructions8085[DATA_BYTE]), byte);
	instructions.push_back(std::move(data_byte));
}

/*
Adds a data word pseudo-instruction.
*/
static void add_data_word(int address, int word) {
	AssemblyLine data_word(address, &(instructions8085[DATA_WORD]), word);
	instructions.push_back(std::move(data_word));
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
			add_data_byte(address, opcode);
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
			add_data_byte(address, opcode);
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
			add_data_byte(address, opcode);
			add_data_byte(address + 1, operand);
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
	while (!rom_stream.eof() && rom_stream.tellg()<=end_address) {
		int address = current_address;
		int data = 0;

		switch (info.get_data_type()) {
		case CODE_T:
			read_code_instruction(rom_stream);
			break;
		case BYTES_T:
			data = fetch_byte(rom_stream);
			add_data_byte(address, data);
			break;
		case DWORDS_BE_T:
			data = fetch_byte(rom_stream);
			if (info.get_data_type() == DWORDS_BE_T) {
				data = (data << 8) | (fetch_byte(rom_stream) & 0xff);
				add_data_word(address, data);
			}
			else {
				add_data_byte(address, data);
			}
			break;
		case DWORDS_LE_T:
			data = fetch_byte(rom_stream);
			if (info.get_data_type() == DWORDS_LE_T) {
				data = (fetch_byte(rom_stream) << 8) | (data & 0xff);
				add_data_word(address, data);
			}
			else {
				add_data_byte(address, data);
			}
			break;
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
	if (line.instruction->operand_type == ADDRESS) {
		Label *label = info.get_label(line.operand);
		if (label)	//Print label
			listing_stream << label->get_name(line.operand);
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
Writes the start of a segment.
*/
static void write_segment_start(Segment *segment, std::ostream &listing_stream) {
	listing_stream << std::endl;
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
	if (label) {
		std::string name = label->get_name(line.address);
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
Successive Data instructions (DATA_BYTE and DATA_WORD) are merged to aid readibility and to preserve space. This function writes
the first part of a data instruction to the output stream.
*/
static void start_data_instruction(const AssemblyLine &line, std::ostream &listing_stream) {
	//Create a new line
	listing_stream << std::endl;

	//add address collumn
	if (add_address_column)
		write_address_column(line, listing_stream);

	//Write label
	Label *label = info.get_label(line.address);
	if (label) {
		std::string name = label->get_name(line.address);
		write_jump_label(name, listing_stream);
	}
	else
		listing_stream << INDENT << INDENT;

	//Write instruction mnemonic
	listing_stream << line.instruction->mnemonic;

	//Write operand
	write_operand(line, listing_stream);
}

/*
This function writes a data instruction that is not the first data instruction of the current line.
*/
static void continue_data_instruction(const AssemblyLine &line, std::ostream &listing_stream) {
	listing_stream << ",";
	write_operand(line, listing_stream);
}

/*
Writes a data instruction to the output stream. Data instructions are handled differently from code instructions, in the 
that successive data instructions are merged together.
*/
static void write_data_instruction(const AssemblyLine &line, std::ostream &listing_stream) {
	static int db_instruction_streak = 0;
	static int prev_opcode = -1;

	//Start a new line if the type of data instruction switched (e.g. from DATA_BYTE to DATA_WORD)
	if (line.instruction->opcode != prev_opcode)
		db_instruction_streak = 0;

	//Start a new line if the current instruction has a label pointing to it
	if (info.label_at(line.address))
		db_instruction_streak = 0;

	//Start a new line if the next instruction is the start of a new segment
	if (info.is_segment_start())
		db_instruction_streak = 0;

	//Start a new line or continue an existing one depending on the previous instructions
	if (db_instruction_streak == 0) {
		start_data_instruction(line, listing_stream);
	}
	else {
		continue_data_instruction(line, listing_stream);
	}

	db_instruction_streak++;

	//Only write a maximum of 8 data instructions on a single line
	if (db_instruction_streak >= 8)
		db_instruction_streak = 0;

	//If the current line has a comment, the line has to end prematurely
	if (info.has_comment()) {
		listing_stream << INDENT << ";" << info.get_comment()->text;
		db_instruction_streak = 0;
	}


	//End the current line if it is the last instruction of a segment
	if (info.is_segment_end())
		db_instruction_streak = 0;

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

		if (line.instruction->opcode == DATA_BYTE) {		//Write data byte
			write_data_instruction(line, listing_stream);
		}
		else if (line.instruction->opcode == DATA_WORD) {	//Write data word
			write_data_instruction(line, listing_stream);
			info.advance();
		}
		else {
			write_code_line(line, listing_stream);	//Write code
			for (int j = 0; j < line.instruction->operand_length; j++)
				info.advance();
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
	
	info.add_range_label("hiram", 0x2000, 0x27ff, CODE_T);
	info.add_segment("Test", CODE_T, 0x00, 0xff);
	info.add_segment("Test2", DWORDS_LE_T, 0x100, 0x1ff);
	info.add_comment("This is a comment", 0x06);
	

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
	label_output = &second_pass_labels;
	single_pass(rom_stream);

	copy_labels_to_info(second_pass_labels);

	//Write final listing
	write_listing(listing_stream);

	//Clean up
	rom_stream.close();
	listing_stream.close();
}
