#include "IPCManagerBS.hpp"
#include "Testing.hpp"
#include "fmt/printf.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#ifdef _WIN32
#include <Windows.h>
#else
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using fmt::print, std::string_view;

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
    const auto &r = makeIPCManagerBS((std::filesystem::current_path() / "test").string());
    if (!r)
    {
        print("{}\n", r.error());
        exit(EXIT_FAILURE);
    }

    const IPCManagerBS &manager = r.value();
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
            if (const auto &r2  = manager.sendMessage(b); !r2)
            {
                print("{}\n", r2.error());
                exit(EXIT_FAILURE);
            }
            printMessage(b, true);
        }

        break;

        case CTB::NON_MODULE: {
            const auto &ctbNonModule = reinterpret_cast<CTBNonModule &>(buffer);
            printMessage(ctbNonModule, false);
            BTCNonModule nonModule;
            nonModule.isHeaderUnit = getRandomBool();
            nonModule.filePath = getRandomString();
            if (const auto &r2 = manager.sendMessage(nonModule); !r2)
            {
                print("{}\n", r2.error());
                exit(EXIT_FAILURE);
            }
            printMessage(nonModule, true);
        }

        break;

        case CTB::LAST_MESSAGE: {
            const auto &lastMessage = reinterpret_cast<CTBLastMessage &>(buffer);
            printMessage(lastMessage, false);
            if (lastMessage.exitStatus == EXIT_SUCCESS)
            {
                BTCLastMessage btcLastMessage;
                if (const auto &r2 = manager.sendMessage(btcLastMessage); !r2)
                {
                    print("{}\n", r2.error());
                    exit(EXIT_FAILURE);
                }
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
        const BTCLastMessage btcLastMessage;

        if (const auto &r2 = manager.sendMessage(btcLastMessage); !r2)
        {
            print("", r2.error());
            exit(EXIT_FAILURE);
        }
        printMessage(btcLastMessage, true);
    }

    print("Exiting");
}
