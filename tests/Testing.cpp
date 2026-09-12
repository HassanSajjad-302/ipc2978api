
#include "Testing.hpp"

#include "IPCManagerBS.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include "fmt/printf.h"

using fmt::print, std::filesystem::current_path;

[[noreturn]] void exitFailure(const string &str)
{
    print("\n\n random-int {}\n\n", randomSeed);
    print(stderr, "{}\n", str);
    print("Test Failed\n");
    exit(EXIT_FAILURE);
}

string fileToString(const string_view file_name)
{
    std::ifstream file_stream{string(file_name)};

    if (file_stream.fail())
    {
        exitFailure(fmt::format("Error opening file {}\n", file_name));
    }

    const std::ostringstream str_stream;
    file_stream >> str_stream.rdbuf();

    if (file_stream.fail() && !file_stream.eof())
    {
        exitFailure(fmt::format("Error reading file {}\n", file_name));
    }

    return str_stream.str();
}

string getRandomString(const uint64_t length)
{
    const string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);
    std::uniform_int_distribution<uint64_t> distribution2(1, 10000);
    const uint64_t length2 = length ? length : distribution2(generator);
    string randomString(length2, '\0');
    for (uint64_t i = 0; i < length2; ++i)
    {
        randomString[i] = characters[distribution(generator)];
    }
    return randomString;
}

bool getRandomBool()
{
    std::uniform_int_distribution distribution(0, 1);
    return distribution(generator);
}

uint64_t getRandomNumber(const uint64_t max)
{
    std::uniform_int_distribution<uint64_t> distribution(0, max);
    return distribution(generator);
}

static TestResponse createTestFile(FileType type, bool isSystem)
{
    string name = getRandomString(10);
    for (char &c : name)
        c = tolower(c);
    string filePath = (current_path() / name).string();
#ifdef _WIN32
    for (char &c : filePath)
        c = tolower(c);
#endif
    string contents = getRandomString();
    if (contents.empty())
        contents.push_back('a');
    std::ofstream(filePath, std::ios::binary) << contents;
    return {std::move(filePath), std::move(contents), type, isSystem};
}

static auto addTestFile(string_view key, const TestResponse &file)
{
    return tempTestFiles.emplace(string(key), file).first;
}

static BMIFile addLogicalNames(vector<string_view> &names, uint64_t count, const TestResponse &file)
{
    BMIFile bmi;
    for (uint64_t i = 0; i < count; ++i)
    {
        const auto entry = addTestFile(getRandomString(), file);
        names.emplace_back(entry->first);
        bmi = {entry->second.filePath, static_cast<uint32_t>(entry->second.fileContent.size())};
    }
    return bmi;
}

BTCModule getBTCModule(const CTBModule &request)
{
    BTCModule response;
    response.isSystem = getRandomBool();
    const auto requested = addTestFile(request.moduleName, createTestFile(FileType::MODULE, response.isSystem));
    response.requested = {requested->second.filePath, static_cast<uint32_t>(requested->second.fileContent.size())};
    const uint64_t count = getRandomNumber(10);
    for (uint64_t i = 0; i < count; ++i)
    {
        ModuleDep dep;
        dep.isSystem = getRandomBool();
        dep.isHeaderUnit = getRandomBool();
        const auto type = dep.isHeaderUnit ? FileType::HEADER_UNIT : FileType::MODULE;
        uint64_t names = getRandomNumber(10);
        if (!names || !dep.isHeaderUnit)
            names = 1;
        dep.file = addLogicalNames(dep.logicalNames, names, createTestFile(type, dep.isSystem));
        response.modDeps.emplace_back(std::move(dep));
    }
    return response;
}

BTCNonModule getBTCNonModule(const CTBNonModule &request)
{
    BTCNonModule response;
    response.isHeaderUnit = request.isHeaderUnit || getRandomBool();
    response.isSystem = getRandomBool();
    const uint64_t headers = getRandomNumber(10);
    for (uint64_t i = 0; i < headers; ++i)
    {
        const auto entry = addTestFile(getRandomString(), createTestFile(FileType::HEADER_FILE, getRandomBool()));
        response.headerFiles.push_back({entry->first, entry->second.filePath, entry->second.isSystem});
    }
    const auto type = response.isHeaderUnit ? FileType::HEADER_UNIT : FileType::HEADER_FILE;
    const auto requested = addTestFile(request.logicalName, createTestFile(type, response.isSystem));
    response.filePath = requested->second.filePath;
    if (!response.isHeaderUnit)
        return response;
    response.fileSize = requested->second.fileContent.size();
    addLogicalNames(response.logicalNames, getRandomNumber(2), requested->second);
    const uint64_t deps = getRandomNumber(10);
    for (uint64_t i = 0; i < deps; ++i)
    {
        HuDep dep;
        dep.isSystem = getRandomBool();
        uint64_t names = getRandomNumber(10);
        if (!names)
            names = 1;
        dep.file = addLogicalNames(dep.logicalNames, names, createTestFile(FileType::HEADER_UNIT, dep.isSystem));
        response.huDeps.emplace_back(std::move(dep));
    }
    return response;
}

std::string_view fileTypeToString(FileType type)
{
    switch (type)
    {
    case FileType::HEADER_FILE:
        return "Header-File";
    case FileType::MODULE:
        return "Module";
    case FileType::HEADER_UNIT:
        return "Header-Unit";
    }
    return "Unknown";
}

void appendResponse(std::string &output, std::string_view path, std::string_view contents, FileType type, bool isSystem)
{
    output += fmt::format("Filepath {}\nFileContent {}\nFileType {}\nIsSystem {}\n", path, contents,
                          fileTypeToString(type), isSystem);
}

void printSendingOrReceiving(const bool sent)
{
    if (sent)
    {
        print("Sending ");
    }
    else
    {
        print("Receiving ");
    }
}

void printMessage(const CTBModule &ctbModule, const bool sent)
{
    printSendingOrReceiving(sent);
    print("CTBModule\n\n");
    print("Module Name: {}\n\n", ctbModule.moduleName);
}

void printMessage(const CTBNonModule &nonModule, const bool sent)
{
    printSendingOrReceiving(sent);
    print("CTBNonModule\n\n");
    print("IsHeaderUnit: {}\n\n", nonModule.isHeaderUnit);
    print("logicalNames: {}\n\n", nonModule.logicalName);
}

void printMessage(const BTCModule &btcModule, const bool sent)
{
    printSendingOrReceiving(sent);
    print("BTCModule\n\n");

    print("Requested FilePath: {}\n\n", btcModule.requested.filePath);
    print("Requested User: {}\n\n", btcModule.isSystem);
    print("Requested FileSize: {}\n\n", btcModule.requested.fileSize);
    print("Deps Size: {}\n\n", btcModule.modDeps.size());
    for (uint64_t i = 0; i < btcModule.modDeps.size(); i++)
    {
        print("Mod-Dep[{}] IsHeaderUnit: {}\n\n", i, btcModule.modDeps[i].isHeaderUnit);
        print("Mod-Dep[{}] FilePath: {}\n\n", i, btcModule.modDeps[i].file.filePath);
        print("Mod-Dep[{}] FileSize: {}\n\n", i, btcModule.modDeps[i].file.fileSize);
        print("Mod-Dep[{}] LogicalName Size: {}\n\n", i, btcModule.modDeps[i].logicalNames.size());
        for (uint64_t j = 0; j < btcModule.modDeps[i].logicalNames.size(); ++j)
        {
            print("Mod-Dep[{}] LogicalName[{}]: {}\n\n", i, j, btcModule.modDeps[i].logicalNames[j]);
        }
        print("Mod-Dep[{}] User: {}\n\n", i, btcModule.modDeps[i].isSystem);
    }
}

void printMessage(const BTCNonModule &nonModule, const bool sent)
{
    printSendingOrReceiving(sent);
    print("BTCNonModule\n\n");
    print("IsHeaderUnit {}\n\n", nonModule.isHeaderUnit);
    print("User {}\n\n", nonModule.isSystem);
    print("FilePath {}\n\n", nonModule.filePath);
    print("FileSize {}\n\n", nonModule.fileSize);

    for (uint64_t i = 0; i < nonModule.logicalNames.size(); i++)
    {
        print("Logical-Name[{}]: {}\n\n", i, nonModule.logicalNames[i]);
    }

    for (uint64_t i = 0; i < nonModule.headerFiles.size(); i++)
    {
        print("Header-File[{}] LogicalName: {}\n\n", i, nonModule.headerFiles[i].logicalName);
        print("Header-File[{}] FilePath: {}\n\n", i, nonModule.headerFiles[i].filePath);
        print("Header-File[{}] User: {}\n\n", i, nonModule.headerFiles[i].isSystem);
    }

    for (uint64_t i = 0; i < nonModule.huDeps.size(); i++)
    {
        print("Hu-Dep[{}] FilePath: {}\n\n", i, nonModule.huDeps[i].file.filePath);
        print("Hu-Dep[{}] FileSize: {}\n\n", i, nonModule.huDeps[i].file.fileSize);
        for (uint64_t j = 0; j < nonModule.huDeps[i].logicalNames.size(); ++j)
        {
            print("Mod-Dep[{}] LogicalName[{}]: {}\n\n", i, j, nonModule.huDeps[i].logicalNames[j]);
        }
        print("Hu-Dep[{}] User: {}\n\n", i, nonModule.huDeps[i].isSystem);
    }
}

TestResponse::TestResponse(string filePath_, string fileContent_, FileType fileType_, bool isSystem_)
    : filePath(std::move(filePath_)), fileContent(std::move(fileContent_)), type(fileType_), isSystem(isSystem_)
{
}
