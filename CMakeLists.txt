cmake_minimum_required(VERSION 3.12)

project(dsm85
		VERSION 1.0
		DESCRIPTION "Disassembler for Intel 8080 and 8085 CPUs"
		HOMEPAGE_URL "https://github.com/0xJonas/dsm85")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(source_files
	src/ArgumentParser.cpp
	src/DSMInfo.cpp
	src/Instructions.cpp
	src/main.cpp
	src/util.cpp
	src/parser/Lexer.cpp
	src/parser/Parser.cpp)

configure_file(src/config_template.h config.h)

add_executable(dsm85 ${source_files})
target_include_directories(dsm85 PUBLIC ${PROJECT_BINARY_DIR})