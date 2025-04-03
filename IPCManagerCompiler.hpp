
#ifndef IPC_MANAGER_COMPILER_HPP
#define IPC_MANAGER_COMPILER_HPP

#include "Manager.hpp"
#include <print>

using std::print;

// IPC Manager BuildSystem
class IPCManagerCompiler : public Manager
{
    string pipeName;
    BTC expectedMessageType;
    bool connectedToBuildSystem = false;

    void connectToBuildSystem();

  public:
    explicit IPCManagerCompiler(const string &objFilePath);
    template <typename T> T receiveMessage() const;
    void sendMessage(const CTBModule &moduleName);
    void sendMessage(const CTBNonModule &nonModule);
    void sendMessage(const CTBLastMessage &lastMessage);
};

template <typename T> T IPCManagerCompiler::receiveMessage() const
{
    // Read from the pipe.
    char buffer[BUFFERSIZE];
    uint32_t bytesRead;
    read(buffer, bytesRead);

    uint32_t bytesProcessed = 0;

    bool unexpectedMessage = false;
    bool bytesEqual = true;
    if constexpr (std::is_same_v<T, BTCModule>)
    {
        if (expectedMessageType == BTC::MODULE)
        {
            BTCModule moduleFile;
            moduleFile.filePath = readStringFromPipe(buffer, bytesRead, bytesProcessed);
            if (bytesRead == bytesProcessed)
            {
                return moduleFile;
            }
            bytesEqual = false;
        }
        unexpectedMessage = true;
    }
    else if constexpr (std::is_same_v<T, BTCNonModule>)
    {
        if (expectedMessageType == BTC::NON_MODULE)
        {
            BTCNonModule nonModule;
            nonModule.found = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
            if (nonModule.found)
            {
                nonModule.isHeaderUnit = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
                nonModule.filePath = readStringFromPipe(buffer, bytesRead, bytesProcessed);
            }
            if (bytesRead == bytesProcessed)
            {
                return nonModule;
            }
            bytesEqual = false;
        }
        unexpectedMessage = true;
    }
    else if constexpr (std::is_same_v<T, BTCLastMessage>)
    {
        if (expectedMessageType == BTC::LAST_MESSAGE)
        {
            if (bytesRead == bytesProcessed)
            {
                return BTCLastMessage{};
            }
            bytesEqual = false;
        }
        unexpectedMessage = true;
    }
    else
    {
        static_assert(false && "Unknown type\n");
    }

    if (unexpectedMessage)
    {
        print("Received Unexpected Message from BuildSystem. Expected {}.", static_cast<uint8_t>(expectedMessageType));
    }

    if (!bytesEqual)
    {
        print("BytesRead {} not equal to BytesProcessed {} in receiveMessage.\n", bytesRead, bytesProcessed);
    }
}

#endif // IPC_MANAGER_COMPILER_HPP
