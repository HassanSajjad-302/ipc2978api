#include "IPCManagerBS.hpp"
#include "IPCManagerCompiler.hpp"
#include "Testing.hpp"
#include "fmt/printf.h"
#include <chrono>
#include <corecrt_io.h>
#include <filesystem>
#include <fstream>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#else
#include "sys/epoll.h"
#include <fcntl.h>
#include <unistd.h>
#endif

using fmt::print, std::string_view;

bool first = true;
std::thread *thr;

#ifdef _WIN32
#define COMPILER_TEST ".\\CompilerTest.exe"
#else
#define COMPILER_TEST "./CompilerTest"
#endif

struct BuildSystemTest
{

    //  Compiler can use this function to read the BMI file. BMI should be read using this function to conserve memory.
    static tl::expected<ProcessMappingOfBMIFile, std::string> readSharedMemoryBMIFile(const BMIFile &file)
    {
        return IPCManagerCompiler::readSharedMemoryBMIFile(file);
    }
};

void listenForCompiler()
{
    if (first)
    {
        // We launch CompilerTest and then sleep. By the time we wake up, CompilerTest has already reached its blocking
        // call.
        thr = new std::thread([] {
            if (system(COMPILER_TEST " > test1.txt") != EXIT_SUCCESS)
            {
                exitFailure("Error running ClientTest first time");
            }
        });
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    else
    {
        // We launch a new thread. This thread sleeps before executing the CompilerTest. By the time it executes
        // CompilerTest, the BuildSystemTest has already reached its blocking call.
        thr = new std::thread([] {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (system(COMPILER_TEST " > test2.txt") != EXIT_SUCCESS)
            {
                exitFailure("Error running ClientTest second time");
            }
        });
    }
}

std::string readCompilerMessage(const uint64_t serverFd, const uint64_t fd)
{

#ifdef _WIN32
    HANDLE hIOCP = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(serverFd));
    HANDLE hPipe = reinterpret_cast<HANDLE>(fd);

    std::string str;
    while (true)
    {
        char buffer[4096];
        DWORD bytesRead = 0;
        OVERLAPPED overlapped = {0};

        // Initiate async read. Even if it is completed successfully, we will get the completion packet. We don't get
        // the packet only if it fails immediately with error other than ERROR_IO_PENDING
        BOOL result = ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, &overlapped);

        DWORD error = GetLastError();
        if (!result && error != ERROR_IO_PENDING)
        {
            exitFailure(getErrorString());
        }

        // Wait for the read to complete.
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED completedOverlapped = nullptr;

        if (!GetQueuedCompletionStatus(hIOCP, &bytesRead, &completionKey, &completedOverlapped, INFINITE))
        {
            exitFailure(getErrorString());
        }

        // Verify completion is for our pipe
        if (completionKey != (ULONG_PTR)hPipe)
        {
            exitFailure("Unexpected completion key");
        }

        if (bytesRead == 0)
        {
            exitFailure("Pipe closed or no data read");
        }

        // Append read data to string
        for (DWORD i = 0; i < bytesRead; ++i)
        {
            str.push_back(buffer[i]);
        }

        // Check for terminator
        if (str[str.size() - 1] == ';')
        {
            str.pop_back();
            break;
        }
    }

    return str;

#else

    epoll_event ev{};
    ev.events = EPOLLIN;
    if (epoll_ctl(serverFd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        exitFailure(getErrorString());
    }

    epoll_wait(serverFd, &ev, 1, -1);
    std::string str;
    while (true)
    {
        char buffer[4096];
        const int readCount = read(fd, buffer, 4096);
        if (readCount == 0 || readCount == -1)
        {
            exitFailure(getErrorString());
        }
        for (uint32_t i = 0; i < readCount; ++i)
        {
            str.push_back(buffer[i]);
        }
        if (str[str.size() - 1] != ';')
        {
            continue;
        }
        str.pop_back();
        break;
    }

    if (epoll_ctl(serverFd, EPOLL_CTL_DEL, fd, &ev) == -1)
    {
        exitFailure(getErrorString());
    }
    return str;
#endif
}

void closeHandle(uint64_t fd)
{
#ifdef _WIN32
    // CloseHandle returns non-zero on success, so we need to check for failure (zero)
    if (!CloseHandle(reinterpret_cast<HANDLE>(fd)))
    {
        exitFailure(getErrorString());
    }
#else
    if (close(fd) == -1)
    {
        exitFailure(getErrorString());
    }
#endif
}

void completeConnection(IPCManagerBS &manager, int serverFd)
{
#ifdef _WIN32
    HANDLE hIOCP = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(serverFd));
    HANDLE hPipe = reinterpret_cast<HANDLE>(manager.fd);

    if (const auto &r2 = manager.completeConnection(); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        if (!*r2)
        {
            // Connection is pending (ERROR_IO_PENDING)
            // Wait for IOCP to signal completion

            DWORD bytesTransferred = 0;
            ULONG_PTR completionKey = 0;
            LPOVERLAPPED overlapped = nullptr;

            if (!GetQueuedCompletionStatus(hIOCP, &bytesTransferred, &completionKey, &overlapped,
                                           INFINITE)) // Wait indefinitely
            {
                exitFailure(getErrorString());
            }

            // Verify this completion is for our pipe
            if (completionKey != (ULONG_PTR)hPipe)
            {
                exitFailure("Unexpected completion key");
            }

            // Connection is now complete - no need to call completeConnection again
            // The ConnectNamedPipe operation has finished
        }
        // else: connection completed synchronously, already done
    }
#else
    if (const auto &r2 = manager.completeConnection(); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        if (!*r2)
        {
            epoll_event ev{};
            ev.events = EPOLLIN;
            if (epoll_ctl(serverFd, EPOLL_CTL_ADD, manager.fd, &ev) == -1)
            {
                exitFailure(getErrorString());
            }

            epoll_event ev2{};
            if (epoll_wait(serverFd, &ev2, 1, -1) == -1)
            {
                exitFailure(getErrorString());
            }
            if (epoll_ctl(serverFd, EPOLL_CTL_DEL, manager.fd, &ev) == -1)
            {
                exitFailure(getErrorString());
            }
            if (const auto &r3 = manager.completeConnection(); !r3)
            {
                exitFailure(r3.error());
            }
        }
    }
#endif
}

uint64_t createMultiplex()
{
#ifdef _WIN32
    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, // handle to associate
                                         nullptr,              // existing IOCP handle
                                         0,                    // completion key (use pipe handle)
                                         0                     // number of concurrent threads (0 = default)
    );
    if (iocp == nullptr)
    {
        exitFailure(getErrorString());
    }
    return reinterpret_cast<uint64_t>(iocp);
#else
    return epoll_create1(0);
#endif
}

int runTest()
{
    const uint64_t serverFd = createMultiplex();

    const auto r = makeIPCManagerBS((std::filesystem::current_path() / "test").string(), serverFd);
    if (!r)
    {
        exitFailure(r.error());
    }

    IPCManagerBS manager = r.value();
    listenForCompiler();
    fflush(stdout);

    completeConnection(manager, serverFd);

    CTB type;
    char buffer[320];
    while (true)
    {
        bool loopExit = false;

        string str = readCompilerMessage(serverFd, manager.fd);
        manager.serverReadString = str;
        if (const auto &r2 = manager.receiveMessage(buffer, type); !r2)
        {
            exitFailure(r2.error());
        }

        switch (type)
        {

            // Compiler will only send CTBNonModule. CTBModule is not tested as the build-system needs to create a
            // shared memory-file using IPCManangerBS::createSharedMemoryBMIFile() mapping before sending a BMIFile.
            // Similarly, IPCManagerCompiler on receiving a BMIFile will call
            // IPCManagerCompiler::readSharedMemoryBMIFile and populate internal data structures. To accurately test, we
            // will have to create lots of on-disk files. The following tests the setup and communication mechanism
            // while the shared-memory file is tested separately. However, Message serialization and deserialization is
            // not fully tested.
        case CTB::NON_MODULE: {
            const auto &ctbNonModule = reinterpret_cast<CTBNonModule &>(buffer);
            printMessage(ctbNonModule, false);
            BTCNonModule nonModule = getBTCNonModule();
            if (const auto &r2 = manager.sendMessage(nonModule); !r2)
            {
                exitFailure(r2.error());
            }
            printMessage(nonModule, true);
        }

        break;

        case CTB::LAST_MESSAGE: {
            const auto &lastMessage = reinterpret_cast<CTBLastMessage &>(buffer);
            print("First ");
            printMessage(lastMessage, false);
            loopExit = true;
        }

        break;
        }

        if (loopExit)
        {
            break;
        }
    }

    string str = readCompilerMessage(serverFd, manager.fd);
    manager.serverReadString = str;
    if (const auto &r2 = manager.receiveMessage(buffer, type); !r2)
    {
        exitFailure(r2.error());
    }

    const auto &lastMessage = reinterpret_cast<CTBLastMessage &>(buffer);
    print("Second ");
    printMessage(lastMessage, false);

    // We have received a message for memory mapped BMI File. We will first create the server memory mapping. And then
    // close that mapping. And then create the client memory mapping, print out the file contents. And then close that
    // mapping. And then finally send the BTCLastMessage. This makes code coverage 100%.

    BMIFile bmi;
    bmi.filePath = (std::filesystem::current_path() / "bmi.txt").generic_string();
    bmi.fileSize = lastMessage.fileSize;
    // Creates server mapping to already created mapping
    if (const auto &r2 = IPCManagerBS::createSharedMemoryBMIFile(bmi); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        // closes the server mapping
        if (const auto &r3 = IPCManagerBS::closeBMIFileMapping(r2.value()); !r3)
        {
            exitFailure(r3.error());
        }
    }

    // creates compiler mapping to already created mapping and read contents.
    if (const auto &r2 = BuildSystemTest::readSharedMemoryBMIFile(bmi); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        if (const auto &processMappingOfBMIFile = r2.value();
            fileToString(bmi.filePath) != processMappingOfBMIFile.file)
        {
            exitFailure(fmt::format("File Contents not similar for {}", bmi.filePath));
        }
        if (const auto &r3 = IPCManagerCompiler::closeBMIFileMapping(r2.value()); !r3)
        {
            exitFailure(r3.error());
        }
    }

    IPCManagerBS oldManager = manager;
    // This tests the scenario where build-system has not yet called the receiveMessage after calling
    // the makeIPCManagerBS() while compiler process already exited and sent the CTBLastMessage.
    // To ensure that we make build-system manager before CompilerTest making compiler manager,
    // we make it here.

    if (auto r2 = makeIPCManagerBS((std::filesystem::current_path() / "test1").string(), serverFd); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        manager = r2.value();
    }

    ProcessMappingOfBMIFile bmi2Mapping;
    {
        // We don't assign the file.fileSize this-time. This IPCManagerBS::createSharedMemoryBMIFile will return it as
        // an out variable. This is used when build-system has to make a mapping for a prebuilt file. This case of
        // opening mapping first time is little different from creating mapping when another process (the compiler) has
        // already created one. Build-system preserves the out variable file.fileSize as it is passed to the later
        // compilations to save them from one system call.

        BMIFile bmi2;
        bmi2.filePath = (std::filesystem::current_path() / "bmi2.txt").generic_string();
        string bmi2Content = getRandomString();
        std::ofstream(bmi2.filePath) << bmi2Content;

        // creates server mapping to a new file.
        if (const auto &r2 = IPCManagerBS::createSharedMemoryBMIFile(bmi2); !r2)
        {
            exitFailure(r2.error());
        }
        else
        {
            // This will be closed later-on after receiving the lastMessage. We need to close since the test is being
            // repeated.
            bmi2Mapping = r2.value();
        }

        if (bmi2.fileSize != bmi2Content.size())
        {
            exitFailure(fmt::format("file.fileSize is different from fileContent.size\n"));
        }

        // After receiving the next message, CompilerTest will check that bmi2.txt is same as the received mapping.

        constexpr BTCLastMessage btcLastMessage;

        if (const auto &r2 = oldManager.sendMessage(btcLastMessage); !r2)
        {
            exitFailure(r2.error());
        }
        print("Reply to Second CTBLastMessage ");
        printMessage(btcLastMessage, true);
    }
    oldManager.closeConnection();

    // we delay the receiveMessage. Compiler in this duration has exited with error after connecting.
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // CompilerTest would have exited by now
    str = readCompilerMessage(serverFd, manager.fd);
    manager.serverReadString = str;
    if (const auto &r2 = manager.receiveMessage(buffer, type); !r2)
    {
        exitFailure(r2.error());
    }

    if (const auto &r2 = IPCManagerBS::closeBMIFileMapping(bmi2Mapping); !r2)
    {
        exitFailure(r2.error());
    }

    if (type != CTB::LAST_MESSAGE)
    {
        print("Test failed. Expected CTB::LAST_MESSAGE.\n");
    }

    print("Received CTBLastMessage on new manager.");
    printMessage(reinterpret_cast<CTBLastMessage &>(buffer), false);

    manager.closeConnection();
    return EXIT_SUCCESS;
}

int main()
{
    first = true;
    runTest();
    thr->join();
    first = false;
    runTest();
    thr->join();
}