#include "IPCManagerBS.hpp"
#include "Testing.hpp"

#include <Windows.h>
#include <chrono>
#include <print>
#include <thread>

using std::print;

std::string_view readSharedMemoryBMIFile(const BMIFile &file)
{
    // 1) Open the existing file‐mapping object (must have been created by another process)
    const HANDLE mapping = OpenFileMapping(FILE_MAP_READ, // read‐only access
                                           FALSE, // do not inherit handle
                                           file.filePath.data() // name of mapping
        );

    if (mapping == nullptr)
    {
        print("Could not open file mapping of file {}.\n", file.filePath);
    }

    // 2) Map a view of the file into our address space
    const LPVOID view = MapViewOfFile(mapping, // handle to mapping object
                                      FILE_MAP_READ, // read‐only view
                                      0, // file offset high
                                      0, // file offset low
                                      file.fileSize // number of bytes to map (0 maps the whole file)
        );

    if (view == nullptr)
    {
        print("Could not open view of file mapping of file {}.\n", file.filePath);
        CloseHandle(mapping);
    }

    return {static_cast<char *>(view), file.fileSize};
}

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
            BMIFile file{.filePath = getRandomString(), .fileSize = 0};
            BTCModule b{.requested = std::move(file)};
            manager.sendMessage(b);
            printMessage(b, true);
        }

        break;

        case CTB::NON_MODULE: {
            const auto &ctbNonModule = reinterpret_cast<CTBNonModule &>(buffer);
            printMessage(ctbNonModule, false);
            BTCNonModule nonModule;
            nonModule.isHeaderUnit = getRandomBool();
            nonModule.filePath = getRandomString();
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

    const auto &lastMessage = reinterpret_cast<CTBLastMessage &>(buffer);
    printMessage(lastMessage, false);
    if (lastMessage.exitStatus == EXIT_SUCCESS)
    {
        BTCLastMessage btcLastMessage;

        manager.sendMessage(btcLastMessage);
        printMessage(btcLastMessage, true);
    }

    print("Exiting");
}
