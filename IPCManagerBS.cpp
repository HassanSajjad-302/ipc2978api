#include "IPCManagerBS.hpp"
#include "Manager.hpp"
#include "Messages.hpp"

#include <cstdio>
#include <print>
#include <string>
#include <strsafe.h>
#include <tchar.h>
#include <windows.h>

using std::string, std::print;

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

        auto &[exitStatus, hasLogicalName, headerFiles, output, errorOutput, outputFilePaths, logicalName] =
            getInitializedObjectFromBuffer<CTBLastMessage>(ctbBuffer);

        exitStatus = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        if (exitStatus != EXIT_SUCCESS)
        {
            return;
        }
        hasLogicalName = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        headerFiles = readVectorOfStringFromPipe(buffer, bytesRead, bytesProcessed);
        output = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        errorOutput = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        outputFilePaths = readVectorOfMaybeMappedFileFromPipe(buffer, bytesRead, bytesProcessed);
        if (hasLogicalName)
        {
            logicalName = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        }
    }
    break;

    default:

        print("Build-System received unknown message type on pipe {}.\n", pipeName);
    }

    if (bytesRead != bytesProcessed)
    {
        print("BytesRead {} not equal to BytesProcessed {} in receiveMessage.\n", bytesRead, bytesProcessed);
    }
}

void IPCManagerBS::sendMessage(const BTCModule &moduleFile) const
{
    vector<char> buffer;
    writeString(buffer, moduleFile.filePath);
    write(buffer);
}

void IPCManagerBS::sendMessage(const BTCNonModule &nonModule) const
{
    vector<char> buffer;
    buffer.emplace_back(nonModule.found);
    buffer.emplace_back(nonModule.isHeaderUnit);
    writeString(buffer, nonModule.filePath);
    write(buffer);
}

void IPCManagerBS::sendMessage(const BTCLastMessage &) const
{
    vector<char> buffer;
    buffer.emplace_back(UINT32_MAX);
    write(buffer);
}
