#include "IPCManagerCompiler.hpp"
#include "TestBuildSystem.hpp"
#include "TestProcess.hpp"
#include "Testing.hpp"
#include "fmt/printf.h"
#include <algorithm>
#include <cstddef>
#include <string>

using fmt::print;
string compilerTestPrunedOutput;
using ipc2978_test::endsWith;

int runTest()
{
    ipc2978_test::TestProcess compilerTest{exitFailure};
    const std::string command = std::string("\"") + COMPILER_TEST + '"';
    compilerTest.startAsyncProcess(command.c_str());
    ipc2978_test::TestBuildSystem manager{compilerTest.writePipe};

    CTB type;
    alignas(std::max_align_t) char buffer[320];
    while (true)
    {
        if (!compilerTest.readCompilerMessage(compilerTestPrunedOutput))
        {
            break;
        }
        if (!endsWith(compilerTestPrunedOutput, delimiter))
        {
            exitFailure("early exit by CompilerTest");
        }
        compilerTest.pruneCompilerOutput(compilerTestPrunedOutput, buffer, type);

        switch (type)
        {

        case CTB::MODULE: {
            const auto &ctbModule = reinterpret_cast<CTBModule &>(buffer);
            printMessage(ctbModule, false);
            BTCModule btcModule = getBTCModule(ctbModule);
            if (const auto &r2 = manager.sendMessage(btcModule); !r2)
            {
                exitFailure(r2.error());
            }
            printMessage(btcModule, true);
        }

        break;

        case CTB::NON_MODULE: {
            const auto &ctbNonModule = reinterpret_cast<CTBNonModule &>(buffer);
            printMessage(ctbNonModule, false);
            BTCNonModule nonModule = getBTCNonModule(ctbNonModule);
            if (const auto &r2 = manager.sendMessage(nonModule); !r2)
            {
                exitFailure(r2.error());
            }
            printMessage(nonModule, true);
        }

        break;
        }
    }
    compilerTest.reapProcess();
    if (compilerTest.exitStatus != EXIT_SUCCESS)
    {
        exitFailure("CompilerTest did not exit successfully");
    }

    string output;
    for (auto &r : tempTestFiles)
    {
        output.append(fmt::format("Key {}\n", r.first));
        const TestResponse &response = r.second;
        appendResponse(output, response.filePath, response.fileContent, response.type, response.isSystem);
    }

    const string actual = fileToString("bmi.txt");
    if (actual != output)
    {
        const auto mismatch = std::mismatch(actual.begin(), actual.end(), output.begin(), output.end());
        exitFailure(fmt::format("Compiler response cache differs from the sent dependencies at byte {} "
                                "(received {} bytes, expected {} bytes)",
                                mismatch.first - actual.begin(), actual.size(), output.size()));
    }

    print("Exiting Successfully\n\n ");
    return EXIT_SUCCESS;
}

int main()
{
    runTest();
    fmt::println("\n\n\nCompilerTest Output\n\n\n {}", compilerTestPrunedOutput);
    compilerTestPrunedOutput.clear();
    tempTestFiles.clear();
    runTest();
    fmt::println("\n\n\nCompilerTest Output\n\n\n {}", compilerTestPrunedOutput);
}

extern "C" const char *__asan_default_options()
{
    return "detect_container_overflow=0";
}
