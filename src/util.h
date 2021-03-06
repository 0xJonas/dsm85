#ifndef UTIL_H
#define UTIL_H

#include <string>

int parse_int_literal(std::string str);

std::string hex16bit(const int v);

std::string hex8bit(const int v);

#endif