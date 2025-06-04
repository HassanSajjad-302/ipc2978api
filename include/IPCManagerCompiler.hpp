
#ifndef IPC_MANAGER_COMPILER_HPP
#define IPC_MANAGER_COMPILER_HPP

#include "Manager.hpp"

using std::string_view;
namespace N2978
{

struct MemoryMappedBMIFile
{
    void *mapping;
    void *view;
};

// IPC Manager BuildSystem
class IPCManagerCompiler : public Manager
{
    string pipeName;
    vector<MemoryMappedBMIFile> memoryMappedBMIFiles;
    bool connectedToBuildSystem = false;

    void connectToBuildSystem();

    template <typename T> T receiveMessage();
    // This is not exposed. sendCTBLastMessage calls this.
    void receiveBTCLastMessage() const;

    void checkBytesReadEqualBytesProcessed(uint32_t bytesRead, uint32_t bytesProcessed);

  public:
    explicit IPCManagerCompiler(const string &objFilePath);
    BTCModule receiveBTCModule(const CTBModule &moduleName);
    BTCNonModule receiveBTCNonModule(const CTBNonModule &nonModule);
    void sendCTBLastMessage(const CTBLastMessage &lastMessage);
    void sendCTBLastMessage(const CTBLastMessage &lastMessage, const string &bmiFile, const string &filePath);
    string_view readSharedMemoryBMIFile(const BMIFile &file);
};

template <typename T> T IPCManagerCompiler::receiveMessage()
{
    // Read from the pipe.
    char buffer[BUFFERSIZE];
    uint32_t bytesRead;
    read(buffer, bytesRead);

    uint32_t bytesProcessed = 0;

    if constexpr (std::is_same_v<T, BTCModule>)
    {
        BTCModule moduleFile;
        moduleFile.requested = readMemoryMappedBMIFileFromPipe(buffer, bytesRead, bytesProcessed);
        moduleFile.deps = readVectorOfModuleDepFromPipe(buffer, bytesRead, bytesProcessed);
        if (bytesRead == bytesProcessed)
        {
            memoryMappedBMIFiles.reserve(memoryMappedBMIFiles.size() + 1 + moduleFile.deps.size());
            return moduleFile;
        }
    }
    else if constexpr (std::is_same_v<T, BTCNonModule>)
    {
        BTCNonModule nonModule;
        nonModule.isHeaderUnit = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        nonModule.filePath = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        nonModule.angled = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        nonModule.fileSize = readUInt32FromPipe(buffer, bytesRead, bytesProcessed);
        nonModule.deps = readVectorOfHuDepFromPipe(buffer, bytesRead, bytesProcessed);
        if (bytesRead == bytesProcessed)
        {
            if (nonModule.fileSize != UINT32_MAX)
            {
                memoryMappedBMIFiles.reserve(memoryMappedBMIFiles.size() + 1 + nonModule.deps.size());
            }
            return nonModule;
        }
    }
    else
    {
        static_assert(false && "Unknown type\n");
    }

    checkBytesReadEqualBytesProcessed(bytesRead, bytesProcessed);
}
} // namespace N2978
#endif // IPC_MANAGER_COMPILER_HPP
