#ifndef MESSAGES_HPP
#define MESSAGES_HPP

#include "Manager.hpp"

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
    HEADER_UNIT_INCLUDE_TRANSLATION = 3,
    LAST_MESSAGE = 4,
};

struct CTBModule
{
    string moduleName;
};

struct CTBHeaderUnit
{
    string headerUnitName;
};

struct CTBResolveInclude
{
    string includeName;
};

struct CTBHeaderUnitIncludeTranslation
{
    string includeName;
};

struct CTBFatBMI
{
    bool hasLogicalName;
    string outputFilePath;
    string logicalName;
};

struct CTBLastMessage
{
    bool exitStatus;
    bool hasLogicalName;
    vector<string> outputFilePaths;
    vector<string> headerFiles;
    string output;
    string errorOutput;
    string logicalName;
    void from(Manager &manager, char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed);
};

enum class BTC : uint8_t
{
    REQUESTED_FILE = 0,
    INCLUDE_PATH = 1,
    HEADER_UNIT_OR_INCLUDE_PATH = 2,
    LAST_MESSAGE = 3,
};

struct BTC_RequestedFile
{
    string filePath;
};

struct BTC_IncludePath
{
    string filePath;
};

struct BTC_HeaderUnitOrIncludePath
{
    bool found;
    bool isHeaderUnit;
    string filePath;
};

struct BTC_LastMessage
{
};

#endif // MESSAGES_HPP
