#ifndef MESSAGES_HPP
#define MESSAGES_HPP

#include "BufferSize.hpp"
#include <string>
#include <vector>
using std::string, std::vector;

// CTB --> Compiler to Build-System
// BTC --> Build-System to Compiler

enum class CTB : uint8_t
{
    MODULE = 0,
    HEADER_UNIT = 1,
    RESOLVE_INCLUDE = 2,
    RESOLVE_HEADER_UNIT = 3,
    HEADER_UNIT_INCLUDE_TRANSLATION = 4,
    LAST_MESSAGE = 5,
};

struct CTBModule
{
    string moduleName;
};

struct CTBHeaderUnit
{
    string headerUnitFilePath;
};

struct CTBResolveInclude
{
    string includeName;
};

struct CTBResolveHeaderUnit
{
    string logicalName;
};

struct CTBHeaderUnitIncludeTranslation
{
    string includeName;
};

struct CTBLastMessage
{
    bool exitStatus;
    bool hasLogicalName;
    vector<string> headerFiles;
    string output;
    string errorOutput;
    vector<string> outputFilePaths;
    string logicalName;
    void from(class Manager &manager, char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed);
};

enum class BTC : uint8_t
{
    REQUESTED_FILE = 0,
    RESOLVED_FILEPATH = 1,
    HEADER_UNIT_OR_INCLUDE_PATH = 2,
    LAST_MESSAGE = 3,
};

struct BTCRequestedFile
{
    string filePath;
};

struct BTCResolvedFilePath
{
    bool exists;
    string filePath;
};

struct BTCHeaderUnitOrIncludePath
{
    bool exists;
    bool isHeaderUnit;
    string filePath;
};

struct BTCLastMessage
{
};

#endif // MESSAGES_HPP
