#include "IPCManagerBS.hpp"
#include "IPCManagerCompiler.hpp"
#include "Testing.hpp"
#include "fmt/printf.h"
#include <chrono>
#include <filesystem>
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

bool first = true;
std::thread *thr;

void exitFailure(const string &str)
{
    print(stderr, "{}\n", str);
    print("Test Failed\n");
    exit(EXIT_FAILURE);
}

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

int runTest()
{
    const auto r = makeIPCManagerBS((std::filesystem::current_path() / "test").string());
    if (!r)
    {
        exitFailure(r.error());
    }

    IPCManagerBS manager = r.value();
    listenForCompiler();
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
            BTCModule b = getBTCModule();
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

    BMIFile file;
    file.filePath = (std::filesystem::current_path() / "bmi.txt").generic_string();
    file.fileSize = lastMessage.fileSize;
    if (const auto &r2 = IPCManagerBS::createSharedMemoryBMIFile(file); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        if (const auto &r3 = IPCManagerBS::closeBMIFileMapping(r2.value()); !r3)
        {
            exitFailure(r3.error());
        }
    }

    if (const auto &r2 = BuildSystemTest::readSharedMemoryBMIFile(file); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        auto &processMappingOfBMIFile = r2.value();
        print("File Content: {}\n\n", processMappingOfBMIFile.file.data());
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

    if (auto r2 = makeIPCManagerBS((std::filesystem::current_path() / "test1").string()); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        manager = r2.value();
    }

    if (lastMessage.errorOccurred == EXIT_SUCCESS)
    {
        constexpr BTCLastMessage btcLastMessage;

        if (const auto &r2 = oldManager.sendMessage(btcLastMessage); !r2)
        {
            exitFailure(r2.error());
        }
        print("Reply to Second CTBLastMessage ");
        printMessage(btcLastMessage, true);
    }
    oldManager.closeConnection();

    std::this_thread::sleep_for(std::chrono::seconds(3));

    // CompilerTest would have exited by now
    if (const auto &r2 = manager.receiveMessage(buffer, type); !r2)
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
    runTest();
    thr->join();
    first = false;
    runTest();
    thr->join();
}