
#include "IPCManagerCompiler.hpp"
#include "Testing.hpp"
#include <chrono>
#include <filesystem>
#include <random>
#include <string>
#include <thread>
#include <print>

using namespace std;
using namespace N2978;

int main()
{
    IPCManagerCompiler manager("test");
    std::uniform_int_distribution distribution(0, 20);
    for (uint64_t i = 0; i < distribution(generator); ++i)
    {
        if (getRandomBool())
        {
            CTBModule ctbModule;
            ctbModule.moduleName = getRandomString();

            const BTCModule &btcModule = manager.receiveBTCModule(ctbModule).value();
            printMessage(ctbModule, true);
            printMessage(btcModule, false);
        }
        else
        {
            CTBNonModule nonModule;
            nonModule.isHeaderUnit = getRandomBool();
            nonModule.str = "3";

            BTCNonModule btcNonModule = manager.receiveBTCNonModule(nonModule).value();
            printMessage(nonModule, true);
            printMessage(btcNonModule, false);
        }
    }

    CTBLastMessage ctbLastMessage;
    ctbLastMessage.exitStatus = EXIT_SUCCESS;
    manager.sendCTBLastMessage(ctbLastMessage);

    printMessage(ctbLastMessage, true);
    print("Received Last Message\n");

    // This tests file sharing. Contents of both outputs should be the same.
    CTBLastMessage ctbLastMessage2;
    ctbLastMessage2.exitStatus = EXIT_SUCCESS;
    string fileContent = getRandomString();
    ctbLastMessage2.fileSize = fileContent.size();
    print("File Content:\n\n", fileContent);
    manager.sendCTBLastMessage(ctbLastMessage2, fileContent,
                               (std::filesystem::current_path() / "bmi.txt").generic_string());

    printMessage(ctbLastMessage2, true);
    print("{}", fileContent);
    print("Received Last Message\n");
}
