
#include "IPCManagerCompiler.hpp"
#include "Testing.hpp"
#include "fmt/printf.h"
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <thread>

using fmt::print;
using namespace std;
using namespace P2978;

struct CompilerTest
{
    static decltype(IPCManagerCompiler::responses) &getResponse(IPCManagerCompiler &manager)
    {
        return manager.responses;
    }
    IPCManagerCompiler *compilerManager;
    explicit CompilerTest(IPCManagerCompiler *c) : compilerManager(c)
    {
    }
    [[nodiscard]] P2978::Result<void> receiveBTCModule(const CTBModule &moduleName)
    {
        return compilerManager->receiveBTCModule(moduleName);
    }
    [[nodiscard]] P2978::Result<void> receiveBTCNonModule(const CTBNonModule &nonModule)
    {
        return compilerManager->receiveBTCNonModule(nonModule);
    }
};

int main()
{
    IPCManagerCompiler manager;
    CompilerTest t(&manager);
    for (uint64_t i = 0; i < 300; ++i)
    {
        if (getRandomBool())
        {
            CTBModule ctbModule;
            string str = getRandomString();
            ctbModule.moduleName = str;

            if (const auto &r2 = t.receiveBTCModule(ctbModule); !r2)
            {
                exitFailure(r2.error());
            }
            else
            {
                printMessage(ctbModule, true);
            }
        }
        else
        {
            CTBNonModule nonModule;
            nonModule.isHeaderUnit = false;
            string str = getRandomString();
            nonModule.logicalName = str;

            if (const auto &r2 = t.receiveBTCNonModule(nonModule); !r2)
            {
                exitFailure(r2.error());
            }
            else
            {
                printMessage(nonModule, true);
            }
        }
    }

    map<string_view, Response> outputResponses;
    for (auto &r : CompilerTest::getResponse(manager))
    {
        outputResponses.emplace(r);
    }

    std::string output;
    for (auto &r : outputResponses)
    {
        output.append(fmt::format("Key {}\n", r.first));

        const Response &response = r.second;
        const string contents =
            response.type == FileType::HEADER_FILE ? fileToString(response.filePath) : string(response.bmiContents);
        appendResponse(output, response.filePath, contents, response.type, response.isSystem);
    }

    std::ofstream result("bmi.txt", std::ios::binary);
    result << output;
    result.close();
    if (!result)
    {
        exitFailure("Could not write bmi.txt");
    }
    print("Successfully Completed CompilerTest\n");
}

extern "C" const char *__asan_default_options()
{
    return "detect_container_overflow=0";
}
