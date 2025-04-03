
#include "Testing.hpp"

string getRandomString()
{
    const string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);
    std::uniform_int_distribution distribution2(0, 10000);
    const uint64_t length = distribution2(generator);
    string random_string(length, '\0');
    for (int i = 0; i < length; ++i)
    {
        random_string += characters[distribution(generator)];
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
        print("HasLogicalName: {}\n\n", lastMessage.hasLogicalName);
        print("HeaderFiles Size: {}\n\n", lastMessage.headerFiles.size());
        for (uint32_t i = 0; i < lastMessage.headerFiles.size(); i++)
        {
            print("HeaderFiles[{}]: {}\n\n", i, lastMessage.headerFiles[i]);
        }
        print("Output: {}\n\n", lastMessage.output);
        print("ErrorOutput: {}\n\n", lastMessage.errorOutput);
        print("OutputFilePaths Size: {}\n\n", lastMessage.outputFilePaths.size());
        for (uint32_t i = 0; i < lastMessage.outputFilePaths.size(); i++)
        {
            print("OutputFilePaths[{}]: {}\n\n", i, lastMessage.outputFilePaths[i]);
        }
        if (lastMessage.hasLogicalName)
        {
            print("LogicalName: {}\n\n", lastMessage.hasLogicalName);
        }
    }
}

void printMessage(const BTCModule &btcModule, const bool sent)
{
    printSendingOrReceiving(sent);
    print("BTCModule\n\n");
    print("Module filepath: {}\n\n", btcModule.filePath);
}

void printMessage(const BTCNonModule &nonModule, const bool sent)
{
    printSendingOrReceiving(sent);
    print("BTCNonModule\n\n");
    print("Found {}\n\n", nonModule.found);
    print("IsHeaderUnit {}\n\n", nonModule.isHeaderUnit);
    print("FilePath {}\n\n", nonModule.filePath);
}

void printMessage(const BTCLastMessage &lastMessage, const bool sent)
{
    printSendingOrReceiving(sent);
    print("BTCLastMessage\n\n");
}
