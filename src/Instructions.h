#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include <string>

#define DATA_BYTE 0x100
#define DATA_WORD 0x101
#define DATA_TEXT 0x102
#define DATA_RET 0x103

enum instruction_type {
	CONTROL, BRANCH, ARITHMETIC, MOVE, DATA
};

//IMMEDIATE HYBRID: Classified as an immediate value but frequently used as an address (eg. LXI instructions)
enum operand_type {
	NONE, IMMEDIATE, ADDRESS, IMMEDIATE_HYBRID, CHARACTER
};

struct Instruction {
	const int opcode;
	const std::string mnemonic;
	const int instruction_type;
	const int operand_length;
	const int operand_type;

	Instruction(int opcode, std::string mnemonic,int instruction_type, int operand_length, int operand_type) :
		opcode(opcode),
		mnemonic(mnemonic),
		instruction_type(instruction_type),
		operand_length(operand_length),
		operand_type(operand_type) {}
};

const extern Instruction instructions8085[260];

#endif
