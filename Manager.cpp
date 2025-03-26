
#include "Manager.hpp"
#include "Messages.hpp"
#include <Windows.h>
#include <print>

using std::print;

void Manager::read(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead) const
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

bool Manager::readBoolFromPipe(char (&buffer)[4096], uint64_t &bytesRead, uint64_t &bytesProcessed)
{
    bool result;
    readNumberOfBytes(reinterpret_cast<char *>(&result), 1, buffer, bytesRead, bytesProcessed);
    return result;
}

string Manager::readStringFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed)
{
    uint64_t stringSize;
    readNumberOfBytes(reinterpret_cast<char *>(&stringSize), 8, buffer, bytesRead, bytesProcessed);
    string str(stringSize, 'a');
    readNumberOfBytes(str.data(), stringSize, buffer, bytesRead, bytesProcessed);
    return str;
}

MaybeMappedFile Manager::readMaybeMappedFileFromPipe(char (&buffer)[4096], uint64_t &bytesRead,
                                                     uint64_t &bytesProcessed)
{
    MaybeMappedFile file;
    file.isMapped = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
    file.filePath = readStringFromPipe(buffer, bytesRead, bytesProcessed);
    return file;
}

vector<string> Manager::readVectorOfStringFromPipe(char (&buffer)[4096], uint64_t &bytesRead, uint64_t &bytesProcessed)
{
    uint64_t vectorSize;
    readNumberOfBytes(reinterpret_cast<char *>(&vectorSize), 8, buffer, bytesRead, bytesProcessed);
    vector<string> vec;
    vec.reserve(vectorSize);
    for (uint64_t i = 0; i < vectorSize; ++i)
    {
        vec.emplace_back(readStringFromPipe(buffer, bytesRead, bytesProcessed));
    }
    return vec;
}

vector<MaybeMappedFile> Manager::readVectorOfMaybeMappedFileFromPipe(char (&buffer)[4096], uint64_t &bytesRead,
                                                                     uint64_t &bytesProcessed)
{
    uint64_t vectorSize;
    readNumberOfBytes(reinterpret_cast<char *>(&vectorSize), 8, buffer, bytesRead, bytesProcessed);
    vector<MaybeMappedFile> vec;
    vec.reserve(vectorSize);
    for (uint64_t i = 0; i < vectorSize; ++i)
    {
        vec.emplace_back(readMaybeMappedFileFromPipe(buffer, bytesRead, bytesProcessed));
    }
    return vec;
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
