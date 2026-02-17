
#include "IPCManagerCompiler.hpp"
#include "Testing.hpp"
#include "fmt/printf.h"
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <string>
#include <thread>

using fmt::print;
using namespace std;
using namespace N2978;

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
    [[nodiscard]] tl::expected<void, std::string> receiveBTCModule(const CTBModule &moduleName)
    {
        return compilerManager->receiveBTCModule(moduleName);
    }
    [[nodiscard]] tl::expected<void, std::string> receiveBTCNonModule(const CTBNonModule &nonModule)
    {
        return compilerManager->receiveBTCNonModule(nonModule);
    }

    static tl::expected<Mapping, std::string> readSharedMemoryBMIFile(const BMIFile &file)
    {
        return IPCManagerCompiler::readSharedMemoryBMIFile(file);
    }
};

int main()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    IPCManagerCompiler manager;
    CompilerTest t(&manager);
    for (uint64_t i = 0; i < 1; ++i)
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

    map<string_view, Response> outputResponses;
    for (auto &r : CompilerTest::getResponse(manager))
    {
        outputResponses.emplace(r);
    }

    set<string> files;
    std::string output;
    for (auto &r : outputResponses)
    {
        output.append(fmt::format("Key {}\n", r.first));

        if (const Response &response = r.second; files.emplace(response.filePath).second)
        {
            auto getFileType = [](const FileType type) {
                switch (type)
                {
                case FileType::HEADER_FILE: {
                    return "Header-File";
                }

                case FileType::MODULE: {
                    return "Module";
                }

                case FileType::HEADER_UNIT: {
                    return "Header-Unit";
                }
                }
            };

            output.append(fmt::format("Filepath {}\n", response.filePath));
            if (response.type == FileType::HEADER_FILE)
            {
                string fileContents = fileToString(response.filePath);
                output.append(fmt::format("FileContent {}\n", fileContents));
            }
            else
            {
                output.append(fmt::format("FileContent {}\n", response.mapping.file));
            }
            output.append(fmt::format("FileType {}\n", getFileType(response.type)));
            output.append(fmt::format("IsSystem {}\n", response.isSystem));
        }
    }

    const string bmi1Content = output;
    print("Sending first bmi-content.");
    if (const auto &r2 =
            manager.sendCTBLastMessage(bmi1Content, (std::filesystem::current_path() / "bmi.txt").generic_string());
        !r2)
    {
        exitFailure(r2.error());
    }

    print("BTCLastMessage for first bmi-content has been received.\n");

    BMIFile bmi2 = BMIFile();
    const string bmiTwoString = (std::filesystem::current_path() / "bmi2.txt").generic_string();
    bmi2.filePath = bmiTwoString;
    const string bmi2Content = fileToString(bmi2.filePath);
    bmi2.fileSize = bmi2Content.size();
    if (const auto &r2 = CompilerTest::readSharedMemoryBMIFile(bmi2); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        if (const auto &processMapping = r2.value(); bmi2Content != processMapping.file)
        {
            exitFailure(fmt::format("File Contents not similar for {}", bmi2.filePath));
        }
        if (const auto &r3 = IPCManagerCompiler::closeBMIFileMapping(r2.value()); !r3)
        {
            exitFailure(r3.error());
        }
    }
    for (std::string *p : allocations)
    {
        delete p;
    }
    print("Successfully Completed CompilerTest\n");
    print(delimiter);
}
