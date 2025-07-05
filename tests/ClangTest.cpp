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

#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace std::filesystem;
using namespace N2978;
using namespace std;

#ifdef _WIN32
#define CLANG_CMD ".\\clang.exe"
#else
#define CLANG_CMD "./clang"
#endif

#ifdef _WIN32
HANDLE stdout_read, stdout_write;
PROCESS_INFORMATION process_info = {};
tl::expected<void, string> Run(const string &command)
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

#else

int stdout_pipe[2], stderr_pipe[2];
int procStatus;

/// Start a process and gather its raw output.  Returns its exit code.
/// Crashes (calls Fatal()) on error.
tl::expected<void, string> Run(const string &command)
{
    // Create pipes for stdout and stderr
    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1)
    {
        return tl::unexpected("pipe" + getErrorString());
    }

    if (const pid_t pid = fork(); pid == -1)
    {
        return tl::unexpected("fork" + getErrorString());
    }
    else
    {
        if (pid == 0)
        {
            // Child process

            // Redirect stdout and stderr to the pipes
            dup2(stdout_pipe[1], STDOUT_FILENO); // Redirect stdout to stdout_pipe
            dup2(stderr_pipe[1], STDERR_FILENO); // Redirect stderr to stderr_pipe

            // Close unused pipe ends
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            close(stdout_pipe[1]);
            close(stderr_pipe[1]);

            // Execute a command (e.g., "ls" or any other)
            exit(WEXITSTATUS(system(command.c_str())));
        }

        // Parent process
        // Close unused pipe ends
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        if (waitpid(pid, &procStatus, 0) == -1)
        {
            return tl::unexpected("waitpid" + getErrorString());
        }
    }
    return {};
}

tl::expected<int, string> printOutputAndClosePipes()
{
    string output;
    char buffer[4096];
    while (true)
    {
      if (const uint64_t readSize =
              read(stdout_pipe[0], buffer, sizeof(buffer) - 1))
        {
            output.append(buffer, readSize);
        }
        else
        {
            break;
        }
    }

    while (true)
    {
      if (const uint64_t readSize =
              read(stderr_pipe[0], buffer, sizeof(buffer) - 1))
        {
            output.append(buffer, readSize);
        }
        else
        {
            break;
        }
    }

    // Close the read ends of the pipes
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

#ifndef IS_THIS_CLANG_REPO
    fmt::print("{}", output);
#endif
    return WEXITSTATUS(procStatus);
}
#endif

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
            CLANG_CMD R"( -std=c++20 mod2.cppm -c -fmodule-output="mod2 .pcm"  -fmodules-reduced-bmi -o  "mod2 .o")") !=
        EXIT_SUCCESS)
    {
        return tl::unexpected("could not run the first command\n");
    }

    if (system(
            CLANG_CMD R"( -std=c++20 mod.cppm -c -fmodule-output="mod .pcm"  -fmodules-reduced-bmi -o  "mod .o"  -fmodule-file=mod2="./mod2 .pcm")") !=
        EXIT_SUCCESS)
    {
        return tl::unexpected("could not run the second command\n");
    }

    if (system(
            CLANG_CMD R"( -std=c++20 mod1.cppm -c -fmodule-output="mod1 .pcm"  -fmodules-reduced-bmi -o  "mod1 .o"  -fmodule-file=mod2="./mod2 .pcm")") !=
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
            CLANG_CMD R"( -std=c++20 -c main.cpp -noScanIPC -o "C:/Projects/llvm-project/llvm/cmake-build-debug/bin/main .o")";
        if (const auto &r2 = Run(compileCommand); !r2)
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
    const path p = current_path();
    current_path(LLVM_TOOLS_BINARY_DIR);
    const path mainFilePath = (LLVM_TOOLS_BINARY_DIR / path("main .o")).lexically_normal();
    remove(mainFilePath);

    const auto &r = runTest();
    current_path(p);
    if (!r)
    {
        FAIL() << r.error();
    }
    if (!exists(mainFilePath))
    {
        FAIL() << "main.o not found\n";
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
