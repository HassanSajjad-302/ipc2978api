
#include "Manager.hpp"
#include "Messages.hpp"
#include "fmt/fmt/printf.h"
#include <Windows.h>

using fmt::print;

namespace N2978
{

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

void Manager::writeUInt32(vector<char> &buffer, const uint32_t value)
{
    const auto ptr = reinterpret_cast<const char *>(&value);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(value));
}

void Manager::writeString(vector<char> &buffer, const string &str)
{
    writeUInt32(buffer, str.size());
    buffer.insert(buffer.end(), str.begin(), str.end()); // Insert all characters
}

void Manager::writeMemoryMappedBMIFile(vector<char> &buffer, const BMIFile &file)
{
    writeUInt32(buffer, file.fileSize);
    writeUInt32(buffer, file.fileSize);
}

void Manager::writeModuleDep(vector<char> &buffer, const ModuleDep &dep)
{
    writeMemoryMappedBMIFile(buffer, dep.file);
    writeString(buffer, dep.logicalName);
}

void Manager::writeHuDep(vector<char> &buffer, const HuDep &dep)
{
    writeMemoryMappedBMIFile(buffer, dep.file);
    writeString(buffer, dep.logicalName);
    buffer.emplace_back(dep.angled);
}

void Manager::writeVectorOfStrings(vector<char> &buffer, const vector<string> &strs)
{
    writeUInt32(buffer, strs.size());
    for (const string &str : strs)
    {
        writeString(buffer, str);
    }
}

void Manager::writeVectorOfMemoryMappedBMIFiles(vector<char> &buffer, const vector<BMIFile> &files)
{
    writeUInt32(buffer, files.size());
    for (const BMIFile &file : files)
    {
        writeMemoryMappedBMIFile(buffer, file);
    }
}

void Manager::writeVectorOfModuleDep(vector<char> &buffer, const vector<ModuleDep> &deps)
{
    writeUInt32(buffer, deps.size());
    for (const ModuleDep &dep : deps)
    {
        writeModuleDep(buffer, dep);
    }
}

void Manager::writeVectorOfHuDep(vector<char> &buffer, const vector<HuDep> &deps)
{
    writeUInt32(buffer, deps.size());
    for (const HuDep &dep : deps)
    {
        writeHuDep(buffer, dep);
    }
}

bool Manager::readBoolFromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead, uint32_t &bytesProcessed) const
{
    bool result;
    readNumberOfBytes(reinterpret_cast<char *>(&result), sizeof(result), buffer, bytesRead, bytesProcessed);
    return result;
}

uint32_t Manager::readUInt32FromPipe(char (&buffer)[4096], uint32_t &bytesRead, uint32_t &bytesProcessed) const
{
    uint32_t size;
    readNumberOfBytes(reinterpret_cast<char *>(&size), 4, buffer, bytesRead, bytesProcessed);
    return size;
}

string Manager::readStringFromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead, uint32_t &bytesProcessed) const
{
    const uint32_t stringSize = readUInt32FromPipe(buffer, bytesRead, bytesProcessed);
    string str(stringSize, 'a');
    readNumberOfBytes(str.data(), stringSize, buffer, bytesRead, bytesProcessed);
    return str;
}

BMIFile Manager::readMemoryMappedBMIFileFromPipe(char (&buffer)[4096], uint32_t &bytesRead,
                                                 uint32_t &bytesProcessed) const
{
    BMIFile file;
    file.filePath = readStringFromPipe(buffer, bytesRead, bytesProcessed);
    file.fileSize = readUInt32FromPipe(buffer, bytesRead, bytesProcessed);
    return file;
}

vector<string> Manager::readVectorOfStringFromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead,
                                                   uint32_t &bytesProcessed) const
{
    const uint32_t vectorSize = readUInt32FromPipe(buffer, bytesRead, bytesProcessed);
    vector<string> vec;
    vec.reserve(vectorSize);
    for (uint32_t i = 0; i < vectorSize; ++i)
    {
        vec.emplace_back(readStringFromPipe(buffer, bytesRead, bytesProcessed));
    }
    return vec;
}

ModuleDep Manager::readModuleDepFromPipe(char (&buffer)[4096], uint32_t &bytesRead, uint32_t &bytesProcessed) const
{
    ModuleDep modDep;
    modDep.file.filePath = readStringFromPipe(buffer, bytesRead, bytesProcessed);
    modDep.file.fileSize = readUInt32FromPipe(buffer, bytesRead, bytesProcessed);
    modDep.logicalName = readStringFromPipe(buffer, bytesRead, bytesProcessed);
    return modDep;
}

vector<ModuleDep> Manager::readVectorOfModuleDepFromPipe(char (&buffer)[4096], uint32_t &bytesRead,
                                                         uint32_t &bytesProcessed) const
{
    const uint32_t vectorSize = readUInt32FromPipe(buffer, bytesRead, bytesProcessed);
    vector<ModuleDep> vec;
    vec.reserve(vectorSize);
    for (uint32_t i = 0; i < vectorSize; ++i)
    {
        vec.emplace_back(readModuleDepFromPipe(buffer, bytesRead, bytesProcessed));
    }
    return vec;
}

HuDep Manager::readHuDepFromPipe(char (&buffer)[4096], uint32_t &bytesRead, uint32_t &bytesProcessed) const
{
    HuDep huDep;
    huDep.file = readMemoryMappedBMIFileFromPipe(buffer, bytesRead, bytesProcessed);
    huDep.logicalName = readStringFromPipe(buffer, bytesRead, bytesProcessed);
    huDep.angled = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
    return huDep;
}

vector<HuDep> Manager::readVectorOfHuDepFromPipe(char (&buffer)[4096], uint32_t &bytesRead,
                                                 uint32_t &bytesProcessed) const
{
    const uint32_t vectorSize = readUInt32FromPipe(buffer, bytesRead, bytesProcessed);
    vector<HuDep> vec;
    vec.reserve(vectorSize);
    for (uint32_t i = 0; i < vectorSize; ++i)
    {
        vec.emplace_back(readHuDepFromPipe(buffer, bytesRead, bytesProcessed));
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
} // namespace N2978