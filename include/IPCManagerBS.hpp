
#ifndef IPC_MANAGER_BS_HPP
#define IPC_MANAGER_BS_HPP

#include "Manager.hpp"
#include "Messages.hpp"

namespace N2978
{

// IPC Manager BuildSystem
class IPCManagerBS : Manager
{
    friend tl::expected<IPCManagerBS, string> makeIPCManagerBS(string BMIIfHeaderUnitObjOtherwisePath);
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
    [[nodiscard]] tl::expected<void, string> sendMessage(const BTCModule &moduleFile) const;
    [[nodiscard]] tl::expected<void, string> sendMessage(const BTCNonModule &nonModule) const;
    [[nodiscard]] tl::expected<void, string> sendMessage(const BTCLastMessage &lastMessage) const;
    static tl::expected<ProcessMappingOfBMIFile, string> createSharedMemoryBMIFile(const BMIFile &bmiFile);
    static tl::expected<void, string> closeBMIFileMapping(const ProcessMappingOfBMIFile &processMappingOfBMIFile);
    void closeConnection() const;
};

tl::expected<IPCManagerBS, string> makeIPCManagerBS(string BMIIfHeaderUnitObjOtherwisePath);
} // namespace N2978
#endif // IPC_MANAGER_BS_HPP
