
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
}

void IPCManagerCompiler::sendMessage(const CTBHeaderUnit &headerUnitPath)
{
    connectToBuildSystem();
    vector<char> buffer = getBufferWithType(CTB::HEADER_UNIT);
    writeString(buffer, headerUnitPath.headerUnitFilePath);
    write(buffer);
}

void IPCManagerCompiler::sendMessage(const CTBResolveInclude &resolveInclude)
{
    connectToBuildSystem();
    vector<char> buffer = getBufferWithType(CTB::RESOLVE_INCLUDE);
    writeString(buffer, resolveInclude.includeName);
    write(buffer);
}

void IPCManagerCompiler::sendMessage(const CTBResolveHeaderUnit &resolveHeaderUnit)
{
    connectToBuildSystem();
    vector<char> buffer = getBufferWithType(CTB::RESOLVE_HEADER_UNIT);
    writeString(buffer, resolveHeaderUnit.logicalName);
    write(buffer);
}

void IPCManagerCompiler::sendMessage(const CTBHeaderUnitIncludeTranslation &huIncTranslation)
{
    connectToBuildSystem();
    vector<char> buffer = getBufferWithType(CTB::HEADER_UNIT_INCLUDE_TRANSLATION);
    writeString(buffer, huIncTranslation.includeName);
    write(buffer);
}

void IPCManagerCompiler::sendMessage(const CTBLastMessage &lastMessage) const
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
}
