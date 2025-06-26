
#ifndef IPC_MANAGER_BS_HPP
#define IPC_MANAGER_BS_HPP

#include "Manager.hpp"
#include "Messages.hpp"

namespace N2978
{

// IPC Manager BuildSystem
class IPCManagerBS : public Manager
{
    friend tl::expected<IPCManagerBS, string> makeIPCManagerBS(const string& BMIIfHeaderUnitObjOtherwisePath);
    bool connectedToCompiler = false;

#ifdef _WIN32
    explicit IPCManagerBS(void *hPipe_);
#else
    explicit IPCManagerBS(int fdSocket_);
#endif

  public:
    IPCManagerBS(const IPCManagerBS &) = default;
    IPCManagerBS &operator=(const IPCManagerBS &) = default;
    IPCManagerBS(IPCManagerBS &&) = default;
    IPCManagerBS &operator=(IPCManagerBS &&) = default;
    tl::expected<void, string> receiveMessage(char (&ctbBuffer)[320], CTB &messageType) const;
    tl::expected<void, string> sendMessage(const BTCModule &moduleFile) const;
    tl::expected<void, string> sendMessage(const BTCNonModule &nonModule) const;
    tl::expected<void, string> sendMessage(const BTCLastMessage &lastMessage) const;
    static tl::expected<MemoryMappedBMIFile, string> createSharedMemoryBMIFile(const BMIFile &bmiFile);
    static tl::expected<void, string> deleteBMIFileMapping(const MemoryMappedBMIFile &memoryMappedBMIFile);
};

tl::expected<IPCManagerBS, string> makeIPCManagerBS(const string& BMIIfHeaderUnitObjOtherwisePath);
} // namespace N2978
#endif // IPC_MANAGER_BS_HPP
