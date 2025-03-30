
#ifndef IPC_MANAGER_COMPILER_HPP
#define IPC_MANAGER_COMPILER_HPP

#include "Manager.hpp"

// IPC Manager BuildSystem
class IPCManagerCompiler : public Manager
{
    string pipeName;
    bool connectedToBuildSystem = false;

    explicit IPCManagerCompiler(const string &objFilePath);
    void connectToBuildSystem();
    void receiveMessage(char (&ctbBuffer)[320], BTC &messageType) const;
    void sendMessage(const CTBModule &moduleName);
    void sendMessage(const CTBHeaderUnit &headerUnitPath) const;
    void sendMessage(const CTBResolveInclude &resolveInclude) const;
    void sendMessage(const CTBResolveHeaderUnit &resolveHeaderUnit) const;
    void sendMessage(const CTBHeaderUnitIncludeTranslation &huIncTranslation) const;
    void sendMessage(const CTBLastMessage &lastMessage) const;
};

#endif // IPC_MANAGER_COMPILER_HPP
