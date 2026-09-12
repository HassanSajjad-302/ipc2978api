
#include "Manager.hpp"
#include "Messages.hpp"
#include "expected.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace P2978
{

std::string getErrorString()
{
#ifdef _WIN32
    const DWORD err = GetLastError();

    char *msg_buf = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                   err, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), reinterpret_cast<char *>(&msg_buf), 0, nullptr);

    if (msg_buf == nullptr)
    {
        char fallback_msg[128] = {};
        snprintf(fallback_msg, sizeof(fallback_msg), "GetLastError() = %ld", err);
        return fallback_msg;
    }

    std::string msg = msg_buf;
    LocalFree(msg_buf);
    return msg;
#else
    return {std::strerror(errno)};
#endif
}

std::string getErrorString(const uint64_t bytesRead_, const uint64_t bytesProcessed_)
{
    return "Error: Bytes Readd vs Bytes Processed Mismatch.\nBytes Read: " + std::to_string(bytesRead_) +
           ", Bytes Processed: " + std::to_string(bytesProcessed_);
}

std::string getErrorString(const ErrorCategory errorCategory_)
{
    std::string errorString;

    switch (errorCategory_)
    {
    case ErrorCategory::PARSING_ERROR:
        errorString = "P2978 Message Parsing Error.";
        break;
    case ErrorCategory::READ_FILE_ZERO_BYTES_READ:
        errorString = "Error: ReadFile Zero Bytes Read.";
        break;
    case ErrorCategory::UNKNOWN_CTB_TYPE:
        errorString = "Error: Unknown CTB message received.";
        break;
    case ErrorCategory::NONE:
        std::string str = __FILE__;
        str += ':';
        str += __LINE__;
        errorString = "P2978 IPC API internal error" + str;
        break;
    }

    return errorString;
}

#ifndef _WIN32
tl::expected<void, std::string> Manager::writeAll(const int fd, const char *buffer, const uint64_t count)
{
    uint64_t bytesWritten = 0;

    while (bytesWritten != count)
    {
        // Converting the syscall's -1 result to uint64_t gives UINT64_MAX.
        const uint64_t result = write(fd, buffer + bytesWritten,
                                      std::min<uint64_t>(count - bytesWritten, SSIZE_MAX));
        if (result == UINT64_MAX)
        {
            if (errno == EINTR)
            {
                // Interrupted by signal: retry
                continue;
            }
            return tl::unexpected(getErrorString());
        }
        if (result == 0)
        {
            // According to POSIX, write() returning 0 is only valid for count == 0
            return tl::unexpected(getErrorString());
        }
        bytesWritten += result;
    }

    return {};
}
#else
tl::expected<void, std::string> Manager::writeAll(void *handle, std::string_view buffer)
{
    uint64_t offset = 0;
    while (offset < buffer.size())
    {
        const DWORD count = static_cast<DWORD>(std::min<uint64_t>(buffer.size() - offset, MAXDWORD));
        DWORD written = 0;
        if (!WriteFile(handle, buffer.data() + offset, count, &written, nullptr))
            return tl::unexpected(getErrorString());
        if (!written)
            return tl::unexpected(std::string("WriteFile wrote zero bytes"));
        offset += written;
    }
    return {};
}
#endif

std::string Manager::getBufferWithType(CTB type)
{
    std::string buffer;
    buffer.push_back(static_cast<uint8_t>(type));
    return buffer;
}

void Manager::writeUInt32(std::string &buffer, const uint32_t value)
{
    const auto ptr = reinterpret_cast<const char *>(&value);
    buffer.append(ptr, sizeof(value));
}

void Manager::writeString(std::string &buffer, const std::string_view &str)
{
    writeUInt32(buffer, str.size());
    buffer.append(str.begin(), str.end()); // Insert all characters
}

void Manager::writePath(std::string &buffer, const std::string_view &str)
{
    writeUInt32(buffer, str.size());
    buffer.append(str.begin(), str.end()); // Insert all characters
    buffer.push_back('\0');
}

void Manager::writeBMIFile(std::string &buffer, const BMIFile &file)
{
    writePath(buffer, file.filePath);
    writeUInt32(buffer, file.fileSize);
}

void Manager::writeModuleDep(std::string &buffer, const ModuleDep &dep)
{
    buffer.push_back(dep.isHeaderUnit);
    writeBMIFile(buffer, dep.file);
    buffer.push_back(dep.isSystem);
    writeVectorOfStrings(buffer, dep.logicalNames);
}

void Manager::writeHuDep(std::string &buffer, const HuDep &dep)
{
    writeBMIFile(buffer, dep.file);
    buffer.push_back(dep.isSystem);
    writeVectorOfStrings(buffer, dep.logicalNames);
}

void Manager::writeHeaderFile(std::string &buffer, const HeaderFile &dep)
{
    writeString(buffer, dep.logicalName);
    writePath(buffer, dep.filePath);
    buffer.push_back(dep.isSystem);
}

void Manager::writeVectorOfStrings(std::string &buffer, const std::vector<std::string_view> &strs)
{
    writeUInt32(buffer, strs.size());
    for (const std::string_view &str : strs)
    {
        writeString(buffer, str);
    }
}

void Manager::writeVectorOfModuleDep(std::string &buffer, const std::vector<ModuleDep> &deps)
{
    writeUInt32(buffer, deps.size());
    for (const ModuleDep &dep : deps)
    {
        writeModuleDep(buffer, dep);
    }
}

void Manager::writeVectorOfHuDeps(std::string &buffer, const std::vector<HuDep> &deps)
{
    writeUInt32(buffer, deps.size());
    for (const HuDep &dep : deps)
    {
        writeHuDep(buffer, dep);
    }
}

void Manager::writeVectorOfHeaderFiles(std::string &buffer, const std::vector<HeaderFile> &headerFiles)
{
    writeUInt32(buffer, headerFiles.size());
    for (const HeaderFile &headerFile : headerFiles)
    {
        writeHeaderFile(buffer, headerFile);
    }
}

tl::expected<bool, std::string> Manager::readBool(const std::string_view message, uint64_t &bytesRead)
{
    if (bytesRead >= message.size())
    {
        return tl::unexpected(getErrorString(ErrorCategory::PARSING_ERROR));
    }
    const unsigned char value = static_cast<unsigned char>(message[bytesRead]);
    if (value > 1)
        return tl::unexpected(getErrorString(ErrorCategory::PARSING_ERROR));
    bool result = value != 0;
    bytesRead += 1;
    return result;
}

tl::expected<uint8_t, std::string> Manager::readUInt8(const std::string_view message, uint64_t &bytesRead)
{
    if (bytesRead >= message.size())
    {
        return tl::unexpected(getErrorString(ErrorCategory::PARSING_ERROR));
    }
    uint8_t result = *reinterpret_cast<const uint8_t *>(message.data() + bytesRead);
    bytesRead += 1;
    return result;
}

tl::expected<uint32_t, std::string> Manager::readUInt32(const std::string_view message, uint64_t &bytesRead)
{
    if (bytesRead > message.size() || message.size() - bytesRead < sizeof(uint32_t))
    {
        return tl::unexpected(getErrorString(ErrorCategory::PARSING_ERROR));
    }
    uint32_t result;
    memcpy(&result, message.data() + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

tl::expected<std::string_view, std::string> Manager::readString(const std::string_view message, uint64_t &bytesRead)
{
    auto r = readUInt32(message, bytesRead);
    if (!r)
    {
        return tl::unexpected(r.error());
    }
    const uint64_t stringSize = *r;
    if (bytesRead > message.size() || stringSize > message.size() - bytesRead)
    {
        return tl::unexpected(getErrorString(ErrorCategory::PARSING_ERROR));
    }
    std::string_view result = {message.data() + bytesRead, stringSize};
    bytesRead += stringSize;
    return result;
}

tl::expected<std::string_view, std::string> Manager::readPath(const std::string_view message, uint64_t &bytesRead)
{
    auto r = readUInt32(message, bytesRead);
    if (!r)
    {
        return tl::unexpected(r.error());
    }
    const uint64_t stringSize = *r;
    if (bytesRead > message.size() || stringSize >= message.size() - bytesRead ||
        message[bytesRead + stringSize] != '\0')
    {
        return tl::unexpected(getErrorString(ErrorCategory::PARSING_ERROR));
    }
    std::string_view result = {message.data() + bytesRead, stringSize};
    bytesRead += stringSize;
    // This string is followed by \0
    bytesRead += 1;
    return result;
}

} // namespace P2978
