
#include "IPCManagerCompiler.hpp"
#include "Testing.hpp"
#include <chrono>
#include <filesystem>
#include "fmt/printf.h"
#include <random>
#include <string>
#include <thread>

using fmt::print;
using namespace std;
using namespace N2978;
int main()
{
    const IPCManagerCompiler &manager = makeIPCManagerCompiler((filesystem::current_path() / "test").string()).value();
    std::uniform_int_distribution distribution(0, 20);
    for (uint64_t i = 0; i < distribution(generator); ++i)
    {
        if (getRandomBool())
        {
            CTBModule ctbModule;
            ctbModule.moduleName = getRandomString();

            if (const auto &r = manager.receiveBTCModule(ctbModule); !r)
            {
                print("{}\n", r.error());
                exit(EXIT_FAILURE);
            }
            else
            {
                printMessage(ctbModule, true);
                printMessage(*r, false);
            }
        }
        else
        {
            CTBNonModule nonModule;
            nonModule.isHeaderUnit = getRandomBool();
            nonModule.str = "3";

            if (const auto &r = manager.receiveBTCNonModule(nonModule); !r)
            {
                print("{}\n", r.error());
                exit(EXIT_FAILURE);
            }
            else
            {
                printMessage(nonModule, true);
                printMessage(*r, false);
            }
        }
    }

    CTBLastMessage ctbLastMessage;
    ctbLastMessage.exitStatus = EXIT_SUCCESS;
    if (const auto &r = manager.sendCTBLastMessage(ctbLastMessage); !r)
    {
        print("{}\n", r.error());
        exit(EXIT_FAILURE);
    }

    printMessage(ctbLastMessage, true);
    print("Received Last Message\n");

    // This tests file sharing. Contents of both outputs should be the same.
    CTBLastMessage ctbLastMessage2;
    ctbLastMessage2.exitStatus = EXIT_SUCCESS;
    string fileContent = getRandomString();
    ctbLastMessage2.fileSize = fileContent.size();
    print("File Content:\n\n", fileContent);
    if (const auto &r = manager.sendCTBLastMessage(ctbLastMessage2, fileContent,
                                                   (std::filesystem::current_path() / "bmi.txt").generic_string());
        !r)
    {
        print("{}\n", r.error());
        exit(EXIT_FAILURE);
    }

    printMessage(ctbLastMessage2, true);
    print("{}", fileContent);
    print("Received Last Message\n");
}
