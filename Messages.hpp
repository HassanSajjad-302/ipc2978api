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
// All fields are sent in declaration order, even if meaningless.

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
    // Following fields are meaningless if the compilation failed.
    // header-includes discovered during compilation.
    vector<string> headerFiles;
    // compiler output
    string output;
    // compiler error output.
    // Any IPC related error output should be reported on stderr.
    string errorOutput;
    // exported module name if any.
    string logicalName;
    // This is communicated because the receiving process has no
    // way to learn the shared memory file size on both Windows
    // and Linux without making a filesystem call.
    // Meaningless if the file compiled is not a module interface unit
    // or a header-unit.
    uint32_t fileSize = UINT32_MAX;
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

struct MemoryMappedBMIFile
{
    string filePath;
    uint32_t fileSize = UINT32_MAX;
};

// Reply for CTBModule
struct BTCModule
{
    MemoryMappedBMIFile requested;
    vector<MemoryMappedBMIFile> deps;
};

// Reply for CTBNonModule
struct BTCNonModule
{
    bool found = false;
    bool isHeaderUnit = false;
    string filePath;
    // if isHeaderUnit == true, fileSize of the requested file.
    // if isHeaderUnit == false, following two are meaning-less.
    uint32_t fileSize;
    vector<MemoryMappedBMIFile> deps;
};

// Reply for CTBLastMessage if the compilation succeeded.
struct BTCLastMessage
{
};

#endif // MESSAGES_HPP
