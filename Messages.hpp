#ifndef MESSAGES_HPP
#define MESSAGES_HPP

#include <string>
#include <vector>
using std::string, std::vector;

struct MaybeMappedFile
{
    bool isMapped;
    string filePath;
};

// CTB --> Compiler to Build-System
// BTC --> Build-System to Compiler

enum class CTB_MessageType : uint8_t
{
    MODULE = 0,
    HEADER_UNIT = 1,
    RESOLVE_INCLUDE = 2,
    HEADER_UNIT_INCLUDE_TRANSLATION = 3,
    LAST_MESSAGE = 4,
};

struct CTB
{
    CTB_MessageType type;
};

class IPCManagerBS;

void readStringFromPipe()
struct CTBModule : CTB
{
    string moduleName;
    void from(IPCManagerBS &manager, char (&array)[320], uint64_t &bytesRead, uint64_t &bytesProcessed);
};

struct CTBHeaderUnit : CTB
{
    string headerUnitName;
};

struct CTBResolveInclude : CTB
{
    string includeName;
};

struct CTBHeaderUnitIncludeTranslation : CTB
{
    string includeName;
};

struct CTBLastMessage : CTB
{
    bool exitStatus;
    bool hasLogicalName;
    vector<string> headerFiles;
    string output;
    string errorOutput;
    string logicalName;
    vector<MaybeMappedFile> outputFiles;
};

enum class BTC_MessageType : uint8_t
{
    REQUESTED_FILE = 0,
    INCLUDE_PATH = 1,
    HEADER_UNIT_OR_INCLUDE_PATH = 2,
    LAST_MESSAGE = 3,
};

struct BTC
{
    BTC_MessageType type;
};

struct BTC_RequestedFile
{
    MaybeMappedFile filePath;
};

struct BTC_IncludePath
{
    string filePath;
};

union MappedFileOrPath
{
    MaybeMappedFile maybeMappedFilePath;
    string filePath;
};

struct BTC_HeaderUnitOrIncludePath
{
    bool found;
    bool isHeaderUnit;
    MappedFileOrPath filePath;
};

#endif // MESSAGES_HPP
