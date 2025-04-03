
#include "IPCManagerCompiler.hpp"
#include "Manager.hpp"
#include "Messages.hpp"

#include <print>
#include <string>
#include <strsafe.h>
#include <tchar.h>
#include <windows.h>

using std::string, std::print;

IPCManagerCompiler::IPCManagerCompiler(const string &objFilePath) : pipeName(R"(\\.\pipe\)" + objFilePath)
{
}

void IPCManagerCompiler::connectToBuildSystem()
{
    if (connectedToBuildSystem)
    {
        return;
    }

    hPipe = CreateFile(pipeName.data(), // pipe name
                       GENERIC_READ |   // read and write access
                           GENERIC_WRITE,
                       0,             // no sharing
                       nullptr,       // default security attributes
                       OPEN_EXISTING, // opens existing pipe
                       0,             // default attributes
                       nullptr);      // no template file

    // Break if the pipe handle is valid.

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        print("Could not open pipe {} to build system\n", pipeName);
    }

    connectedToBuildSystem = true;
}

void IPCManagerCompiler::sendMessage(const CTBModule &moduleName)
{
    connectToBuildSystem();
    vector<char> buffer = getBufferWithType(CTB::MODULE);
    writeString(buffer, moduleName.moduleName);
    write(buffer);
    expectedMessageType = BTC::MODULE;
}

void IPCManagerCompiler::sendMessage(const CTBNonModule &nonModule)
{
    connectToBuildSystem();
    vector<char> buffer = getBufferWithType(CTB::MODULE);
    buffer.emplace_back(nonModule.isHeaderUnit);
    writeString(buffer, nonModule.str);
    write(buffer);
    expectedMessageType = BTC::NON_MODULE;
}

void IPCManagerCompiler::sendMessage(const CTBLastMessage &lastMessage)
{
    vector<char> buffer = getBufferWithType(CTB::LAST_MESSAGE);
    buffer.emplace_back(lastMessage.exitStatus);
    if (lastMessage.exitStatus == EXIT_SUCCESS)
    {
        buffer.emplace_back(lastMessage.hasLogicalName);
        writeVectorOfStrings(buffer, lastMessage.headerFiles);
        writeString(buffer, lastMessage.output);
        writeString(buffer, lastMessage.errorOutput);
        writeVectorOfStrings(buffer, lastMessage.outputFilePaths);
        if (lastMessage.hasLogicalName)
        {
            writeString(buffer, lastMessage.logicalName);
        }
    }
    write(buffer);
    expectedMessageType = BTC::LAST_MESSAGE;
}
