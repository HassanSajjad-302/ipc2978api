//===- unittests/IPC2978/IPC2978Test.cpp - Tests IPC2978 Support -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// The following line is uncommented by clang/lib/IPC2978/setup.py for clang/unittests/IPC2978/IPC2978.cpp

// #define IS_THIS_CLANG_REPO

#include <cstddef>
#include <cstring>
#include <iostream>
#ifdef IS_THIS_CLANG_REPO
#include "gtest/gtest.h"
#else
#include "Testing.hpp"
#endif

#include "TestBuildSystem.hpp"
#include "TestProcess.hpp"
#include <filesystem>
#include <fstream>

using namespace std::filesystem;
using namespace P2978;
using namespace std;

#ifdef _WIN32
constexpr const char *clangExecutableName = "clang.exe";
#else
constexpr const char *clangExecutableName = "clang";
#endif

#ifndef IPC2978_CLANG_EXECUTABLE
#define IPC2978_CLANG_EXECUTABLE ""
#endif

namespace
{

tl::expected<string, string> compilerCommand(const path &compiler)
{
    std::error_code error;
    const path executable = absolute(compiler, error);
    if (error)
        return tl::unexpected("Cannot resolve Clang executable: " + error.message());
    if (!is_regular_file(executable, error))
        return tl::unexpected("Clang executable not found: " + executable.string() +
                              "\nPass the path to a Clang rebuilt with this IPC2978 library, or configure "
                              "IPC2978_CLANG_EXECUTABLE.");
#ifndef _WIN32
    if (access(executable.c_str(), X_OK) != 0)
        return tl::unexpected("Clang file is not executable: " + executable.string());
#endif
    string command = "\"";
    for (const char c : executable.string())
    {
#ifndef _WIN32
        // TestProcess uses wordexp on Unix, so preserve shell metacharacters literally.
        if (c == '\\' || c == '"' || c == '$' || c == '`')
            command += '\\';
#endif
        command += c;
    }
    command += '"';
    return command;
}

struct CompilerSession
{
    ipc2978_test::TestProcess process;
    std::string output;
    CTB type{};
    alignas(std::max_align_t) char buffer[320];

    tl::expected<void, std::string> readRequest()
    {
        if (!process.readCompilerMessage(output))
            return tl::unexpected(process.error.empty() ? "Compiler exited before the expected request:\n" + output
                                                        : process.error);
        if (!process.pruneCompilerOutput(output, buffer, type))
            return tl::unexpected(process.error);
        return {};
    }

    tl::expected<ipc2978_test::TestBuildSystem, std::string> start(const std::string &command, bool requestExpected)
    {
        output.clear();
        if (!process.startAsyncProcess(command.c_str()))
            return tl::unexpected(process.error);
        if (requestExpected)
        {
            if (const auto result = readRequest(); !result)
                return tl::unexpected(result.error());
        }
        return ipc2978_test::TestBuildSystem{process.writePipe};
    }

    tl::expected<void, std::string> finish()
    {
        if (process.readCompilerMessage(output))
            return tl::unexpected(std::string("Unexpected compiler request at completion"));
        if (!process.error.empty())
            return tl::unexpected(process.error);
        if (!process.reapProcess())
            return tl::unexpected(process.error);
        if (process.exitStatus != EXIT_SUCCESS)
            return tl::unexpected("Compiler exited with status " + std::to_string(process.exitStatus) + ":\n" + output);
        std::cout << output;
        return {};
    }
};

// main.cpp
const string mainDotCpp = R"(
// only one request of Foo will be made as A and big.hpp
// will be provided with it.
import Foo;
import A;
#include "y.hpp"
#include "z.hpp"

int main()
{
    Hello();
    World();
    Foo();
}
)";

// Creates all the input files (source files + pcm files) that are needed for the test.
void setupTest()
{
    //  a.cpp
    const string aDotCpp = R"(
export module A;     // primary module interface unit

export import :B;    // Hello() is visible when importing 'A'.
import :C;           // WorldImpl() is now visible only for 'a.cpp'.
// export import :C; // ERROR: Cannot export a module implementation unit.

// World() is visible by any translation unit importing 'A'.
export char const* World()
{
    return WorldImpl();
}
)";
    // a-b.cpp
    const string aBDotCPP = R"(
export module A:B; // partition module interface unit

// Hello() is visible by any translation unit importing 'A'.
export char const* Hello() { return "Hello"; }
)";

    // a-c.cpp
    const string aCDotCPP = R"(
module A:C; // partition module implementation unit

// WorldImpl() is visible by any module unit of 'A' importing ':C'.
char const* WorldImpl() { return "World"; }
)";

    // m.hpp, n.hpp and o.hpp are to be used as header-units, header-files
    // while x.hpp, y.hpp and z.hpp are to be used as big-hu by include big.hpp

    // m.hpp
    const string mDotHpp = R"(
// this file can not be included without first defining M_HEADER_FILE
// this is to demonstrate difference between header-file and header-unit.
// as macros don't seep into header-units

#ifdef M_HEADER_FILE
inline int m = 5;
#else
fail compilation
#endif
)";

    // n.hpp
    const string nDotHpp = R"(
// should work just fine as macro should not seep in here while inclusion.

#ifdef N_HEADER_FILE
fail compilation
#else
#define M_HEADER_FILE
#include "m.hpp"
inline int n = 5 + m;
#endif

// COMMAND_MACRO should be defined while compiling this.
// however, it should still be fine if it is not defined while compiling
// a file consuming this

#ifndef COMMAND_MACRO
fail compilation
#endif
)";

    // o.hpp
    const string oDotHpp = R"(
// TRANSLATING should be defined if /translateInclude is being used.
// "o.hpp" should still be treated as header-file.

#define M_HEADER_FILE
#include "m.hpp"
#ifdef TRANSLATING
#include "n.hpp"
#else
import "n.hpp";
#endif

inline int o = n + m + 5;
)";

    // x.hpp
    const string xDotHpp = R"(
#ifndef X_HPP
#define X_HPP
inline int x = 5;
#endif
)";

    // y.hpp
    const string yDotHpp = R"(
#ifndef Y_HPP
#define Y_HPP
#include "x.hpp"
inline int y = x + 5;
#endif
)";

    // z.hpp
    const string zDotHpp = R"(
#ifndef Z_HPP
#define Z_HPP
#include "y.hpp"
inline int z = x + y + 5;
#endif
)";

    // big.hpp
    const string bigDotHpp = R"(
#include "x.hpp"
// todo
// following two should not be requested as big.hpp includes the following as well.
#include "y.hpp"
#include "z.hpp"
)";

    // foo.cpp
    const string fooDotCpp = R"(
module;
#include "x.hpp"
#include "z.hpp"
#include "y.hpp"
#include "big.hpp"
export module Foo;
import A;

export void Foo()
{
    Hello();
    World();
    int s = x + y + z;
}
)";

    ofstream("a.cpp") << aDotCpp;
    ofstream("a-b.cpp") << aBDotCPP;
    ofstream("a-c.cpp") << aCDotCPP;
    ofstream("m.hpp") << mDotHpp;
    ofstream("n.hpp") << nDotHpp;
    ofstream("o.hpp") << oDotHpp;
    ofstream("x.hpp") << xDotHpp;
    ofstream("y.hpp") << yDotHpp;
    ofstream("z.hpp") << zDotHpp;
    ofstream("big.hpp") << bigDotHpp;
    ofstream("foo.cpp") << fooDotCpp;
    ofstream("main.cpp") << mainDotCpp;
}

tl::unexpected<string> errorReturn()
{
    return tl::unexpected<string>("IPC2978 Test Error: Wrong Message Received\n");
}

#define CHECK(condition)                                                                                               \
    if (!(condition))                                                                                                  \
    {                                                                                                                  \
        return errorReturn();                                                                                          \
    }

#define SEND_MESSAGE(message)                                                                                          \
    if (const auto &_r_send_##message = manager.sendMessage(message); !_r_send_##message)                              \
    {                                                                                                                  \
        return tl::unexpected("manager send message failed" + _r_send_##message.error() + "\n");                       \
    }

#define CHECK_RESULT(expression)                                                                                       \
    if (const auto &result = (expression); !result)                                                                    \
        return tl::unexpected(result.error());

tl::expected<void, string> runTest(const path &compiler)
{
    const auto command = compilerCommand(compiler);
    CHECK_RESULT(command)
    const string &clangCommand = *command;
    CompilerSession session;
    auto &type = session.type;
    auto &buffer = session.buffer;
    remove(path("main .o"));
    setupTest();

    string str = current_path().string();
#ifdef _WIN32
    for (char &c : str)
    {
        c = tolower(c);
    }
#endif

    path curPath(str);

    string mainFilePath = (curPath / "main .o").string();
    string modFilePath = (curPath / "mod .pcm").string();
    string mod1FilePath = (curPath / "mod1 .pcm").string();
    string mod2FilePath = (curPath / "mod2 .pcm").string();
    string aCObj = (curPath / "a-c .o").string();
    string aCPcm = (curPath / "a-c .pcm").string();
    string aBObj = (curPath / "a-b .o").string();
    string aBPcm = (curPath / "a-b .pcm").string();
    string aObj = (curPath / "a .o").string();
    string aPcm = (curPath / "a .pcm").string();
    string bObj = (curPath / "b .o").string();
    string bPcm = (curPath / "b .pcm").string();
    string mHpp = (curPath / "m.hpp").string();
    string nHpp = (curPath / "n.hpp").string();
    string oHpp = (curPath / "o.hpp").string();
    string nPcm = (curPath / "n .pcm").string();
    string oPcm = (curPath / "o .pcm").string();
    string xHpp = (curPath / "x.hpp").string();
    string yHpp = (curPath / "y.hpp").string();
    string zHpp = (curPath / "z.hpp").string();
    string bigHpp = (curPath / "big.hpp").string();
    string bigPcm = (curPath / "big .pcm").string();
    string fooPcm = (curPath / "foo .pcm").string();
    string fooObj = (curPath / "foo .o").string();
    string mainObj = (curPath / "main .o").string();

    // compiling a-c.cpp
    {
        string compileCommand = clangCommand + R"( -std=c++20 -fmodules-reduced-bmi -o ")" + aCObj +
                                "\" -useIPC -c -xc++-module a-c.cpp -fmodule-output=\"" + aCPcm + "\"";

        CHECK_RESULT(session.start(compileCommand, false))
        CHECK_RESULT(session.finish())
    }

    // compiling a-b.cpp
    {
        string compileCommand = clangCommand + R"( -std=c++20 -fmodules-reduced-bmi -o ")" + aBObj +
                                "\" -useIPC -c -xc++-module a-b.cpp -fmodule-output=\"" + aBPcm + "\"";

        CHECK_RESULT(session.start(compileCommand, false))
        CHECK_RESULT(session.finish())
    }
    // compiling a.cpp
    {
        string compileCommand = clangCommand + R"( -std=c++20 -fmodules-reduced-bmi -o ")" + aObj +
                                "\" -useIPC -c -xc++-module a.cpp -fmodule-output=\"" + aPcm + "\"";

        auto managerResult = session.start(compileCommand, true);
        CHECK_RESULT(managerResult)
        auto &manager = *managerResult;

        CHECK(type == CTB::MODULE)
        const auto &ctbModule = reinterpret_cast<CTBModule &>(buffer);
        CHECK(ctbModule.moduleName == "A:B")

        BTCModule btcMod;
        btcMod.filePath = aBPcm;

        ModuleDep modDep;
        modDep.filePath = aCPcm;
        modDep.logicalNames.emplace_back("A:C");
        modDep.isHeaderUnit = false;
        btcMod.modDeps.emplace_back(std::move(modDep));

        SEND_MESSAGE(btcMod)

        CHECK_RESULT(session.finish())
    }

    // compiling n.hpp
    {
        string compileCommand = clangCommand + R"( -std=c++20 -fmodule-header=user -o ")" + nPcm +
                                "\" -useIPC -xc++-header n.hpp -DCOMMAND_MACRO";

        auto managerResult = session.start(compileCommand, true);
        CHECK_RESULT(managerResult)
        auto &manager = *managerResult;

        CHECK(type == CTB::NON_MODULE)
        const auto &ctbNonModMHpp = reinterpret_cast<CTBNonModule &>(buffer);
        CHECK(ctbNonModMHpp.logicalName == "m.hpp" && ctbNonModMHpp.isHeaderUnit == false)

        BTCNonModule nonModMPcm;
        nonModMPcm.isHeaderUnit = false;
        nonModMPcm.filePath = mHpp;
        SEND_MESSAGE(nonModMPcm)
        CHECK_RESULT(session.finish())
    }

    // compiling o.hpp
    {
        string compileCommand =
            clangCommand + R"( -std=c++20 -fmodule-header=user -o ")" + oPcm + "\" -useIPC -xc++-header o.hpp";

        auto managerResult = session.start(compileCommand, true);
        CHECK_RESULT(managerResult)
        auto &manager = *managerResult;

        CHECK(type == CTB::NON_MODULE)
        const auto &ctbNonModMHpp = reinterpret_cast<CTBNonModule &>(buffer);
        CHECK(ctbNonModMHpp.logicalName == "m.hpp" && ctbNonModMHpp.isHeaderUnit == false)

        BTCNonModule nonModMPcm;
        nonModMPcm.isHeaderUnit = false;
        nonModMPcm.filePath = mHpp;
        SEND_MESSAGE(nonModMPcm)

        CHECK_RESULT(session.readRequest())
        CHECK(type == CTB::NON_MODULE)
        const auto &ctbNonModNHpp = reinterpret_cast<CTBNonModule &>(buffer);
        CHECK(ctbNonModNHpp.logicalName == "n.hpp" && ctbNonModNHpp.isHeaderUnit == true)

        BTCNonModule nonModNPcm;
        nonModNPcm.isHeaderUnit = true;

        nonModNPcm.filePath = nPcm;

        SEND_MESSAGE(nonModNPcm)
        CHECK_RESULT(session.finish())
    }

    // compiling o.hpp with include-translation. BTCNonModule for n.hpp will be received with
    // isHeaderUnit = true.
    {
        string compileCommand =
            clangCommand + R"( -std=c++20 -fmodule-header=user -o ")" + oPcm + "\" -useIPC -xc++-header o.hpp -DTRANSLATING";

        auto managerResult = session.start(compileCommand, true);
        CHECK_RESULT(managerResult)
        auto &manager = *managerResult;

        CHECK(type == CTB::NON_MODULE)
        const auto &ctbNonModMHpp = reinterpret_cast<CTBNonModule &>(buffer);
        CHECK(ctbNonModMHpp.logicalName == "m.hpp" && ctbNonModMHpp.isHeaderUnit == false)

        BTCNonModule nonModMPcm;
        nonModMPcm.isHeaderUnit = false;
        nonModMPcm.filePath = mHpp;
        SEND_MESSAGE(nonModMPcm)

        CHECK_RESULT(session.readRequest())
        CHECK(type == CTB::NON_MODULE)
        const auto &ctbNonModNHpp = reinterpret_cast<CTBNonModule &>(buffer);
        CHECK(ctbNonModNHpp.logicalName == "n.hpp" && ctbNonModNHpp.isHeaderUnit == false)

        BTCNonModule nonModNPcm;

        nonModNPcm.isHeaderUnit = true;
        nonModNPcm.filePath = nPcm;

        SEND_MESSAGE(nonModNPcm)
        CHECK_RESULT(session.finish())
    }

    // compiling big.hpp
    {
        string compileCommand =
            clangCommand + R"( -std=c++20 -fmodule-header=user -o ")" + bigPcm + "\" -useIPC -xc++-header big.hpp";

        auto managerResult = session.start(compileCommand, true);
        CHECK_RESULT(managerResult)
        auto &manager = *managerResult;

        CHECK(type == CTB::NON_MODULE)
        const auto &ctbNonModMHpp = reinterpret_cast<CTBNonModule &>(buffer);
        CHECK(ctbNonModMHpp.logicalName == "x.hpp" && ctbNonModMHpp.isHeaderUnit == false)

        BTCNonModule headerFile;
        headerFile.isHeaderUnit = false;
        headerFile.filePath = xHpp;
        HeaderFile yHeaderFile;
        yHeaderFile.logicalName = "y.hpp";
        yHeaderFile.filePath = yHpp;
        yHeaderFile.isSystem = true;
        headerFile.headerFiles.emplace_back(yHeaderFile);
        HeaderFile zHeaderFile;
        zHeaderFile.logicalName = "z.hpp";
        zHeaderFile.filePath = zHpp;
        zHeaderFile.isSystem = true;
        headerFile.headerFiles.emplace_back(zHeaderFile);

        SEND_MESSAGE(headerFile)
        CHECK_RESULT(session.finish())
    }

    // compiling foo.cpp
    {
        string compileCommand = clangCommand + R"( -std=c++20 -fmodules-reduced-bmi -o ")" + fooObj +
                                "\" -useIPC -c -xc++-module foo.cpp -fmodule-output=\"" + fooPcm + "\"";

        auto managerResult = session.start(compileCommand, true);
        CHECK_RESULT(managerResult)
        auto &manager = *managerResult;

        CHECK(type == CTB::NON_MODULE)
        const auto &xHeader = reinterpret_cast<CTBNonModule &>(buffer);
        CHECK(xHeader.logicalName == "x.hpp" && xHeader.isHeaderUnit == false)

        BTCNonModule bigHu;
        bigHu.isHeaderUnit = true;
        bigHu.logicalNames.emplace_back("big.hpp");
        bigHu.logicalNames.emplace_back("y.hpp");
        bigHu.logicalNames.emplace_back("z.hpp");

        bigHu.filePath = bigPcm;

        SEND_MESSAGE(bigHu)

        CHECK_RESULT(session.readRequest())
        CHECK(type == CTB::MODULE)
        const auto &aModule = reinterpret_cast<CTBModule &>(buffer);
        CHECK(aModule.moduleName == "A")

        BTCModule amod;
        amod.filePath = aPcm;
        ModuleDep abModDep;
        abModDep.isHeaderUnit = false;
        abModDep.filePath = aBPcm;
        abModDep.logicalNames.emplace_back("A:B");
        amod.modDeps.emplace_back(std::move(abModDep));
        ModuleDep acModDep;
        acModDep.isHeaderUnit = false;
        acModDep.filePath = aCPcm;
        acModDep.logicalNames.emplace_back("A:C");
        amod.modDeps.emplace_back(std::move(acModDep));

        SEND_MESSAGE(amod)
        CHECK_RESULT(session.finish())
    }

    // compiling main.cpp
    {
        string compileCommand = clangCommand + R"( -std=c++20 -o ")" + mainObj + "\" -useIPC -c main.cpp";

        auto managerResult = session.start(compileCommand, true);
        CHECK_RESULT(managerResult)
        auto &manager = *managerResult;

        CHECK(type == CTB::MODULE)
        const auto &ctbModule = reinterpret_cast<CTBModule &>(buffer);
        CHECK(ctbModule.moduleName == "Foo")

        BTCModule foo;
        foo.filePath = fooPcm;

        ModuleDep bigModDep;
        bigModDep.isHeaderUnit = true;
        bigModDep.filePath = bigPcm;
        bigModDep.logicalNames.emplace_back("big.hpp");
        bigModDep.logicalNames.emplace_back("x.hpp");
        bigModDep.logicalNames.emplace_back("y.hpp");
        bigModDep.logicalNames.emplace_back("z.hpp");
        foo.modDeps.emplace_back(std::move(bigModDep));

        ModuleDep aModDep;
        aModDep.isHeaderUnit = false;
        aModDep.filePath = aPcm;
        aModDep.logicalNames.emplace_back("A");
        foo.modDeps.emplace_back(std::move(aModDep));

        ModuleDep bModDep;
        bModDep.isHeaderUnit = false;
        bModDep.filePath = aBPcm;
        bModDep.logicalNames.emplace_back("A:B");
        foo.modDeps.emplace_back(std::move(bModDep));

        ModuleDep cModDep;
        cModDep.isHeaderUnit = false;
        cModDep.filePath = aCPcm;
        cModDep.logicalNames.emplace_back("A:C");
        foo.modDeps.emplace_back(std::move(cModDep));

        SEND_MESSAGE(foo)

        CHECK_RESULT(session.finish())
    }

    fflush(stdout);
    return {};
}
} // namespace

#ifdef IS_THIS_CLANG_REPO
TEST(IPC2978Test, IPC2978Test)
{
    const path p = current_path();
    current_path(LLVM_TOOLS_BINARY_DIR);
    const path mainFilePath = (LLVM_TOOLS_BINARY_DIR / path("main .o")).lexically_normal();
    const auto &r = runTest(path(LLVM_TOOLS_BINARY_DIR) / clangExecutableName);
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
int main(int argc, char **argv)
{
    if (argc > 2)
    {
        std::cerr << "Usage: " << argv[0] << " [clang-executable]\n";
        return EXIT_FAILURE;
    }
    path compiler = argc == 2 ? argv[1] : IPC2978_CLANG_EXECUTABLE;
    if (compiler.empty())
        compiler = path(".") / clangExecutableName;
    if (const auto &r = runTest(compiler); !r)
    {
        std::cout << r.error() << std::endl;
        return EXIT_FAILURE;
    }
    if (!exists(path("main .o")))
    {
        std::cout << "main.o not found" << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
