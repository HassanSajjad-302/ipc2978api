
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

  public:
    explicit IPCManagerCompiler(const string &objFilePath);
    template <typename T> T receiveMessage() const;
    void sendMessage(const CTBModule &moduleName);
    void sendMessage(const CTBHeaderUnit &headerUnitPath);
    void sendMessage(const CTBResolveInclude &resolveInclude);
    void sendMessage(const CTBResolveHeaderUnit &resolveHeaderUnit);
    void sendMessage(const CTBHeaderUnitIncludeTranslation &huIncTranslation);
    void sendMessage(const CTBLastMessage &lastMessage) const;
};

template <typename T> T IPCManagerCompiler::receiveMessage() const
{
    // Read from the pipe.
    char buffer[BUFFERSIZE];
    uint32_t bytesRead;
    read(buffer, bytesRead);

    uint32_t bytesProcessed = 1;

    BTC messageType;
    bool bytesEqual = true;
    if constexpr (std::is_same_v<T, BTCRequestedFile>)
    {
        if (static_cast<BTC>(buffer[0]) == BTC::REQUESTED_FILE)
        {
            BTCRequestedFile requestedFile;
            requestedFile.filePath = readStringFromPipe(buffer, bytesRead, bytesProcessed);
            if (bytesRead == bytesProcessed)
            {
                return requestedFile;
            }
            bytesEqual = false;
        }
        messageType = BTC::REQUESTED_FILE;
    }

    else if constexpr (std::is_same_v<T, BTCResolvedFilePath>)
    {
        if (static_cast<BTC>(buffer[0]) == BTC::RESOLVED_FILEPATH)
        {
            BTCResolvedFilePath resolvedFilePath;
            resolvedFilePath.filePath = readStringFromPipe(buffer, bytesRead, bytesProcessed);
            if (bytesRead == bytesProcessed)
            {
                return resolvedFilePath;
            }
            bytesEqual = false;
        }
        messageType = BTC::RESOLVED_FILEPATH;
    }
    else if constexpr (std::is_same_v<T, BTCHeaderUnitOrIncludePath>)
    {
        if (static_cast<BTC>(buffer[0]) == BTC::HEADER_UNIT_OR_INCLUDE_PATH)
        {
            BTCHeaderUnitOrIncludePath headerUnitOrIncludePath;
            headerUnitOrIncludePath.exists = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
            if (headerUnitOrIncludePath.exists)
            {
                headerUnitOrIncludePath.isHeaderUnit = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
                headerUnitOrIncludePath.filePath = readStringFromPipe(buffer, bytesRead, bytesProcessed);
            }
            if (bytesRead == bytesProcessed)
            {
                return headerUnitOrIncludePath;
            }
            bytesEqual = false;
        }
        messageType = BTC::HEADER_UNIT_OR_INCLUDE_PATH;
    }
    else if constexpr (std::is_same_v<T, BTCLastMessage>)
    {
        if (static_cast<BTC>(buffer[0]) == BTC::LAST_MESSAGE)
        {
            if (bytesRead == bytesProcessed)
            {
                return BTCLastMessage{};
            }
            bytesEqual = false;
        }
        messageType = BTC::LAST_MESSAGE;
    }
    else
    {
        static_assert(false && "Unknown type\n");
    }

    if (!bytesEqual)
    {
        print("BytesRead {} not equal to BytesProcessed {} in receiveMessage.\n", bytesRead, bytesProcessed);
    }

    if (messageType != static_cast<BTC>(buffer[0]))
    {
        print("Received Unexpected Message type from BuildSystem. Expected {} Received {}\n",
              static_cast<uint8_t>(messageType), buffer[0]);
    }
}

#endif // IPC_MANAGER_COMPILER_HPP
