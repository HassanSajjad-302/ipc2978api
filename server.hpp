
#ifndef SERVER_HPP
#define SERVER_HPP

#include "Manager.hpp"

// IPC Manager BuildSystem
class IPCManagerBS : public Manager
{
    string pipeName;
    bool connectedToClient = false;

    explicit IPCManagerBS(const string& objFilePath);
    void connectToNewClient() const;
    void receiveMessage(char (&ctbBuffer)[320], CTB &messageType);
    void sendMessage(const BTC_RequestedFile &requestedFile) const;
    void sendMessage(const BTC_ResolvedFilePath &includePath) const;
    void sendMessage(const BTC_HeaderUnitOrIncludePath &headerUnitOrIncludePath) const;
    void sendMessage(const BTC_LastMessage &lastMessage) const;
};
#endif // SERVER_HPP
