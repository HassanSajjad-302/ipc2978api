
#include "IPCManagerCompiler.hpp"
#include "Testing.hpp"

#include <chrono>
#include <print>
#include <random>
#include <string>
#include <thread>

int main()
{
    IPCManagerCompiler manager("test");
    std::uniform_int_distribution distribution(0, 20);
    for (uint64_t i = 0; i < distribution(generator); ++i)
    {
        if (getRandomBool())
        {
            CTBModule ctbModule;
            ctbModule.moduleName = getRandomString();
            manager.sendMessage(ctbModule);
            printMessage(ctbModule, true);
            BTCModule btcModule = manager.receiveMessage<BTCModule>();
            printMessage(btcModule, false);
        }
        else
        {
            CTBNonModule nonModule;
            nonModule.isHeaderUnit = getRandomBool();
            nonModule.str = "3";
            manager.sendMessage(nonModule);
            printMessage(nonModule, true);
            BTCNonModule btcNonModule = manager.receiveMessage<BTCNonModule>();
            printMessage(btcNonModule, false);
        }
    }

    CTBLastMessage ctbLastMessage;
    ctbLastMessage.exitStatus = getRandomBool();
    manager.sendMessage(ctbLastMessage);
    printMessage(ctbLastMessage, true);
    if (ctbLastMessage.exitStatus == EXIT_SUCCESS)
    {
        BTCLastMessage btcLastMessage = manager.receiveMessage<BTCLastMessage>();
        printMessage(btcLastMessage, false);
    }
}
