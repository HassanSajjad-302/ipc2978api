
#ifndef IPC_MANAGER_BS_HPP
#define IPC_MANAGER_BS_HPP

#include "Manager.hpp"
#include "Messages.hpp"

namespace N2978
{

// IPC Manager BuildSystem
class IPCManagerBS : public Manager
{
    string pipeName;
    bool connectedToCompiler = false;

    void connectToCompiler();

  public:
    explicit IPCManagerBS(const string &objFilePath);
    void receiveMessage(char (&ctbBuffer)[320], CTB &messageType);
    void sendMessage(const BTCModule &moduleFile) const;
    void sendMessage(const BTCNonModule &nonModule) const;
    void sendMessage(const BTCLastMessage &lastMessage) const;
    static void *createSharedMemoryBMIFile(const string &bmiFilePath);
};
} // namespace N2978
#endif // IPC_MANAGER_BS_HPP
