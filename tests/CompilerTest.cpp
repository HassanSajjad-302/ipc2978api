
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

int main()
{
    const auto &r = makeIPCManagerCompiler((filesystem::current_path() / "test").string());
    if (!r)
    {
        exitFailure(r.error());
    }
    const IPCManagerCompiler &manager = r.value();
    std::uniform_int_distribution distribution(0, 20);
    for (uint64_t i = 0; i < distribution(generator); ++i)
    {
        if (getRandomBool())
        {
            CTBModule ctbModule;
            ctbModule.moduleName = getRandomString();

            if (const auto &r2 = manager.receiveBTCModule(ctbModule); !r2)
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
            nonModule.str = "3";

            if (const auto &r2 = manager.receiveBTCNonModule(nonModule); !r2)
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

    CTBLastMessage ctbLastMessage;
    ctbLastMessage.exitStatus = EXIT_SUCCESS;
    if (const auto &r2 = manager.sendCTBLastMessage(ctbLastMessage); !r2)
    {
        exitFailure(r2.error());
    }

    print("First ");
    printMessage(ctbLastMessage, true);

    // This tests file sharing. Contents of both outputs should be the same.
    CTBLastMessage lastMessageWithSharedFile;
    lastMessageWithSharedFile.exitStatus = EXIT_SUCCESS;
    string fileContent = getRandomString();
    lastMessageWithSharedFile.fileSize = fileContent.size();
    print("Second ");
    if (const auto &r2 = manager.sendCTBLastMessage(lastMessageWithSharedFile, fileContent,
                                                    (std::filesystem::current_path() / "bmi.txt").generic_string());
        !r2)
    {
        exitFailure(r.error());
    }

    printMessage(lastMessageWithSharedFile, true);
    print("File Content: {}\n\n", fileContent.data());

    print("BTCLastMessage has been received\n");
}
