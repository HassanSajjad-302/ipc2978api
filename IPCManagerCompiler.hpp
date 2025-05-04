
#ifndef IPC_MANAGER_COMPILER_HPP
#define IPC_MANAGER_COMPILER_HPP

#include "Manager.hpp"
#include <print>

using std::print;

// IPC Manager BuildSystem
class IPCManagerCompiler : public Manager
{
    string pipeName;
    bool connectedToBuildSystem = false;

    void connectToBuildSystem();

    template <typename T> T receiveMessage() const;

  public:
    explicit IPCManagerCompiler(const string &objFilePath);
    BTCModule receiveBTCModule(const CTBModule &moduleName);
    BTCNonModule receiveBTCNonModule(const CTBNonModule &nonModule);
    void sendBTCLastMessage(const CTBLastMessage &lastMessage);
    // The BMI file-path should be the first in the outputs in CTBLastMessage::outputFilePaths
    void sendBTCLastMessage(const CTBLastMessage &lastMessage, const string &bmiFile);
};

template <typename T> T IPCManagerCompiler::receiveMessage() const
{
    // Read from the pipe.
    char buffer[BUFFERSIZE];
    uint32_t bytesRead;
    read(buffer, bytesRead);

    uint32_t bytesProcessed = 0;

    bool bytesEqual = true;
    if constexpr (std::is_same_v<T, BTCModule>)
    {
        BTCModule moduleFile;
        moduleFile.filePath = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (bytesRead == bytesProcessed)
        {
            return moduleFile;
        }
        bytesEqual = false;
    }
    else if constexpr (std::is_same_v<T, BTCNonModule>)
    {
        BTCNonModule nonModule;
        nonModule.found = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        nonModule.isHeaderUnit = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        nonModule.filePath = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (bytesRead == bytesProcessed)
        {
            return nonModule;
        }
        bytesEqual = false;
    }
    else if constexpr (std::is_same_v<T, BTCLastMessage>)
    {
        bytesProcessed = 1;
        if (buffer[0] != UINT32_MAX)
        {
            print("Incorrect Last Message Received\n");
            if (bytesRead == bytesProcessed)
            {
                return BTCLastMessage{};
            }
            bytesEqual = false;
        }
    }
    else
    {
        static_assert(false && "Unknown type\n");
    }

    if (!bytesEqual)
    {
        print("BytesRead {} not equal to BytesProcessed {} in receiveMessage.\n", bytesRead, bytesProcessed);
    }
}

#endif // IPC_MANAGER_COMPILER_HPP
