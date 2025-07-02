#include "IPCManagerBS.hpp"
#include "IPCManagerCompiler.hpp"
#include "Testing.hpp"
#include "fmt/printf.h"

#include <chrono>
#include <filesystem>
#include <iostream>
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

void exitFailure(const string &r)
{

    print(stderr, "{}\n", r);
    print("Test Failed\n");
    exit(EXIT_FAILURE);
}

int main()
{
    const auto &r = makeIPCManagerBS((std::filesystem::current_path() / "test").string());
    if (!r)
    {
        exitFailure(r.error());
    }

    const IPCManagerBS &manager = r.value();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Listening";
    fflush(stdout);
    char buffer[320];

    CTB type;
    while (true)
    {
        bool loopExit = false;

        if (const auto &r2 = manager.receiveMessage(buffer, type); !r2)
        {
            exitFailure(r2.error());
        }

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
            if (const auto &r2 = manager.sendMessage(b); !r2)
            {
                exitFailure(r2.error());
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
                exitFailure(r2.error());
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
                    exitFailure(r.error());
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

    if (const auto &r2 = manager.receiveMessage(buffer, type); !r2)
    {
        exitFailure(r2.error());
    }

    const auto &lastMessage = reinterpret_cast<CTBLastMessage &>(buffer);
    printMessage(lastMessage, false);
    print("FileContents\n");
    BMIFile file;
    file.filePath = (std::filesystem::current_path() / "bmi.txt").generic_string();
    file.fileSize = lastMessage.fileSize;
    if (const auto &r2 =  IPCManagerCompiler::readSharedMemoryBMIFile(file); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        print("File Content:\n\n", r2.value().file);
    }
    if (lastMessage.exitStatus == EXIT_SUCCESS)
    {
        constexpr BTCLastMessage btcLastMessage;

        if (const auto &r2 = manager.sendMessage(btcLastMessage); !r2)
        {
            exitFailure(r2.error());
        }
        printMessage(btcLastMessage, true);
    }

    print("Exiting");
}
