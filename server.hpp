
#ifndef SERVER_HPP
#define SERVER_HPP

#include "Manager.hpp"

// IPC Manager BuildSystem
class IPCManagerBS : public Manager
{
    string pipeName;
    bool connectedToClient = false;

    explicit IPCManagerBS(string pipeName_);
    void connectToNewClient() const;
    void receiveMessage(char (&ctbBuffer)[320]);
    static vector<char> getBufferWithType(BTC type);
    static void writeString(vector<char> &buffer, const string &str);
    void sendMessage(const BTC_RequestedFile &requestedFile) const;
    void sendMessage(const BTC_IncludePath &includePath) const;
    void sendMessage(const BTC_HeaderUnitOrIncludePath &headerUnitOrIncludePath) const;
    void sendMessage(const BTC_LastMessage &lastMessage) const;
};
#endif // SERVER_HPP
