#ifndef MESSAGES_HPP
#define MESSAGES_HPP

#include "BufferSize.hpp"
#include <string>
#include <vector>
using std::string, std::vector;

// CTB --> Compiler to Build-System
// BTC --> Build-System to Compiler

// string is 4 bytes that hold the size of the char array, followed by the array.
// vector is 4 bytes that hold the size of the array, followed by the array.
// All fields are to be sent, even if unused/empty.

// Compiler to Build System
// This is the first byte of the compiler to build-system message.
enum class CTB : uint8_t
{
    MODULE = 0,
    NON_MODULE = 1,
    LAST_MESSAGE = 2,
};

// This is sent when the compiler needs a module.
struct CTBModule
{
    string moduleName;
};

// This is sent when the compiler needs something else than a module.
// isHeaderUnit is set when the compiler knows that it is a header-unit.
// If findInclude flag is provided, then the compiler sends logicalName,
// Otherwise compiler sends the full path.
struct CTBNonModule
{
    bool isHeaderUnit = false;
    string str;
};

// This is the last message sent by the compiler.
struct CTBLastMessage
{
    // Whether the compilation succeeded or failed.
    bool exitStatus = false;
    // Following fields are sent but are empty
    // if the compilation failed.
    // True if the file compiled is a module interface unit.
    bool hasLogicalName = false;
    // header-includes discovered during compilation.
    vector<string> headerFiles;
    // compiler output
    string output;
    // compiler error output.
    // Any IPC related error output should be on stderr.
    string errorOutput;
    // output files
    vector<string> outputFilePaths;
    // exported module name
    string logicalName;
};

// Build System to Compiler
// Unlike CTB, this is not written as the first byte
// since the compiler knows what message it will receive.
enum class BTC : uint8_t
{
    MODULE = 0,
    NON_MODULE = 1,
    LAST_MESSAGE = 2,
};

// Reply for CTBModule
struct BTCModule
{
    string filePath;
};

// Reply for CTBNonModule
struct BTCNonModule
{
    bool found = false;
    bool isHeaderUnit = false;
    string filePath;
};

// Reply for CTBLastMessage if the compilation succeeded.
struct BTCLastMessage
{
};

#endif // MESSAGES_HPP
