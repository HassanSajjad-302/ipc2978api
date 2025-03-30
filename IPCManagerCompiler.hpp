
#ifndef IPC_MANAGER_COMPILER_HPP
#define IPC_MANAGER_COMPILER_HPP

#include "Manager.hpp"

// IPC Manager BuildSystem
class IPCManagerCompiler : public Manager
{
    string pipeName;
    bool connectedToBuildSystem = false;

    void connectToBuildSystem();

  public:
    explicit IPCManagerCompiler(const string &objFilePath);
    void receiveMessage(char (&ctbBuffer)[320], BTC &messageType) const;
    void sendMessage(const CTBModule &moduleName);
    void sendMessage(const CTBHeaderUnit &headerUnitPath);
    void sendMessage(const CTBResolveInclude &resolveInclude);
    void sendMessage(const CTBResolveHeaderUnit &resolveHeaderUnit);
    void sendMessage(const CTBHeaderUnitIncludeTranslation &huIncTranslation);
    void sendMessage(const CTBLastMessage &lastMessage) const;
};

#endif // IPC_MANAGER_COMPILER_HPP
