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

    // Read from the pipe.
    char buffer[BUFFERSIZE];
    uint64_t bytesRead;
    read(buffer, bytesRead);

    uint64_t bytesProcessed = 1;

    // read call fails if zero byte is read, so safe to process 1 byte
    switch (static_cast<CTB>(buffer[0]))
    {

    case CTB::MODULE:
        messageType = CTB::MODULE;
        reinterpret_cast<CTBModule &>(ctbBuffer).moduleName = readStringFromPipe(buffer, bytesRead, bytesProcessed);

        break;

    case CTB::HEADER_UNIT:
        messageType = CTB::HEADER_UNIT;
        reinterpret_cast<CTBHeaderUnit &>(ctbBuffer).headerUnitFilePath =
            readStringFromPipe(buffer, bytesRead, bytesProcessed);

        break;

    case CTB::RESOLVE_INCLUDE:
        messageType = CTB::RESOLVE_INCLUDE;
        reinterpret_cast<CTBResolveInclude &>(ctbBuffer).includeName =
            readStringFromPipe(buffer, bytesRead, bytesProcessed);

        break;

    case CTB::RESOLVE_HEADER_UNIT:
        messageType = CTB::RESOLVE_HEADER_UNIT;
        reinterpret_cast<CTBResolveHeaderUnit &>(ctbBuffer).logicalName =
            readStringFromPipe(buffer, bytesRead, bytesProcessed);

        break;

    case CTB::HEADER_UNIT_INCLUDE_TRANSLATION:
        messageType = CTB::HEADER_UNIT_INCLUDE_TRANSLATION;
        reinterpret_cast<CTBHeaderUnitIncludeTranslation &>(ctbBuffer).includeName =
            readStringFromPipe(buffer, bytesRead, bytesProcessed);

        break;

    case CTB::LAST_MESSAGE: {

        messageType = CTB::HEADER_UNIT_INCLUDE_TRANSLATION;

        auto &[exitStatus, hasLogicalName, headerFiles, output, errorOutput, outputFilePaths, logicalName] =
            reinterpret_cast<CTBLastMessage &>(ctbBuffer);

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

void IPCManagerBS::sendMessage(const BTCRequestedFile &requestedFile) const
{
    vector<char> buffer = getBufferWithType(BTC::REQUESTED_FILE);
    writeString(buffer, requestedFile.filePath);
    write(buffer);
}
void IPCManagerBS::sendMessage(const BTCResolvedFilePath &includePath) const
{
    vector<char> buffer = getBufferWithType(BTC::REQUESTED_FILE);
    buffer.emplace_back(includePath.exists);
    if (includePath.exists)
    {
        writeString(buffer, includePath.filePath);
    }
    write(buffer);
}

void IPCManagerBS::sendMessage(const BTCHeaderUnitOrIncludePath &headerUnitOrIncludePath) const
{
    vector<char> buffer = getBufferWithType(BTC::HEADER_UNIT_OR_INCLUDE_PATH);
    buffer.emplace_back(headerUnitOrIncludePath.exists);
    if (headerUnitOrIncludePath.exists)
    {
        buffer.emplace_back(headerUnitOrIncludePath.isHeaderUnit);
        writeString(buffer, headerUnitOrIncludePath.filePath);
    }
    write(buffer);
}

void IPCManagerBS::sendMessage(const BTCLastMessage &) const
{
    vector<char> buffer = getBufferWithType(BTC::HEADER_UNIT_OR_INCLUDE_PATH);
    write(buffer);
}
