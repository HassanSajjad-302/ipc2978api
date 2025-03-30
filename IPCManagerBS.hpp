
#ifndef IPC_MANAGER_BS_HPP
#define IPC_MANAGER_BS_HPP

#include "Manager.hpp"
#include "Messages.hpp"

// IPC Manager BuildSystem
class IPCManagerBS : public Manager
{
    string pipeName;
    bool connectedToCompiler = false;

    explicit IPCManagerBS(const string &objFilePath);
    void connectToCompiler() const;
    void receiveMessage(char (&ctbBuffer)[320], CTB &messageType);
    void sendMessage(const BTCRequestedFile &requestedFile) const;
    void sendMessage(const BTCResolvedFilePath &includePath) const;
    void sendMessage(const BTCHeaderUnitOrIncludePath &headerUnitOrIncludePath) const;
    void sendMessage(const BTCLastMessage &lastMessage) const;
};
#endif // IPC_MANAGER_BS_HPP
