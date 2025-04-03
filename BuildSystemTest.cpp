#include "IPCManagerBS.hpp"
#include "Testing.hpp"

#include <chrono>
#include <print>
#include <thread>

using std::print;

int main()
{
    IPCManagerBS manager("test");
    std::this_thread::sleep_for(std::chrono::seconds(5));
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
            BTCModule b{.filePath = generateRandomString()};
            printMessage(b, true);
            manager.sendMessage(b);
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
                nonModule.filePath = generateRandomString();
            }
            printMessage(nonModule, true);
            manager.sendMessage(nonModule);
        }

        break;

        case CTB::LAST_MESSAGE: {
            const auto &lastMessage = reinterpret_cast<CTBLastMessage &>(buffer);
            printMessage(lastMessage, false);
            BTCLastMessage btcLastMessage;
            printMessage(btcLastMessage, true);
            manager.sendMessage(btcLastMessage);
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
