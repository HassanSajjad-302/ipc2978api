
#include "Testing.hpp"
#include "fmt/printf.h"

using fmt::print;
string getRandomString()
{
    const string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);
    std::uniform_int_distribution distribution2(0, 10000);
    const uint64_t length = distribution2(generator);
    string random_string(length, '\0');
    for (int i = 0; i < length; ++i)
    {
        random_string[i] = characters[distribution(generator)];
    }

    return random_string;
}

bool getRandomBool()
{
    std::uniform_int_distribution distribution(0, 1);
    return distribution(generator);
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
    print("str: {}\n\n", nonModule.str);
}

void printMessage(const CTBLastMessage &lastMessage, const bool sent)
{
    printSendingOrReceiving(sent);
    print("CTBLastMessage\n\n");
    print("ExitStatus: {}\n\n", lastMessage.exitStatus);
    if (lastMessage.exitStatus)
    {
        print("HeaderFiles Size: {}\n\n", lastMessage.headerFiles.size());
        for (uint32_t i = 0; i < lastMessage.headerFiles.size(); i++)
        {
            print("HeaderFiles[{}]: {}\n\n", i, lastMessage.headerFiles[i]);
        }
        print("Output: {}\n\n", lastMessage.output);
        print("ErrorOutput: {}\n\n", lastMessage.errorOutput);
        print("LogicalName: {}\n\n", lastMessage.logicalName);
        print("FileSize: {}\n\n", lastMessage.fileSize);
    }
}

void printMessage(const BTCModule &btcModule, const bool sent)
{
    printSendingOrReceiving(sent);
    print("BTCModule\n\n");

    print("Requested FilePath: {}\n\n", btcModule.requested.filePath);
    print("Requested FileSize: {}\n\n", btcModule.requested.fileSize);
    print("Deps Size: {}\n\n", btcModule.deps.size());
    for (uint32_t i = 0; i < btcModule.deps.size(); i++)
    {
        print("Deps[{}] FilePath: {}\n\n", i, btcModule.deps[i].file.filePath);
        print("Deps[{}] FileSize: {}\n\n", i, btcModule.deps[i].file.fileSize);
        print("Deps[{}] LogicalName: {}\n\n", i, btcModule.deps[i].logicalName);
    }
}

void printMessage(const BTCNonModule &nonModule, const bool sent)
{
    printSendingOrReceiving(sent);
    print("BTCNonModule\n\n");
    print("IsHeaderUnit {}\n\n", nonModule.isHeaderUnit);
    print("FilePath {}\n\n", nonModule.filePath);
    print("FileSize {}\n\n", nonModule.fileSize);
    for (uint32_t i = 0; i < nonModule.deps.size(); i++)
    {
        print("Deps[{}] FilePath: {}\n\n", i, nonModule.deps[i].file.filePath);
        print("Deps[{}] FileSize: {}\n\n", i, nonModule.deps[i].file.fileSize);
        print("Deps[{}] LogicalName: {}\n\n", i, nonModule.deps[i].logicalName);
        print("Deps[{}] Angled: {}\n\n", i, nonModule.deps[i].angled);
    }
}

void printMessage(const BTCLastMessage &lastMessage, const bool sent)
{
    printSendingOrReceiving(sent);
    print("BTCLastMessage\n\n");
}
