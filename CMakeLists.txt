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

add_executable(dsm85 ${source_files})

# remove default /W3 warning level from MSVC compiler flags
string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	if(MSVC)
		target_compile_options(dsm85 PRIVATE /W4 /Od)
	else()
		target_compile_options(dsm85 PRIVATE -Wall -Og)
	endif()
else()
	if(MSVC)
		target_compile_options(dsm85 PRIVATE /W4 /WX)
	else()
		target_compile_options(dsm85 PRIVATE -Wall -Werror)
	endif()
endif()

if(X86 AND NOT MSVC)
	target_compile_options(dsm85 PRIVATE -m32)
endif()