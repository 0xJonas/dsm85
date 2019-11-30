#include "DSMInfo.h"
#include <stdexcept>
#include <iostream>

template<class T>
static int bisect(std::vector<T> list, unsigned int address, unsigned int (*value_of)(T item)) {
	unsigned int start = 0, end = (unsigned int) list.size();
	unsigned int pivot = (start + end) / 2;
	while (start < end) {
		if (value_of(list[pivot]) < address)
			start = pivot + 1;
		else if (value_of(list[pivot]) > address)
			end = pivot;
		else
			break;
		pivot = (start + end) / 2;
	}
	return pivot;
}

//--------Data type----------

static unsigned int value_of_data_type(std::pair<unsigned int, data_type> item) {
	return item.first;
}

/*
Using this function, the following is ensured:
-Ranges are sorted in ascending order
-Consecutive ranges have different data types
-The whole address space is covered
*/
void DSMInfo::set_data_type(unsigned int start_address, unsigned int end_address, data_type type) {
	if (start_address >= end_address)
		return;

	auto start_it = data_types.begin() + bisect<std::pair<unsigned int, data_type>>(data_types, start_address, value_of_data_type);
	auto end_it = data_types.begin() + bisect<std::pair<unsigned int, data_type>>(data_types, end_address, value_of_data_type);

	//Remember type at the end of the range, since it might have to get re-added later.
	data_type type_at_end = (end_it-1)->second;

	start_it = data_types.erase(start_it, end_it);

	start_it = data_types.insert(start_it, std::pair<unsigned int, data_type>(start_address, type));

	//Continue previous data type region
	if (start_it + 1 == data_types.end() || (start_it + 1)->first != end_address) {
		start_it = data_types.insert(start_it + 1, std::pair<unsigned int, data_type>(end_address, type_at_end));
		start_it--;
	}

	//Merge with next region if data types are the same
	if (start_it + 1 != data_types.end() && (start_it + 1)->second == type)
		data_types.erase(start_it + 1);

	//Merge with previous region if data types are the same
	if (start_it != data_types.begin() && (start_it - 1)->second == type)
		data_types.erase(start_it);
}

data_type DSMInfo::get_data_type() {
	data_type type = data_types[data_type_index].second;	//Read data type from labels
	if (type == UNDEFINED_T) {
		Segment *s = get_segment();		//Read data type from current segment
		if (s)
			return s->type;
		else
			return CODE_T;		//Default type
	}
	else
		return type;
}

//---------Control----------

void DSMInfo::reset(unsigned int base_address) {
	//Reset indices
	current_address = base_address;
	segment_index = 0;
	data_type_index = 0;
	comment_index = 0;

	//Setup first instance of next_*_start variables

	if (segments.size() > 1)
		next_segment_start = segments[1]->start_address;
	else	//no user-defined segments
		next_segment_start = (unsigned int)-1;

	if (data_types.size() > 1) {
		next_data_type_start = data_types[1].first;
	}
	else {
		next_data_type_start = (unsigned int)-1;
	}

	if (comments.size() > 1)
		next_comment = comments[1]->address;
	else	//No user-defined comments
		next_comment = (unsigned int)-1;
}

void DSMInfo::advance() {
	current_address++;

	//Check if the next segment has been entered
	if (current_address >= next_segment_start) {
		//Increment index
		segment_index++;
		if (segment_index + 1 < segments.size()) {	//Set when segment_index should be incremented again
			next_segment_start = segments[segment_index + 1]->start_address;
		}
		else {	//No more segments remaining
			next_segment_start = (unsigned int) -1;
		}
	}

	//Check if the data type changed. Since the last entry in the data type vector extends to the end of the file, do not increment
	//the index again when the last change has been reached.
	if (current_address >= next_data_type_start && data_type_index + 1 < data_types.size()) {
		data_type_index++;
		if(data_type_index + 1 < data_types.size())
			next_data_type_start = data_types[data_type_index + 1].first;
	}

	//Check if the next address has a comment
	if (current_address >= next_comment) {
		comment_index++;
		if (comment_index + 1< comments.size()) {
			next_comment = comments[comment_index + 1]->address;
		}
		else {
			next_comment = (unsigned int)-1;
		}
	}
}

//-------Comments---------

static unsigned int value_of_comment(Comment *c) {
	return c->address;
}

void DSMInfo::add_comment(std::string text, unsigned int address) {
	unsigned int index = bisect<Comment*>(comments, address, value_of_comment);

	Comment *c = new Comment(text, address);

	//delete previous comment, if it exists
	if (index < comments.size() && comments[index]->address == address) {
		delete comments[index];
		comments[index] = c;
	}
	else {
		comments.insert(comments.begin() + index, c);
	}
}

bool DSMInfo::has_comment() {
	return comment_index<comments.size()
		&& current_address == comments[comment_index]->address;
}

Comment *DSMInfo::get_comment() {
	if (has_comment())
		return comments[comment_index];
	else
		return nullptr;
}

//-------Segments---------

static unsigned int value_of_segment(Segment *item) {
	return item->start_address;
}

void DSMInfo::add_segment(std::string name, data_type data_type, unsigned int start_address, unsigned int end_address) {
	unsigned int start_index = bisect<Segment *>(segments, start_address, value_of_segment);
	unsigned int end_index = bisect<Segment *>(segments, end_address, value_of_segment);

	//check if segment overlaps with existing segments
	if (start_index != end_index
		|| start_index > 0 && segments[start_index - 1]->end_address >= start_address)
		throw std::invalid_argument("Segments can not overlap");

	Segment *s = new Segment(name, data_type, start_address, end_address);
	segments.insert(segments.begin() + start_index, s);
}

bool DSMInfo::is_segment_start() {
	return segment_index < segments.size()
		&& current_address == segments[segment_index]->start_address;
}

bool DSMInfo::is_segment_end() {
	return segment_index < segments.size()
		&& current_address == segments[segment_index]->end_address;
}

Segment *DSMInfo::get_segment() {
	if (segment_index < segments.size()) {	//Still segments remaining
		Segment *s = segments[segment_index];
		if (current_address >= s->start_address && current_address <= s->end_address)	//Inside current segment
			return s;
		else	//Outside of current segment but not yet inside the next segment
			return nullptr;
	}
	else	//Outside the last segment
		return nullptr;
}

//---------Labels-----------

void DSMInfo::add_label(std::string name, unsigned int address, data_type type, bool jump_label) {
	Label *l = new Label(name, address, type, jump_label);
	label_refs.push_back(l);

	labels[address] = l;
	set_data_type(address, address + 1, type);

}

void DSMInfo::add_range_label(std::string name, unsigned int start_address, unsigned int end_address, data_type type, bool jump_label) {
	RangeLabel *rl = new RangeLabel(name, start_address, end_address, type, jump_label);
	RangeLabel *rl_head = new RangeLabel(name, start_address, end_address, type, true);
	label_refs.push_back(rl);
	label_refs.push_back(rl_head);

	//Separate label for the first element, since it signifies the start of the range
	labels[start_address] = rl_head;

	//Add pointer to the new label for every byte in the range.
	for (unsigned int i = start_address + 1; i <= end_address; i++) {
		labels[i] = rl;
	}
	set_data_type(start_address, end_address + 1, type);
}

bool DSMInfo::label_at(unsigned int address) {
	return labels.find(address) != labels.end();
}

Label *DSMInfo::get_label(unsigned int address) {
	if(label_at(address))
		return labels[address];
	else
		return nullptr;
}

void DSMInfo::test() {
	set_data_type(10, 100, CODE_T);
	set_data_type(100, 200, BYTES_T);
	set_data_type(50, 150, CODE_T);
	set_data_type(170, 200, CODE_T);
	set_data_type(170, 300, DWORDS_BE_T);
	set_data_type(200, 201, CODE_T);
	print_data_types();
}

void DSMInfo::print_data_types() {
	std::string type_names[] = { "undefined","code", "bytes","dwords_be","dwords_le" };
	for (auto i : data_types) {
		std::cout << i.first << " " << type_names[i.second] << std::endl;
	}
}