
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

void exitFailure(const string &r)
{

    fmt::println(r);
    print("Test Failed\n");
    exit(EXIT_FAILURE);
}

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
};

int main()
{
    const auto r = makeIPCManagerCompiler((filesystem::current_path() / "test").string());
    if (!r)
    {
        exitFailure(r.error());
    }

    IPCManagerCompiler manager = r.value();
    CompilerTest t(&manager);
    std::uniform_int_distribution distribution(0, 20);
    for (uint64_t i = 0; i < distribution(generator); ++i)
    {
        if (getRandomBool())
        {
            CTBModule ctbModule;
            ctbModule.moduleName = getRandomString();

            if (const auto &r2 = t.receiveBTCModule(ctbModule); !r2)
            {
                exitFailure(r2.error());
            }
            else
            {
                printMessage(ctbModule, true);
                printMessage(*r2, false);
            }
        }
        else
        {
            CTBNonModule nonModule;
            nonModule.isHeaderUnit = getRandomBool();
            nonModule.logicalName = "3";

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
    }

    manager.lastMessage.errorOccurred = false;
    if (const auto &r2 = manager.sendCTBLastMessage(); !r2)
    {
        exitFailure(r2.error());
    }

    print("First ");
    printMessage(manager.lastMessage, true);

    manager.lastMessage = CTBLastMessage{};
    manager.lastMessage.errorOccurred = false;
    string fileContent = getRandomString();
    manager.lastMessage.fileSize = fileContent.size();
    print("Second ");
    if (const auto &r2 =
            manager.sendCTBLastMessage(fileContent, (std::filesystem::current_path() / "bmi.txt").generic_string());
        !r2)
    {
        exitFailure(r.error());
    }

    printMessage(manager.lastMessage, true);
    print("File Content: {}\n\n", fileContent.data());

    print("BTCLastMessage has been received\n");

    manager.closeConnection();

    std::this_thread::sleep_for(std::chrono::seconds(3));
    const auto r2 = makeIPCManagerCompiler((filesystem::current_path() / "test1").string());
    if (!r2)
    {
        exitFailure(r2.error());
    }
    manager = r2.value();

    manager.lastMessage = CTBLastMessage{};
    manager.lastMessage.errorOccurred = true;
    if (const auto &r3 = manager.sendCTBLastMessage(); !r3)
    {
        exitFailure(r3.error());
    }

    print("BTCLastMessage received on new manager.\n");
    printMessage(manager.lastMessage, true);

    manager.closeConnection();
}
