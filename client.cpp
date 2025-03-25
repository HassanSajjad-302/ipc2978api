#include <conio.h>
#include <stdio.h>
#include <string>
#include <utility>
#include <tchar.h>

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
