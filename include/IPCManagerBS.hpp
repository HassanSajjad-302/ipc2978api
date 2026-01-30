
#ifndef IPC_MANAGER_BS_HPP
#define IPC_MANAGER_BS_HPP

#include "Manager.hpp"
#include "Messages.hpp"

namespace N2978
{

// IPC Manager BuildSystem
class IPCManagerBS : public Manager
{
  public:
    // This is unused as build-system will read the string externally on the read-event on the following fd/HANDLE and
    // will assign it to the serverReadString which is then read by this class.
    uint64_t readFd = 0;
    uint64_t writeFd = 0;

    std::string_view serverReadString;

    tl::expected<uint32_t, std::string> readInternal(char (&buffer)[BUFFERSIZE]) const override;
    tl::expected<void, std::string> writeInternal(const std::string &buffer) const override;

    explicit IPCManagerBS(uint64_t readFd_, uint64_t writeFd_);
    tl::expected<void, std::string> receiveMessage(char (&ctbBuffer)[320], CTB &messageType) const;
    [[nodiscard]] tl::expected<void, std::string> sendMessage(const BTCModule &moduleFile) const;
    [[nodiscard]] tl::expected<void, std::string> sendMessage(const BTCNonModule &nonModule) const;
    [[nodiscard]] tl::expected<void, std::string> sendMessage(const BTCLastMessage &lastMessage) const;
    static tl::expected<ProcessMappingOfBMIFile, std::string> createSharedMemoryBMIFile(BMIFile &bmiFile);
    static tl::expected<void, std::string> closeBMIFileMapping(const ProcessMappingOfBMIFile &processMappingOfBMIFile);
};
} // namespace N2978
#endif // IPC_MANAGER_BS_HPP
