
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

BTCModule IPCManagerCompiler::receiveBTCModule(const CTBModule &moduleName)
{
    connectToBuildSystem();
    vector<char> buffer = getBufferWithType(CTB::MODULE);
    writeString(buffer, moduleName.moduleName);
    write(buffer);
    return receiveMessage<BTCModule>();
}

BTCNonModule IPCManagerCompiler::receiveBTCNonModule(const CTBNonModule &nonModule)
{
    connectToBuildSystem();
    vector<char> buffer = getBufferWithType(CTB::NON_MODULE);
    buffer.emplace_back(nonModule.isHeaderUnit);
    writeString(buffer, nonModule.str);
    write(buffer);
    return receiveMessage<BTCNonModule>();
}

void IPCManagerCompiler::sendBTCLastMessage(const CTBLastMessage &lastMessage)
{
    connectToBuildSystem();
    vector<char> buffer = getBufferWithType(CTB::LAST_MESSAGE);
    buffer.emplace_back(lastMessage.exitStatus);
    buffer.emplace_back(lastMessage.hasLogicalName);
    writeVectorOfStrings(buffer, lastMessage.headerFiles);
    writeString(buffer, lastMessage.output);
    writeString(buffer, lastMessage.errorOutput);
    writeVectorOfStrings(buffer, lastMessage.outputFilePaths);
    writeString(buffer, lastMessage.logicalName);
    write(buffer);
}

void IPCManagerCompiler::sendBTCLastMessage(const CTBLastMessage &lastMessage, const string &bmiFile)
{
    const HANDLE hFile = CreateFile(lastMessage.outputFilePaths[0].c_str(), GENERIC_READ | GENERIC_WRITE,
                                    0, // no sharing during setup
                                    nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
    }

    LARGE_INTEGER fileSize;
    fileSize.QuadPart = bmiFile.size();
    // 3) Create a RW mapping of that file:
    const HANDLE hMap = CreateFileMapping(hFile, nullptr, PAGE_READWRITE, fileSize.HighPart, fileSize.LowPart,
                                          lastMessage.outputFilePaths[0].c_str());
    if (!hMap)
    {
        CloseHandle(hFile);
    }

    void *pView = MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, bmiFile.size());
    if (!pView)
    {
        CloseHandle(hMap);
        CloseHandle(hFile);
    }

    memcpy(pView, bmiFile.c_str(), bmiFile.size());

    if (!FlushViewOfFile(pView, bmiFile.size()))
    {
        // even if flush fails, we’ll still tear down handles
    }

    UnmapViewOfFile(pView);

    CloseHandle(hFile);

    sendBTCLastMessage(lastMessage);
    if (lastMessage.exitStatus == EXIT_SUCCESS)
    {
        receiveMessage<BTCLastMessage>();
    }

    CloseHandle(hMap);
}
