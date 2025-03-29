
#ifndef SERVER_HPP
#define SERVER_HPP

#include "Manager.hpp"

// IPC Manager BuildSystem
class IPCManagerBS : public Manager
{
    string pipeName;

    explicit IPCManagerBS(string pipeName_) : pipeName(std::move(pipeName_))
    {
        char p[] = TEXT("\\\\.\\pipe\\mynamedpipe");
        LPTSTR lpszPipename = p;

        hPipe = CreateNamedPipe(lpszPipename,                      // pipe name
                                PIPE_ACCESS_DUPLEX |               // read/write access
                                    FILE_FLAG_FIRST_PIPE_INSTANCE, // overlapped mode
                                PIPE_TYPE_MESSAGE |                // message-type pipe
                                    PIPE_READMODE_MESSAGE |        // message read mode
                                    PIPE_WAIT,                     // blocking mode
                                1,                                 // unlimited instances
                                BUFFERSIZE * sizeof(TCHAR),        // output buffer size
                                BUFFERSIZE * sizeof(TCHAR),        // input buffer size
                                PIPE_TIMEOUT,                      // client time-out
                                NULL);                             // default security attributes
        if (hPipe == INVALID_HANDLE_VALUE)
        {
            printf("CreateNamedPipe failed with %d.\n", GetLastError());
        }
    }

    void initialize()
    {
    }

    void receiveMessage(char (&ctbBuffer)[320])
    {
        // Has to first wait for the message

        // Read from the pipe.
        char buffer[BUFFERSIZE];
        uint64_t bytesRead;
        read(buffer, bytesRead);

        uint64_t bytesProcessed = 0;

        // read call fails if zero byte is read, so safe to read 1 byte
        switch (static_cast<CTB>(buffer[0]))
        {

        case CTB::MODULE:
            CTBModule &m = reinterpret_cast<CTBModule &>(ctbBuffer);
            m.moduleName = readStringFromPipe(buffer, bytesRead, bytesProcessed);
            break;
        case CTB::HEADER_UNIT:
            break;
        case CTB::RESOLVE_INCLUDE:
            break;
        case CTB::HEADER_UNIT_INCLUDE_TRANSLATION:
            break;
        case CTB::LAST_MESSAGE:
            break;
        }
    }

    static vector<char> getBufferWithType(const BTC type)
    {
        vector<char> buffer;
        buffer.emplace_back(static_cast<char>(type));
        return buffer;
    }

    static void writeString(vector<char> &buffer, const string &str)
    {
        uint64_t size = str.size();
        buffer.emplace_back(&size, sizeof(size));
        buffer.emplace_back(str.data(), size);
    }

    void sendMessage(const BTC_RequestedFile &requestedFile) const
    {
        vector<char> buffer = getBufferWithType(BTC::REQUESTED_FILE);
        writeString(buffer, requestedFile.filePath);
        write(buffer);
    }

    void sendMessage(const BTC_IncludePath &includePath) const
    {
        vector<char> buffer = getBufferWithType(BTC::REQUESTED_FILE);
        writeString(buffer, includePath.filePath);
        write(buffer);
    }

    void sendMessage(const BTC_HeaderUnitOrIncludePath &headerUnitOrIncludePath) const
    {
        vector<char> buffer = getBufferWithType(BTC::HEADER_UNIT_OR_INCLUDE_PATH);
        buffer.emplace_back(headerUnitOrIncludePath.found);
        if (headerUnitOrIncludePath.found)
        {
            buffer.emplace_back(headerUnitOrIncludePath.isHeaderUnit);
            writeString(buffer, headerUnitOrIncludePath.filePath);
        }
        write(buffer);
    }

    void sendMessage(const BTC_LastMessage &lastMessage) const
    {
        vector<char> buffer = getBufferWithType(BTC::HEADER_UNIT_OR_INCLUDE_PATH);
        write(buffer);
    }

    BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
    {
        BOOL fConnected, fPendingIO = FALSE;

        // Start an overlapped connection for this pipe instance.
        fConnected = ConnectNamedPipe(hPipe, lpo);

        // Overlapped ConnectNamedPipe should return zero.
        if (fConnected)
        {
            printf("ConnectNamedPipe failed with %d.\n", GetLastError());
            return 0;
        }

        switch (GetLastError())
        {
        // The overlapped connection in progress.
        case ERROR_IO_PENDING:
            fPendingIO = TRUE;
            break;

            // Client is already connected, so signal an event.

        case ERROR_PIPE_CONNECTED:
            if (SetEvent(lpo->hEvent))
                break;

        // If an error occurs during the connect operation...
        default: {
            printf("ConnectNamedPipe failed with %d.\n", GetLastError());
            return 0;
        }
        }
        return fPendingIO;
    }
};
#endif // SERVER_HPP
