#include "IPCManagerBS.hpp"
#include "Manager.hpp"
#include "Messages.hpp"

#include "fmt/printf.h"
#include <cstdio>
#include <string>
#include <strsafe.h>
#include <tchar.h>
#include <windows.h>

using std::string, fmt::print;

namespace N2978
{

IPCManagerBS::IPCManagerBS(const string &objFilePath) : pipeName(R"(\\.\pipe\)" + objFilePath)
{
    hPipe = CreateNamedPipe(pipeName.c_str(),                  // pipe name
                            PIPE_ACCESS_DUPLEX |               // read/write access
                                FILE_FLAG_FIRST_PIPE_INSTANCE, // overlapped mode
                            PIPE_TYPE_MESSAGE |                // message-type pipe
                                PIPE_READMODE_MESSAGE |        // message read mode
                                PIPE_WAIT,                     // blocking mode
                            1,                                 // unlimited instances
                            BUFFERSIZE * sizeof(TCHAR),        // output buffer size
                            BUFFERSIZE * sizeof(TCHAR),        // input buffer size
                            PIPE_TIMEOUT,                      // client time-out
                            nullptr);                          // default security attributes
    if (hPipe == INVALID_HANDLE_VALUE)
    {
        printf("CreateNamedPipe failed with %d.\n", GetLastError());
    }
}

void IPCManagerBS::connectToCompiler()
{
    if (connectedToCompiler)
    {
        return;
    }

    // ConnectNamedPipe should return zero.
    if (!ConnectNamedPipe(hPipe, nullptr))
    {
        switch (GetLastError())
        {
            // Client is already connected, so signal an event.
        case ERROR_PIPE_CONNECTED:
            break;

        // If an error occurs during the connect operation...
        default:
            printf("ConnectNamedPipe failed with %d.\n", GetLastError());
        }
    }
    connectedToCompiler = true;
}

void IPCManagerBS::receiveMessage(char (&ctbBuffer)[320], CTB &messageType)
{
    connectToCompiler();
    // Read from the pipe.
    char buffer[BUFFERSIZE];
    uint32_t bytesRead;
    read(buffer, bytesRead);

    uint32_t bytesProcessed = 1;

    // read call fails if zero byte is read, so safe to process 1 byte
    switch (static_cast<CTB>(buffer[0]))
    {

    case CTB::MODULE: {

        messageType = CTB::MODULE;
        getInitializedObjectFromBuffer<CTBModule>(ctbBuffer).moduleName =
            readStringFromPipe(buffer, bytesRead, bytesProcessed);
    }

    break;

    case CTB::NON_MODULE: {

        messageType = CTB::NON_MODULE;
        auto &[isHeaderUnit, str] = getInitializedObjectFromBuffer<CTBNonModule>(ctbBuffer);
        isHeaderUnit = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        str = readStringFromPipe(buffer, bytesRead, bytesProcessed);
    }

    break;

    case CTB::LAST_MESSAGE: {

        messageType = CTB::LAST_MESSAGE;

        auto &[exitStatus, headerFiles, output, errorOutput, logicalName, fileSize] =
            getInitializedObjectFromBuffer<CTBLastMessage>(ctbBuffer);

        exitStatus = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        headerFiles = readVectorOfStringFromPipe(buffer, bytesRead, bytesProcessed);
        output = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        errorOutput = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        logicalName = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        fileSize = readUInt32FromPipe(buffer, bytesRead, bytesProcessed);
    }
    break;

    default:

        print("Build-System received unknown message type on pipe {}.\n{}\n", pipeName, GetLastError());
    }

    if (bytesRead != bytesProcessed)
    {
        print("BytesRead {} not equal to BytesProcessed {} in receiveMessage.\n{}\n", bytesRead, bytesProcessed,
              GetLastError());
    }
}

void IPCManagerBS::sendMessage(const BTCModule &moduleFile) const
{
    vector<char> buffer;
    writeMemoryMappedBMIFile(buffer, moduleFile.requested);
    writeVectorOfModuleDep(buffer, moduleFile.deps);
    write(buffer);
}

void IPCManagerBS::sendMessage(const BTCNonModule &nonModule) const
{
    vector<char> buffer;
    buffer.emplace_back(nonModule.isHeaderUnit);
    writeString(buffer, nonModule.filePath);
    buffer.emplace_back(nonModule.angled);
    writeUInt32(buffer, nonModule.fileSize);
    writeVectorOfHuDep(buffer, nonModule.deps);
    write(buffer);
}

void IPCManagerBS::sendMessage(const BTCLastMessage &) const
{
    vector<char> buffer;
    buffer.emplace_back(false);
    write(buffer);
}

void *IPCManagerBS::createSharedMemoryBMIFile(const string &bmiFilePath)
{
    // 1) Open the existing file‐mapping object (must have been created by another process)
    const HANDLE mapping = OpenFileMapping(FILE_MAP_READ,     // read‐only access
                                           FALSE,             // do not inherit handle
                                           bmiFilePath.data() // name of mapping
    );

    if (mapping == nullptr)
    {
        print("Could not open file mapping of file {}.\n{}\n", bmiFilePath, GetLastError());
    }

    return mapping;
}
} // namespace N2978