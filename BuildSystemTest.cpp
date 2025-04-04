#include "IPCManagerBS.hpp"
#include "Testing.hpp"

#include <chrono>
#include <print>
#include <thread>

using std::print;

int main()
{
    IPCManagerBS manager("test");
    std::this_thread::sleep_for(std::chrono::seconds(10));
    char buffer[320];

    while (true)
    {
        bool loopExit = false;
        CTB type;
        manager.receiveMessage(buffer, type);
        switch (type)
        {

        case CTB::MODULE: {
            const auto &ctbModule = reinterpret_cast<CTBModule &>(buffer);
            printMessage(ctbModule, false);
            BTCModule b{.filePath = getRandomString()};
            manager.sendMessage(b);
            printMessage(b, true);
        }

        break;

        case CTB::NON_MODULE: {
            const auto &ctbNonModule = reinterpret_cast<CTBNonModule &>(buffer);
            printMessage(ctbNonModule, false);
            BTCNonModule nonModule;
            nonModule.found = getRandomBool();
            if (nonModule.found)
            {
                nonModule.isHeaderUnit = getRandomBool();
                nonModule.filePath = getRandomString();
            }
            manager.sendMessage(nonModule);
            printMessage(nonModule, true);
        }

        break;

        case CTB::LAST_MESSAGE: {
            const auto &lastMessage = reinterpret_cast<CTBLastMessage &>(buffer);
            printMessage(lastMessage, false);
            if (lastMessage.exitStatus == EXIT_SUCCESS)
            {
                BTCLastMessage btcLastMessage;
                manager.sendMessage(btcLastMessage);
                printMessage(btcLastMessage, true);
            }
            loopExit = true;
        }

        break;
        }

        if (loopExit)
        {
            break;
        }
    }
    print("Exiting");
}
