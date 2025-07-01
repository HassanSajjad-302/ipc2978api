//===- unittests/IPC2978/IPC2978Test.cpp - Tests IPC2978 Support -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// The following line is uncommented by clang/lib/IPC2978/setup.py for clang/unittests/IPC2978/IPC2978.cpp

// #define IS_THIS_CLANG_REPO

#ifdef IS_THIS_CLANG_REPO
#include "clang/IPC2978/IPCManagerBS.hpp"
#include "gtest/gtest.h"
template <typename T> void printMessage(const T &, bool)
{
}
#else
#include "IPCManagerBS.hpp"
#include "Testing.hpp"
#include "fmt/printf.h"
#endif

#include <Windows.h>
#include <filesystem>
#include <fstream>

using namespace std::filesystem;
using namespace N2978;
using namespace std;

// This code is used for launching process and using pipes to get stdout and stderr since the return of stdout and
// stderr is not implemented in CTBLastMessage yet.
// Copied From HMake codebase which copied from Ninja code-base with some changes.
/// Wraps a synchronous execution of a CL subprocess.
struct CLWrapper
{
    CLWrapper() : env_block_(nullptr)
    {
    }

    /// Set the environment block (as suitable for CreateProcess) to be used
    /// by Run().
    void SetEnvBlock(void *env_block)
    {
        env_block_ = env_block;
    }

    /// Start a process and gather its raw output.  Returns its exit code.
    /// Crashes (calls Fatal()) on error.
    [[nodiscard]] tl::expected<void, string> Run(const std::string &command) const;

    void *env_block_;
};

HANDLE stdout_read, stdout_write;
PROCESS_INFORMATION process_info = {};
tl::expected<void, string> CLWrapper::Run(const string &command) const
{
    SECURITY_ATTRIBUTES security_attributes = {};
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;

    if (!CreatePipe(&stdout_read, &stdout_write, &security_attributes, 0))
        return tl::unexpected("CreatePipe" + getErrorString());

    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0))
        return tl::unexpected("SetHandleInformation" + getErrorString());

    STARTUPINFOA startup_info = {};
    startup_info.cb = sizeof(STARTUPINFOA);
    startup_info.hStdError = stdout_write;
    startup_info.hStdOutput = stdout_write;
    startup_info.dwFlags |= STARTF_USESTDHANDLES;

    if (!CreateProcessA(nullptr, const_cast<char *>(command.c_str()), nullptr, nullptr,
                        /* inherit handles */ TRUE, 0, env_block_, nullptr, &startup_info, &process_info))
    {
        return tl::unexpected("CreateProcess" + getErrorString());
    }

    if (!CloseHandle(stdout_write))
    {
        return tl::unexpected("CloseHandle" + getErrorString());
    }
}

bool isProcessAlive(HANDLE hProcess)
{
    DWORD exitCode;
    if (GetExitCodeProcess(hProcess, &exitCode))
    {
        return exitCode == STILL_ACTIVE;
    }
    return false; // Error getting exit code
}

tl::expected<int, string> printOutputAndClosePipes()
{

    string output;
    // Read all output of the subprocess.
    DWORD read_len = 1;
    while (read_len)
    {
        char buf[64 << 10];
        read_len = 0;
        if (!::ReadFile(stdout_read, buf, sizeof(buf), &read_len, nullptr) && GetLastError() != ERROR_BROKEN_PIPE)
        {
            return tl::unexpected("ReadFile" + getErrorString());
        }
        output.append(buf, read_len);
    }

    // Wait for it to exit and grab its exit code.
    if (WaitForSingleObject(process_info.hProcess, INFINITE) == WAIT_FAILED)
        return tl::unexpected("WaitForSingleObject" + getErrorString());
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(process_info.hProcess, &exit_code))
        return tl::unexpected("GetExitCodeProcess" + getErrorString());

    if (!CloseHandle(stdout_read) || !CloseHandle(process_info.hProcess) || !CloseHandle(process_info.hThread))
    {
        return tl::unexpected("CloseHandle" + getErrorString());
    }

#ifndef IS_THIS_CLANG_REPO
    fmt::print("{}", output);
#endif
    return exit_code;
}

[[nodiscard]] tl::expected<void, string> setupTest()
{

    const string mod = R"(export module mod;
import mod2;

export void func()
{
    EmptyClass2 a;
    func2();
    // does nothing
}

export struct EmptyClass
{
};)";

    const string mod1 = R"(module;

export module mod1;
import mod2;)";

    const string mod2 = R"(export module mod2;

export void func2()
{
    // does nothing
}

export struct EmptyClass2
{
};)";

    const string main = R"(import mod;
import mod1;

int main()
{
    func();
    EmptyClass a;
})";

    ofstream("mod.cppm") << mod;
    ofstream("mod1.cppm") << mod1;
    ofstream("mod2.cppm") << mod2;
    ofstream("main.cpp") << main;

    // compile main.cpp which imports B.cpp which imports A.cpp.

    if (system(
            R"(.\clang.exe -std=c++20 mod2.cppm -c -fmodule-output="mod2 .pcm"  -fmodules-reduced-bmi -o  "mod2 .o")") !=
        EXIT_SUCCESS)
    {
        return tl::unexpected("could not run the first command\n");
    }

    if (system(
            R"(.\clang.exe -std=c++20 mod.cppm -c -fmodule-output="mod .pcm"  -fmodules-reduced-bmi -o  "mod .o"  -fmodule-file=mod2="./mod2 .pcm")") !=
        EXIT_SUCCESS)
    {
        return tl::unexpected("could not run the second command\n");
    }

    if (system(
            R"(.\clang.exe -std=c++20 mod1.cppm -c -fmodule-output="mod1 .pcm"  -fmodules-reduced-bmi -o  "mod1 .o"  -fmodule-file=mod2="./mod2 .pcm")") !=
        EXIT_SUCCESS)
    {
        tl::unexpected("could not run the second command\n");
    }
    return {};
}

tl::expected<int, string> runTest()
{

    if (const auto &r = setupTest(); !r)
    {
        return tl::unexpected(r.error());
    }

    string current = current_path().generic_string() + '/';
    string mainFilePath = current + "main .o";
    string modFilePath = current + "mod .pcm";
    string mod1FilePath = current + "mod1 .pcm";
    string mod2FilePath = current + "mod2 .pcm";

    if (const auto &r = makeIPCManagerBS(std::move(mainFilePath)); r)
    {
        uint8_t messageCount = 0;
        const IPCManagerBS &manager = *r;

        string compileCommand =
            R"(.\clang.exe -std=c++20 -c main.cpp -noScanIPC -o "C:/Projects/llvm-project/llvm/cmake-build-debug/bin/main .o")";
        CLWrapper wrapper;
        if (const auto &r2 = wrapper.Run(compileCommand); !r2)
        {
            return tl::unexpected(r2.error());
        }

        while (true)
        {
            CTB type;
            char buffer[320];
            if (const auto &r2 = manager.receiveMessage(buffer, type); !r2)
            {
                string str = r2.error();
                return tl::unexpected("manager receive message failed" + r2.error() + "\n");
            }

            switch (type)
            {

            case CTB::MODULE: {
                const auto &ctbModule = reinterpret_cast<CTBModule &>(buffer);
                printMessage(ctbModule, false);

                if (!messageCount)
                {
                    ++messageCount;

                    BMIFile mod;
                    mod.filePath = modFilePath;
                    mod.fileSize = 0;

                    BMIFile mod2;
                    mod2.filePath = mod2FilePath;
                    mod2.fileSize = 0;
                    ModuleDep mod2Dep;
                    mod2Dep.file = std::move(mod2);
                    mod2Dep.logicalName = "mod2";

                    BTCModule b;
                    b.requested = std::move(mod);
                    b.deps.emplace_back(std::move(mod2Dep));

                    if (const auto &r2 = manager.sendMessage(b); !r2)
                    {
                        return tl::unexpected(r2.error());
                    }
                    printMessage(b, true);
                }
                else if (messageCount == 1)
                {
                    ++messageCount;
                    BMIFile mod1;
                    mod1.filePath = mod1FilePath;
                    mod1.fileSize = 0;

                    BTCModule b;
                    b.requested = std::move(mod1);

                    if (const auto &r2 = manager.sendMessage(b); !r2)
                    {
                        return tl::unexpected(r2.error());
                    }
                    printMessage(b, true);
                }
            }

            break;

            case CTB::NON_MODULE: {
            }

            case CTB::LAST_MESSAGE: {
            }

                return tl::unexpected("Unexpected message received");
            }

            if (messageCount == 2)
            {
                break;
            }
        }
    }
    else
    {
        return tl::unexpected("creating manager failed" + r.error() + "\n");
    }

    return printOutputAndClosePipes();
}

#ifdef IS_THIS_CLANG_REPO
TEST(IPC2978Test, IPC2978Test)
{
    if (const auto &r = runTest(); !r)
    {
        FAIL() << r.error();
    }
}
#else
int main()
{
    if (const auto &r = runTest(); !r)
    {
        fmt::print("{}\n", r.error());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
