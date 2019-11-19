#ifndef __INSTRUCTIONS
#define __INSTRUCTIONS

#include <string>

#define DATA_BYTE 0x101
#define DATA_WORD 0x102

enum instruction_type {
	CONTROL, BRANCH, ARITHMETIC, MOVE, DATA
};

enum operand_type {
	NONE, IMMEDIATE, ADDRESS
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

const extern Instruction instructions8085[258];

#endif
