
#ifndef IPC_MANAGER_COMPILER_HPP
#define IPC_MANAGER_COMPILER_HPP

#include "Manager.hpp"
#include "expected.hpp"

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

    tl::expected<void, string> connectToBuildSystem();

    template <typename T> tl::expected<T, string> receiveMessage();
    // This is not exposed. sendCTBLastMessage calls this.
    tl::expected<void, string> receiveBTCLastMessage() const;

  public:
    explicit IPCManagerCompiler(const string &objFilePath);
    tl::expected<BTCModule, string> receiveBTCModule(const CTBModule &moduleName);
    tl::expected<BTCNonModule, string> receiveBTCNonModule(const CTBNonModule &nonModule);
    tl::expected<void, string> sendCTBLastMessage(const CTBLastMessage &lastMessage);
    tl::expected<void, string> sendCTBLastMessage(const CTBLastMessage &lastMessage, const string &bmiFile,
                                                  const string &filePath);
    tl::expected<string_view, string> readSharedMemoryBMIFile(const BMIFile &file);
};

template <typename T> tl::expected<T, string> IPCManagerCompiler::receiveMessage()
{
    // Read from the pipe.
    char buffer[BUFFERSIZE];
    uint32_t bytesRead;
    if (const auto &r = read(buffer); !r)
    {
        return tl::unexpected(r.error());
    }
    else
    {
        bytesRead = *r;
    }

    uint32_t bytesProcessed = 0;

    if constexpr (std::is_same_v<T, BTCModule>)
    {
        const auto &r = readMemoryMappedBMIFileFromPipe(buffer, bytesRead, bytesProcessed);
        if (!r)
        {
            return tl::unexpected(r.error());
        }

        const auto &r2 = readVectorOfModuleDepFromPipe(buffer, bytesRead, bytesProcessed);
        if (!r2)
        {
            return tl::unexpected(r2.error());
        }

        BTCModule moduleFile;
        moduleFile.requested = *r;
        moduleFile.deps = *r2;
        if (bytesRead == bytesProcessed)
        {
            memoryMappedBMIFiles.reserve(memoryMappedBMIFiles.size() + 1 + moduleFile.deps.size());
            return moduleFile;
        }
    }
    else if constexpr (std::is_same_v<T, BTCNonModule>)
    {
        const auto &r = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        if (!r)
        {
            return tl::unexpected(r.error());
        }

        const auto &r2 = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (!r2)
        {
            return tl::unexpected(r2.error());
        }

        const auto &r3 = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        if (!r3)
        {
            return tl::unexpected(r3.error());
        }

        const auto &r4 = readUInt32FromPipe(buffer, bytesRead, bytesProcessed);
        if (!r4)
        {
            return tl::unexpected(r4.error());
        }

        const auto &r5 = readVectorOfHuDepFromPipe(buffer, bytesRead, bytesProcessed);
        if (!r5)
        {
            return tl::unexpected(r5.error());
        }

        BTCNonModule nonModule;
        nonModule.isHeaderUnit = *r;
        nonModule.filePath = *r2;
        nonModule.angled = *r3;
        nonModule.fileSize = *r4;
        nonModule.deps = *r5;

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

    if (bytesRead != bytesProcessed)
    {
        IPCErr(bytesRead, bytesProcessed)
    }
}
} // namespace N2978
#endif // IPC_MANAGER_COMPILER_HPP
