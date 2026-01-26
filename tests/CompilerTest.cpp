
#include "IPCManagerCompiler.hpp"
#include "Testing.hpp"
#include "fmt/printf.h"
#include <chrono>
#include <filesystem>
#include <random>
#include <string>
#include <thread>

using fmt::print;
using namespace std;
using namespace N2978;

struct CompilerTest
{
    IPCManagerCompiler *compilerManager;
    explicit CompilerTest(IPCManagerCompiler *c) : compilerManager(c)
    {
    }
    [[nodiscard]] tl::expected<BTCModule, std::string> receiveBTCModule(const CTBModule &moduleName)
    {
        return compilerManager->receiveBTCModule(moduleName);
    }
    [[nodiscard]] tl::expected<BTCNonModule, std::string> receiveBTCNonModule(const CTBNonModule &nonModule)
    {
        return compilerManager->receiveBTCNonModule(nonModule);
    }

    static tl::expected<ProcessMappingOfBMIFile, std::string> readSharedMemoryBMIFile(const BMIFile &file)
    {
        return IPCManagerCompiler::readSharedMemoryBMIFile(file);
    }
};

int main()
{

    IPCManagerCompiler manager;
    CompilerTest t(&manager);
    std::uniform_int_distribution distribution(0, 20);
    for (uint64_t i = 0; i < distribution(generator); ++i)
    {
        CTBNonModule nonModule;
        nonModule.isHeaderUnit = false;
        nonModule.logicalName = getRandomString();

        if (const auto &r2 = t.receiveBTCNonModule(nonModule); !r2)
        {
            exitFailure(r2.error());
        }
        else
        {
            printMessage(nonModule, true);
            printMessage(*r2, false);
        }
    }

    manager.lastMessage.errorOccurred = false;
    string bmi1Content = getRandomString();
    manager.lastMessage.fileSize = bmi1Content.size();
    print("Second ");
    if (const auto &r2 =
            manager.sendCTBLastMessage(bmi1Content, (std::filesystem::current_path() / "bmi.txt").generic_string());
        !r2)
    {
        exitFailure(r2.error());
    }

    printMessage(manager.lastMessage, true);

    print("BTCLastMessage has been received\n");

    BMIFile bmi2 = BMIFile();
    bmi2.filePath = (std::filesystem::current_path() / "bmi2.txt").generic_string();
    string bmi2Content = fileToString(bmi2.filePath);
    bmi2.fileSize = bmi2Content.size();
    if (const auto &r2 = CompilerTest::readSharedMemoryBMIFile(bmi2); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        if (const auto &processMappingOfBMIFile = r2.value(); bmi2Content != processMappingOfBMIFile.file)
        {
            exitFailure(fmt::format("File Contents not similar for {}", bmi2.filePath));
        }
        if (const auto &r3 = IPCManagerCompiler::closeBMIFileMapping(r2.value()); !r3)
        {
            exitFailure(r3.error());
        }
    }

    // Build-system will create the new manager before sending the last-message so we can safely connect. However, it
    // wil wait for us to send the lastMessage with error before it wil receiving imitating a situation where compiler
    // has already exited even before the build-system call receiveMessage.
    /*
    const auto r2 = makeIPCManagerCompiler((filesystem::current_path() / "test1").string());
    if (!r2)
    {
        exitFailure(r2.error());
    }
    manager = r2.value();
    */

    manager.lastMessage = CTBLastMessage{};
    manager.lastMessage.errorOccurred = true;
    if (const auto &r3 = manager.sendCTBLastMessage(); !r3)
    {
        exitFailure(r3.error());
    }

    print("BTCLastMessage received on new manager.\n");
    printMessage(manager.lastMessage, true);
}
