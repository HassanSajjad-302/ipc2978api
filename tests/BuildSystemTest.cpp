#include "IPCManagerBS.hpp"
#include "Testing.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <print>
#include <thread>
#ifdef _WIN32
#include <Windows.h>
#else
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using std::print, std::string_view;

tl::expected<string_view, string> readSharedMemoryBMIFile(const BMIFile &file)
{
#ifdef _WIN32
    // 1) Open the existing file‐mapping object (must have been created by another process)
    const HANDLE mapping = OpenFileMappingA(FILE_MAP_READ,       // read‐only access
                                            FALSE,               // do not inherit a handle
                                            file.filePath.data() // name of mapping
    );

    if (mapping == nullptr)
    {
        return tl::unexpected(string{});
    }

    // 2) Map a view of the file into our address space
    const LPVOID view = MapViewOfFile(mapping,       // handle to mapping object
                                      FILE_MAP_READ, // read‐only view
                                      0,             // file offset high
                                      0,             // file offset low
                                      file.fileSize  // number of bytes to map (0 maps the whole file)
    );

    if (view == nullptr)
    {
        CloseHandle(mapping);
        return tl::unexpected(string{});
    }

    MemoryMappedBMIFile f{};
    f.mapping = mapping;
    f.view = view;
    return string_view{static_cast<char *>(view), file.fileSize};
#else
    const int fd = open(file.filePath.data(), O_RDONLY);
    if (fd == -1)
    {
        return tl::unexpected(getErrorString());
    }
    void *mapping = mmap(nullptr, file.fileSize, PROT_READ, MAP_SHARED | MAP_POPULATE, fd, 0);

    if (close(fd) == -1)
    {
        return tl::unexpected(getErrorString());
    }

    if (mapping == MAP_FAILED)
    {
        return tl::unexpected(getErrorString());
    }

    MemoryMappedBMIFile f{};
    f.mapping = mapping;
    f.mappingSize = file.fileSize;
    return string_view{static_cast<char *>(mapping), file.fileSize};
#endif
}
int main()
{
    const IPCManagerBS &manager = makeIPCManagerBS((std::filesystem::current_path() / "test").string()).value();
    // std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "Listening";
    fflush(stdout);
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
            BMIFile file;
            file.filePath = getRandomString();
            file.fileSize = 0;
            BTCModule b;
            b.requested = std::move(file);
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
