
#include "client.hpp"
#include "Manager.hpp"
#include "Messages.hpp"

#include <cstdio>
#include <print>
#include <string>
#include <strsafe.h>
#include <tchar.h>
#include <windows.h>

using std::string, std::print;

IPCManagerCompiler::IPCManagerCompiler(const string &objFilePath) : pipeName(R"(\\.\pipe\mynamedpipe)" + objFilePath)
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

void IPCManagerCompiler::connectToNewClient() const
{
    // Start an overlapped connection for this pipe instance.

    // Overlapped ConnectNamedPipe should return zero.
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
}

void IPCManagerCompiler::receiveMessage(char (&ctbBuffer)[320], CTB &messageType)
{
    if (!connectedToClient)
    {
        connectToNewClient();
        connectedToClient = true;
    }

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

    case CTB::LAST_MESSAGE:
        messageType = CTB::HEADER_UNIT_INCLUDE_TRANSLATION;

        CTBLastMessage &lastMessage = reinterpret_cast<CTBLastMessage &>(ctbBuffer);
        lastMessage.from(*this, buffer, bytesRead, bytesProcessed);

        break;
    }

    if (bytesRead != bytesProcessed)
    {
        print("BytesRead {} not equal to BytesProcessed {} in receiveMessage.\n", bytesRead, bytesProcessed);
    }
}

void IPCManagerCompiler::sendMessage(const BTC_RequestedFile &requestedFile) const
{
    vector<char> buffer = getBufferWithType(BTC::REQUESTED_FILE);
    writeString(buffer, requestedFile.filePath);
    write(buffer);
}
void IPCManagerCompiler::sendMessage(const BTC_ResolvedFilePath &includePath) const
{
    vector<char> buffer = getBufferWithType(BTC::REQUESTED_FILE);
    buffer.emplace_back(includePath.exists);
    if (includePath.exists)
    {
        writeString(buffer, includePath.filePath);
    }
    write(buffer);
}

void IPCManagerCompiler::sendMessage(const BTC_HeaderUnitOrIncludePath &headerUnitOrIncludePath) const
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

void IPCManagerCompiler::sendMessage(const BTC_LastMessage &) const
{
    vector<char> buffer = getBufferWithType(BTC::HEADER_UNIT_OR_INCLUDE_PATH);
    write(buffer);
}

#include <conio.h>
#include <stdio.h>
#include <string>
#include <tchar.h>
#include <utility>

#include "Messages.hpp"
#include <windows.h>

using std::string;

#define BUFSIZE 4096

int main(int argc, TCHAR *argv[])
{
    string str("Default message from client.");
    LPTSTR lpvMessage = str.data();
    TCHAR chBuf[BUFSIZE];
    BOOL fSuccess = FALSE;
    DWORD cbRead, cbToWrite, cbWritten, dwMode;

    string pipeName("\\\\.\\pipe\\mynamedpipe");
    LPTSTR lpszPipename = pipeName.data();

    if (argc > 1)
        lpvMessage = argv[1];

    // Try to open a named pipe; wait for it, if necessary.

    HANDLE hPipe = CreateFile(lpszPipename,  // pipe name
                              GENERIC_READ | // read and write access
                                  GENERIC_WRITE,
                              0,             // no sharing
                              NULL,          // default security attributes
                              OPEN_EXISTING, // opens existing pipe
                              0,             // default attributes
                              NULL);         // no template file

    // Break if the pipe handle is valid.

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        // Exit if an error other than ERROR_PIPE_BUSY occurs.

        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            _tprintf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
            return -1;
        }

        // All pipe instances are busy, so wait for 20 seconds.

        if (!WaitNamedPipe(lpszPipename, 20000))
        {
            printf("Could not open pipe: 20 second wait timed out.");
            return -1;
        }
    }

    // Send a message to the pipe server.

    cbToWrite = (lstrlen(lpvMessage) + 1) * sizeof(TCHAR);
    _tprintf(TEXT("Sending %d byte message: \"%s\"\n"), cbToWrite, lpvMessage);

    fSuccess = WriteFile(hPipe,      // pipe handle
                         lpvMessage, // message
                         cbToWrite,  // message length
                         &cbWritten, // bytes written
                         NULL);      // not overlapped

    if (!fSuccess)
    {
        _tprintf(TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError());
        return -1;
    }

    printf("\nMessage sent to server, receiving reply as follows:\n");

    do
    {
        // Read from the pipe.

        fSuccess = ReadFile(hPipe,                   // pipe handle
                            chBuf,                   // buffer to receive reply
                            BUFSIZE * sizeof(TCHAR), // size of buffer
                            &cbRead,                 // number of bytes read
                            NULL);                   // not overlapped

        if (!fSuccess && GetLastError() != ERROR_MORE_DATA)
            break;

        _tprintf(TEXT("\"%s\"\n"), chBuf);
    } while (!fSuccess); // repeat loop if ERROR_MORE_DATA

    if (!fSuccess)
    {
        _tprintf(TEXT("ReadFile from pipe failed. GLE=%d\n"), GetLastError());
        return -1;
    }

    printf("\n<End of message, press ENTER to terminate connection and exit>");
    _getch();

    CloseHandle(hPipe);

    return 0;
}
