
#include "Testing.hpp"

#include "IPCManagerBS.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include "fmt/printf.h"

using fmt::print, std::filesystem::current_path;

void exitFailure(const string &str)
{
    print("\n\n random-int {}\n\n", randomSeed);
    print(stderr, "{}\n", str);
    print("Test Failed\n");
    string str2 = str;
    exit(EXIT_FAILURE);
}

string fileToString(const string_view file_name)
{
    std::ifstream file_stream{string(file_name)};

    if (file_stream.fail())
    {
        // Error opening file.
        exitFailure(fmt::format("Error opening file {}\n", file_name));
    }

    const std::ostringstream str_stream;
    file_stream >> str_stream.rdbuf(); // NOT str_stream << file_stream.rdbuf()

    if (file_stream.fail() && !file_stream.eof())
    {
        // Error reading file.
        exitFailure(fmt::format("Error reading file {}\n", file_name));
    }

    return str_stream.str();
}

bool startsWith(const std::string &str, const std::string &prefix)
{
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

string getRandomString(const uint32_t length)
{
    const string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);
    std::uniform_int_distribution distribution2(0, 10000);
    const uint64_t length2 = length ? length : distribution2(generator);
    string randomString(length2, '\0');
    for (int i = 0; i < length2; ++i)
    {
        // ; is not added to string as it is being used as delimiter.
        if (const char c = characters[distribution(generator)]; c != ';')
        {
            randomString[i] = c;
        }
        else
        {
            --i;
        }
    }

    if (startsWith(randomString, "OUzgC3g2BUwkUh1p1kSJVtXsv0bpJYCSZ"))
    {
        bool brekapoint = true;
    }
    return randomString;
}

bool getRandomBool()
{
    std::uniform_int_distribution distribution(0, 1);
    return distribution(generator);
}

uint32_t getRandomNumber(const uint32_t max)
{
    std::uniform_int_distribution<> distribution(0, max);
    return distribution(generator);
}

auto createTempTestFilesEntry(const bool makeMapping, const string_view key, const FileType fileType,
                              const bool isSystem) -> auto
{
    string str = getRandomString(10);
    for (char &c : str)
    {
        c = tolower(c);
    }

    const string filePath = (current_path() / str).string();
    const string fileContents = getRandomString();
    std::ofstream(filePath) << fileContents;
    string *s = new string(key);
    buildTestallocations.emplace_back(s);
    const auto &it = tempTestFiles.emplace(*s, TestResponse{filePath, fileContents, fileType, isSystem});

    if (makeMapping)
    {
        BMIFile file;
        file.filePath = filePath;
        if (const auto &mapping = IPCManagerBS::createSharedMemoryBMIFile(file); !mapping)
        {
            exitFailure("Failed to create shared memory bmifile");
        }
    }

    return it;
}

BTCModule getBTCModule(const CTBModule &ctbModule)
{
    BTCModule b;
    b.isSystem = getRandomBool();
    auto it = createTempTestFilesEntry(true, ctbModule.moduleName, FileType::MODULE, b.isSystem);

    b.requested.filePath = it.first->second.filePath;
    b.requested.fileSize = it.first->second.fileContent.size();

    const uint32_t modDepCount = getRandomNumber(10);
    for (uint32_t i = 0; i < modDepCount; ++i)
    {
        string str = getRandomString(10);
        for (char &c : str)
        {
            c = tolower(c);
        }

        string *filePath = new string((current_path() / str).string());
        string *fileContents = new string(getRandomString());
        {
            // Creating file and the mapping
            std::ofstream(*filePath) << *fileContents;
            buildTestallocations.emplace_back(filePath);
            buildTestallocations.emplace_back(fileContents);

            BMIFile file;
            file.filePath = *filePath;
            if (const auto &mapping = IPCManagerBS::createSharedMemoryBMIFile(file); !mapping)
            {
                exitFailure("Failed to create shared memory bmifile");
            }
        }

        ModuleDep modDep;
        modDep.isSystem = getRandomBool();
        modDep.isHeaderUnit = getRandomBool();
        modDep.file.filePath = *filePath;
        modDep.file.fileSize = fileContents->size();

        uint32_t logicalNameSize = getRandomNumber(10);

        // if module or the random-number is 0, we set the logicalNameSize = 1
        if (logicalNameSize == 0 || !modDep.isHeaderUnit)
        {
            logicalNameSize = 1;
        }

        for (uint32_t j = 0; j < logicalNameSize; ++j)
        {
            string *s = new string(getRandomString());
            buildTestallocations.emplace_back(s);
            const auto &it2 = tempTestFiles.emplace(
                *s, TestResponse{*filePath, *fileContents,
                                 modDep.isHeaderUnit ? FileType::HEADER_UNIT : FileType::MODULE, modDep.isSystem});
            modDep.logicalNames.emplace_back(it2.first->first);
        }
        b.modDeps.emplace_back(std::move(modDep));
    }
    return b;
}

BTCNonModule getBTCNonModule(const CTBNonModule &ctbNonModule)
{
    BTCNonModule nonModule;
    nonModule.isHeaderUnit = getRandomBool();
    nonModule.isSystem = getRandomBool();

    const uint32_t headerFilesSize = getRandomNumber(10);

    for (uint32_t i = 0; i < headerFilesSize; ++i)
    {
        auto it = createTempTestFilesEntry(false, getRandomString(), FileType::HEADER_FILE, getRandomBool());
        HeaderFile h;
        h.logicalName = it.first->first;
        h.isSystem = it.first->second.isSystem;
        h.filePath = it.first->second.filePath;
        nonModule.headerFiles.emplace_back(h);
    }

    if (!nonModule.isHeaderUnit)
    {
        auto it = createTempTestFilesEntry(false, ctbNonModule.logicalName, FileType::HEADER_FILE, nonModule.isSystem);
        nonModule.filePath = it.first->second.filePath;
        return nonModule;
    }

    auto it = createTempTestFilesEntry(true, ctbNonModule.logicalName, FileType::HEADER_UNIT, nonModule.isSystem);
    nonModule.filePath = it.first->second.filePath;
    nonModule.fileSize = it.first->second.fileContent.size();

    uint32_t logicalNameSize = getRandomNumber(2);
    for (uint32_t i = 0; i < logicalNameSize; ++i)
    {
        string *s = new string(getRandomString());
        buildTestallocations.emplace_back(s);
        const auto &it2 =
            tempTestFiles.emplace(*s, TestResponse{it.first->second.filePath, it.first->second.fileContent,
                                                   FileType::HEADER_UNIT, nonModule.isSystem});
        nonModule.logicalNames.emplace_back(it2.first->first);
    }

    uint32_t huDepSize = getRandomNumber(10);

    for (uint32_t i = 1; i < huDepSize; ++i)
    {
        string str = getRandomString(10);
        for (char &c : str)
        {
            c = tolower(c);
        }

        string *filePath = new string((current_path() / str).string());
        string *fileContents = new string(getRandomString());
        {

            // Creating file and the mapping
            std::ofstream(*filePath) << *fileContents;
            buildTestallocations.emplace_back(filePath);
            buildTestallocations.emplace_back(fileContents);

            BMIFile file;
            file.filePath = *filePath;
            if (const auto &mapping = IPCManagerBS::createSharedMemoryBMIFile(file); !mapping)
            {
                exitFailure("Failed to create shared memory bmifile");
            }
        }
        HuDep huDep;
        huDep.isSystem = getRandomBool();
        huDep.file.filePath = *filePath;
        huDep.file.fileSize = fileContents->size();

        logicalNameSize = getRandomNumber(10);
        if (logicalNameSize == 0)
        {
            logicalNameSize = 1;
        }

        for (uint32_t j = 0; j < logicalNameSize; ++j)
        {
            string *s = new string(getRandomString());
            buildTestallocations.emplace_back(s);
            const auto &it2 = tempTestFiles.emplace(
                *s, TestResponse{*filePath, *fileContents, FileType::HEADER_UNIT, huDep.isSystem});
            huDep.logicalNames.emplace_back(it2.first->first);
        }
        nonModule.huDeps.emplace_back(std::move(huDep));
    }

    return nonModule;
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

void printMessage(const CTBLastMessage &lastMessage, const bool sent)
{
    printSendingOrReceiving(sent);
    print("CTBLastMessage\n\n");
    print("FileSize: {}\n\n", lastMessage.fileSize);
}

void printMessage(const BTCModule &btcModule, const bool sent)
{
    printSendingOrReceiving(sent);
    print("BTCModule\n\n");

    print("Requested FilePath: {}\n\n", btcModule.requested.filePath);
    print("Requested User: {}\n\n", btcModule.isSystem);
    print("Requested FileSize: {}\n\n", btcModule.requested.fileSize);
    print("Deps Size: {}\n\n", btcModule.modDeps.size());
    for (uint32_t i = 0; i < btcModule.modDeps.size(); i++)
    {
        print("Mod-Dep[{}] IsHeaderUnit: {}\n\n", i, btcModule.modDeps[i].isHeaderUnit);
        print("Mod-Dep[{}] FilePath: {}\n\n", i, btcModule.modDeps[i].file.filePath);
        print("Mod-Dep[{}] FileSize: {}\n\n", i, btcModule.modDeps[i].file.fileSize);
        print("Mod-Dep[{}] LogicalName Size: {}\n\n", i, btcModule.modDeps[i].logicalNames.size());
        for (uint32_t j = 0; j < btcModule.modDeps[i].logicalNames.size(); ++j)
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

    for (uint32_t i = 0; i < nonModule.logicalNames.size(); i++)
    {
        print("Logical-Name[{}]: {}\n\n", i, nonModule.logicalNames[i]);
    }

    for (uint32_t i = 0; i < nonModule.headerFiles.size(); i++)
    {
        print("Header-File[{}] LogicalName: {}\n\n", i, nonModule.headerFiles[i].logicalName);
        print("Header-File[{}] FilePath: {}\n\n", i, nonModule.headerFiles[i].filePath);
        print("Header-File[{}] User: {}\n\n", i, nonModule.headerFiles[i].isSystem);
    }

    for (uint32_t i = 0; i < nonModule.huDeps.size(); i++)
    {
        print("Hu-Dep[{}] FilePath: {}\n\n", i, nonModule.huDeps[i].file.filePath);
        print("Hu-Dep[{}] FileSize: {}\n\n", i, nonModule.huDeps[i].file.fileSize);
        for (uint32_t j = 0; j < nonModule.huDeps[i].logicalNames.size(); ++j)
        {
            print("Mod-Dep[{}] LogicalName[{}]: {}\n\n", i, j, nonModule.huDeps[i].logicalNames[j]);
        }
        print("Hu-Dep[{}] User: {}\n\n", i, nonModule.huDeps[i].isSystem);
    }
}

void printMessage(const BTCLastMessage &lastMessage, const bool sent)
{
    printSendingOrReceiving(sent);
    print("BTCLastMessage\n\n");
}

TestResponse::TestResponse(string filePath_, string fileContent_, FileType fileType_, bool isSystem_)
    : filePath(std::move(filePath_)), fileContent(std::move(fileContent_)), type(fileType_), isSystem(isSystem_)
{
}
