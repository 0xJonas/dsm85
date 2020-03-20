#ifndef DSMINFO_H
#define DSMINFO_H

#include <string>
#include <vector>
#include <unordered_map>

enum data_type {
	UNDEFINED_T, CODE_T, BYTES_T, DWORDS_BE_T, DWORDS_LE_T, TEXT_T, RET_T
};

/*
A segment in the disassembly. A segment has name, a start and end address and a data type.
*/
struct Segment {
	std::string name;
	data_type type;
	unsigned int start_address;
	unsigned int end_address;

	Segment(std::string name, data_type type, unsigned int start, unsigned int end) :
		name(name),
		type(type),
		start_address(start),
		end_address(end) 
	{}
};

/*
A singel-address label in the disassembly. A label has a name (identifier), an address and a data type.
jump_label determines whether this label will appear as a jump target in the output column before an instruction.
If jump_label is false, the label will only appear if used as an operand.
*/
struct Label {
protected:
	std::string name;

public:
	unsigned int start_address;
	data_type type;
	bool jump_label;

	Label(std::string name, unsigned int address, data_type type, bool jump_label) :
		name(name),
		start_address(address),
		type(type),
		jump_label(jump_label)
	{}

	virtual ~Label() = default;

	/*
	Returns the name that gets used in a jump target context.
	*/
	virtual std::string get_jump_target_name(unsigned int address) {
		(void)address;
		return name;
	}

	/*
	Returns the name that gets used in an operand context.
	*/
	virtual std::string get_operand_name(unsigned int address) {
		(void)address;
		return name;
	}

	/*
	Determine whether an instance is a single-address label or a range label.
	*/
	virtual bool range_label() {
		return false;
	}

	/*
	Determine whether an instance is an indirect label
	*/
	virtual bool indirect_label() {
		return false;
	}
};

/*
Reprents a ranged label. These labels names a range of bytes.
*/
struct RangeLabel : public Label {
	unsigned int end_address;

	RangeLabel(std::string name, unsigned int start_address, unsigned int end_address, data_type type, bool jump_label) :
		Label(name, start_address, type, jump_label),
		end_address(end_address) 
	{}

	virtual std::string get_jump_target_name(unsigned int address) {
		if (address == start_address)
			return name;
		else
			return "";
	}

	/*
	Returns the name based on the input address. Required for RangeLabels to have an index in their names.
	*/
	virtual std::string get_operand_name(unsigned int address) {
		return name + "[" + std::to_string(address - start_address) + "]";
	}

	/*
	Determine whether an instance is a single-address label or a range label.
	*/
	virtual bool range_label() {
		return true;
	}
};

struct IndirectLabel : public Label {
	unsigned int offset;

	IndirectLabel(std::string name, unsigned int address, unsigned int offset, bool jump_label) :
		Label(name, address, CODE_T, jump_label),
		offset(offset)
	{}

	virtual std::string get_jump_target_name(unsigned int address) {
		(void)address;
		return name;
	}

	virtual std::string get_operand_name(unsigned int address) {
		(void)address;
		return "";
	}

	int get_offset() {
		return offset;
	}

	/*
	Determine whether an instance is an indirect label
	*/
	virtual bool indirect_label() {
		return true;
	}
};

/*
A comment in the disassembly. A comment is basically text that is a associated with a single address.
*/
struct Comment {
	std::string text;
	unsigned int address;

	Comment(std::string text, unsigned int address) :
		text(text),
		address(address)
	{}
};

/*
DSMInfo instances contain various information that should get added to a disassembly. This includes segment boundaries, labels, comments etc.
Once a DSMInfo instance is populated with these information, the data can be accessed in a stream-like manner. The disassembler will call
advance() once for every address, which will cause the DSMInfo instance to load the data for the next address. Only the information for the current
address can be read at any time.
*/
class DSMInfo {

	//Indices
	unsigned int current_address = 0;
	unsigned int segment_index = 0;
	unsigned int next_segment_start = 0;
	unsigned int data_type_index = 0;
	unsigned int next_data_type_start = 0;
	unsigned int comment_index = 0;
	unsigned int next_comment = 0;

	//Main data structures
	std::vector<Segment *> segments;
	std::vector<std::pair<unsigned int, data_type>> data_types;
	std::unordered_map<unsigned int, Label *> labels;
	std::vector<Comment *> comments;

	//vector of all labels in the order they were added. Used for memory management.
	std::vector<Label *> label_refs;

	/*
	Helper to manage the data_types vector.
	*/
	void set_data_type(unsigned int start_address, unsigned int end_address, data_type data_type);

public:
	DSMInfo() {
		//Initialize data types with all undefined
		data_types.push_back(std::pair<unsigned int, data_type>(0, UNDEFINED_T));
	}

	~DSMInfo() {
		for (Segment *s : segments)
			delete s;
		for (Comment *c : comments)
			delete c;
		for (Label *l : label_refs)
			delete l;
	}

	/*
	Returns the data type at the current address. This first checks if any single-address or ranged labels specify a data type.
	If not this function returns the data_type of the current segment. If the current address does not lie in a segments,
	this function returns CODE_T.
	*/
	data_type get_data_type();

	/*
	Resets the DSMInfo stream and initialized it with the given base_address.
	*/
	void reset(unsigned int base_address);

	/*
	Advances the DSMInfo stream to the next address.
	*/
	void advance();

	/*
	Adds a segment to this DSMInfo. If it overlaps with already existing segments, an exception is thrown.
	*/
	void add_segment(std::string name, data_type data_type, unsigned int start_address, unsigned int end_address);

	/*
	Checks whether a segment starts at the current address.
	*/
	bool is_segment_start();

	/*
	Checks whether a segment ends at the current address.
	*/
	bool is_segment_end();

	/*
	Returns a pointer to the segment that contains the current address. Returns nullptr if the current address does not lie in a user-defined segment.
	*/
	Segment *get_segment();

	/*
	Adds a new comment to this DSMInfo. If a comment already exists at the given address, it will get overwritten.
	*/
	void add_comment(std::string text, unsigned int address);

	/*
	Checks wether there is a comment at the current address.
	*/
	bool has_comment();

	/*
	Returns a pointer to the comment at the current address, or nullptr if there is none.
	*/
	Comment *get_comment();
	
	/*
	Adds a new single-address label to this DSMInfo. If a label already exists at the given address, it will get overwritten.
	*/
	void add_label(std::string name, unsigned int address, data_type type, bool jump_label = true);

	/*
	Adds an indirect label where the pointer is stored at the given address in the file. 
	*/
	void add_indirect_label(std::string name, unsigned int address, unsigned int offset);

	/*
	Adds a new range label to this DSMInfo. This will override all existing labels inside the given range.
	*/
	void add_range_label(std::string name, unsigned int address, unsigned int end_address, data_type type, bool jump_label = false);

	/*
	Checks whether there is a label at the given address
	*/
	bool label_at(unsigned int address);

	/*
	Returns the label at the given address or nullptr if there is none
	*/
	Label *get_label(unsigned int address);

	//Test
	void test();
	void print_data_types();
};

#endif