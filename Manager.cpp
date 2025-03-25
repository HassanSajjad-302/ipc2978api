
#include "Manager.hpp"
#include <Windows.h>
#include <print>

using std::print;

void Manager::read(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead)
{
    const bool fSuccess = ReadFile(hPipe,               // pipe handle
                                   buffer,              // buffer to receive reply
                                   BUFFERSIZE,          // size of buffer
                                   LPDWORD(&bytesRead), // number of bytes read
                                   NULL);               // not overlapped

    if (!fSuccess && GetLastError() != ERROR_MORE_DATA)
    {
        print(stderr, "ReadFile failed with %d.\n", GetLastError());
    }
}

void Manager::readStringFromPipe(string &output, char (&buffer)[BUFFERSIZE], uint64_t &bytesRead,
                                 uint64_t &bytesProcessed)
{
    // Determining the size of the string
    if (uint64_t bytesNeeded = 8; bytesProcessed - bytesRead >= bytesNeeded)
    {
        const uint64_t stringSize = buffer[bytesProcessed];
        bytesProcessed += bytesNeeded;
        bytesNeeded = stringSize;
    }
    else
    {
    }
}

void Manager::readNumberOfBytes(char *output, const uint64_t size, char (&buffer)[4096], uint64_t &bytesRead,
                                uint64_t &bytesProcessed)
{
    uint64_t pendingSize = size;
    while (true)
    {
        const uint64_t bytesAvailable = bytesRead - bytesProcessed;
        if (bytesAvailable >= pendingSize)
        {
            memcpy(output, buffer + bytesProcessed, pendingSize);
            bytesProcessed += pendingSize;
            break;
        }

        if (bytesAvailable)
        {
            memcpy(output, buffer + bytesProcessed, bytesAvailable);
            pendingSize -= bytesAvailable;
        }

        bytesProcessed = 0;
        read(buffer, bytesRead);
    }
}
