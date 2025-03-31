
#include "Manager.hpp"
#include "Messages.hpp"
#include <Windows.h>
#include <print>

using std::print;

void Manager::read(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead) const
{
    const bool success = ReadFile(hPipe,               // pipe handle
                                  buffer,              // buffer to receive reply
                                  BUFFERSIZE,          // size of buffer
                                  LPDWORD(&bytesRead), // number of bytes read
                                  nullptr);            // not overlapped

    if (!bytesRead)
    {
        print(stderr, "ReadFile failed. Zero bytes Read. Error-Code {}.\n", GetLastError());
        return;
    }

    if (const uint32_t lastError = GetLastError(); !success && lastError != ERROR_MORE_DATA)
    {
        print(stderr, "ReadFile failed with             {}.\n", lastError);
    }
}

void Manager::write(const vector<char> &buffer) const
{
    const bool success = WriteFile(hPipe,         // pipe handle
                                   buffer.data(), // message
                                   buffer.size(), // message length
                                   nullptr,       // bytes written
                                   nullptr);      // not overlapped
    if (!success)
    {
        print(stderr, "WriteFile failed with %d.\n", GetLastError());
    }
}

vector<char> Manager::getBufferWithType(CTB type)
{
    vector<char> buffer;
    buffer.emplace_back(static_cast<uint8_t>(type));
    return buffer;
}

vector<char> Manager::getBufferWithType(const BTC type)
{
    vector<char> buffer;
    buffer.emplace_back(static_cast<uint8_t>(type));
    return buffer;
}

void Manager::writeString(vector<char> &buffer, const string &str)
{
    const uint32_t size = str.size();
    const auto ptr = reinterpret_cast<const char *>(&size);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(size));
    buffer.insert(buffer.end(), str.begin(), str.end()); // Insert all characters
}

void Manager::writeVectorOfStrings(vector<char> &buffer, const vector<string> &strs)
{
    const uint32_t size = strs.size();
    const auto ptr = reinterpret_cast<const char *>(&size);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(size));
    for (const string &str : strs)
    {
        writeString(buffer, str);
    }
}

bool Manager::readBoolFromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead, uint32_t &bytesProcessed) const
{
    bool result;
    readNumberOfBytes(reinterpret_cast<char *>(&result), sizeof(result), buffer, bytesRead, bytesProcessed);
    return result;
}

string Manager::readStringFromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead, uint32_t &bytesProcessed) const
{
    uint32_t stringSize;
    readNumberOfBytes(reinterpret_cast<char *>(&stringSize), sizeof(stringSize), buffer, bytesRead, bytesProcessed);
    string str(stringSize, 'a');
    readNumberOfBytes(str.data(), stringSize, buffer, bytesRead, bytesProcessed);
    return str;
}

vector<string> Manager::readVectorOfStringFromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead,
                                                   uint32_t &bytesProcessed) const
{
    uint32_t vectorSize;
    readNumberOfBytes(reinterpret_cast<char *>(&vectorSize), sizeof(vectorSize), buffer, bytesRead, bytesProcessed);
    vector<string> vec;
    vec.reserve(vectorSize);
    for (uint32_t i = 0; i < vectorSize; ++i)
    {
        vec.emplace_back(readStringFromPipe(buffer, bytesRead, bytesProcessed));
    }
    return vec;
}

vector<string> Manager::readVectorOfMaybeMappedFileFromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead,
                                                            uint32_t &bytesProcessed) const
{
    uint32_t vectorSize;
    readNumberOfBytes(reinterpret_cast<char *>(&vectorSize), sizeof(vectorSize), buffer, bytesRead, bytesProcessed);
    vector<string> vec;
    vec.reserve(vectorSize);
    for (uint32_t i = 0; i < vectorSize; ++i)
    {
        vec.emplace_back(readStringFromPipe(buffer, bytesRead, bytesProcessed));
    }
    return vec;
}

void Manager::readNumberOfBytes(char *output, const uint32_t size, char (&buffer)[BUFFERSIZE], uint32_t &bytesRead,
                                uint32_t &bytesProcessed) const
{
    uint32_t pendingSize = size;
    uint32_t offset = 0;
    while (true)
    {
        const uint32_t bytesAvailable = bytesRead - bytesProcessed;
        if (bytesAvailable >= pendingSize)
        {
            memcpy(output + offset, buffer + bytesProcessed, pendingSize);
            bytesProcessed += pendingSize;
            break;
        }

        if (bytesAvailable)
        {
            memcpy(output + offset, buffer + bytesProcessed, bytesAvailable);
            offset += bytesAvailable;
            pendingSize -= bytesAvailable;
        }

        bytesProcessed = 0;
        read(buffer, bytesRead);
    }
}
