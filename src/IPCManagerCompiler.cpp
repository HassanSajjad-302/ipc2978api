
#include "IPCManagerCompiler.hpp"
#include "Manager.hpp"
#include "Messages.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>

#ifdef _WIN32
#include <Windows.h>
#else
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define TRY_READ(var, func, ...)                                                                                       \
    const auto &var = func(__VA_ARGS__);                                                                               \
    if (!var)                                                                                                          \
    {                                                                                                                  \
        return tl::unexpected(var.error());                                                                            \
    }

#define TRY_READ_VAL(var, func, ...)                                                                                   \
    const auto &var##_result = func(__VA_ARGS__);                                                                      \
    if (!var##_result)                                                                                                 \
    {                                                                                                                  \
        return tl::unexpected(var##_result.error());                                                                   \
    }                                                                                                                  \
    auto &var = *var##_result;

namespace P2978
{

Response::Response(std::string_view filePath_, const Mapping &mapping_, const FileType type_, const bool isSystem_)
    : filePath(std::move(filePath_)), mapping(mapping_), type(type_), isSystem(isSystem_)
{
}

static bool endsWith(const std::string_view str, const std::string &suffix)
{
    if (suffix.size() > str.size())
    {
        return false;
    }
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

tl::expected<std::string_view, std::string> IPCManagerCompiler::readInternal(char (&buffer)[4096]) const
{
    std::string *output = nullptr;
    while (true)
    {
        uint64_t bytesRead = 0;
#ifdef _WIN32
        DWORD readCount = 0;
        const bool success = ReadFile(GetStdHandle(STD_INPUT_HANDLE), // pipe handle
                                      buffer,                         // buffer to receive reply
                                      4096,                           // size of buffer
                                      &readCount,                     // number of bytes read
                                      nullptr);                       // not overlapped

        bytesRead = readCount;
        if (const DWORD lastError = GetLastError(); !success && lastError != ERROR_MORE_DATA)
        {
            return tl::unexpected(getErrorString());
        }

#else
        bytesRead = read(STDIN_FILENO, buffer, 4096);
        if (bytesRead == UINT64_MAX)
        {
            if (errno == EINTR)
                continue;
            return tl::unexpected(getErrorString());
        }

#endif
        if (!bytesRead)
        {
            return tl::unexpected(getErrorString(ErrorCategory::READ_FILE_ZERO_BYTES_READ));
        }

        if (!output)
        {
            output = new std::string{};
            allocations.emplace_back(output);
        }

        output->append(buffer, bytesRead);

        // We return once we receive the delimiter.
        if (endsWith(*output, delimiter))
        {
            return std::string_view{output->data(), output->size() - strlen(delimiter)};
        }
    }
}

tl::expected<void, std::string> IPCManagerCompiler::writeInternal(const std::string_view buffer) const
{
#ifdef _WIN32
    return writeAll(GetStdHandle(STD_OUTPUT_HANDLE), buffer);
#else
    if (const auto &r = writeAll(STDOUT_FILENO, buffer.data(), buffer.size()); !r)
    {
        return tl::unexpected(r.error());
    }
#endif
    return {};
}

tl::expected<IPCManagerCompiler::BMIFileMapping, std::string> IPCManagerCompiler::readProcessMappingOfBMIFile(
    const std::string_view message, uint64_t &bytesRead)
{
    const auto &r = readPath(message, bytesRead);
    if (!r)
    {
        return tl::unexpected(r.error());
    }
    const auto &r2 = readUInt32(message, bytesRead);
    if (!r2)
    {
        return tl::unexpected(r2.error());
    }

    BMIFile file;
    file.filePath = *r;
    file.fileSize = *r2;

    std::string path(file.filePath);
    if (const auto it = filePathProcessMapping.find(path); it != filePathProcessMapping.end())
    {
        if (file.fileSize != UINT32_MAX && file.fileSize != it->second.file.size())
            return tl::unexpected(std::string("Invalid BMI file size: ") + path);
        return BMIFileMapping{file, it->second};
    }

    if (const auto &r3 = readBMIFile(file); r3)
    {
        filePathProcessMapping.emplace(std::move(path), r3.value());
        BMIFileMapping bmiFileMapping;
        bmiFileMapping.file = file;
        bmiFileMapping.mapping = *r3;
        return bmiFileMapping;
    }
    else
    {
        return tl::unexpected(r3.error());
    }
}

tl::expected<void, std::string> IPCManagerCompiler::readLogicalNames(const std::string_view message,
                                                                     uint64_t &bytesRead, const BMIFileMapping &mapping,
                                                                     const FileType type, const bool isSystem)
{
    TRY_READ_VAL(logicalNamesSize, readUInt32, message, bytesRead);
    for (uint64_t i = 0; i < logicalNamesSize; ++i)
    {
        TRY_READ_VAL(logicalName, readString, message, bytesRead);
        responses.emplace(logicalName, Response(mapping.file.filePath, mapping.mapping, type, isSystem));
    }

    return {};
}

tl::expected<void, std::string> IPCManagerCompiler::receiveBTCModule(const CTBModule &moduleName)
{
    std::string buffer = getBufferWithType(CTB::MODULE);
    writeString(buffer, moduleName.moduleName);
    writeUInt32(buffer, buffer.size());
    buffer.append(delimiter, strlen(delimiter));
    // This call sends the CTBModule to the build-system.
    if (const auto &r = writeInternal(buffer); !r)
    {
        return tl::unexpected(r.error());
    }

    char stackBuffer[4096];
    auto received = readInternal(stackBuffer);

    if (!received)
    {
        return tl::unexpected(received.error());
    }
    const std::string_view message = *received;

    uint64_t bytesRead = 0;

    TRY_READ_VAL(requested, readProcessMappingOfBMIFile, message, bytesRead);
    TRY_READ_VAL(isSystem, readBool, message, bytesRead);

    std::string *str = new std::string(moduleName.moduleName);
    allocations.emplace_back(str);
    responses.emplace(*str, Response(requested.file.filePath, requested.mapping, FileType::MODULE, isSystem));

    TRY_READ_VAL(modDepsSize, readUInt32, message, bytesRead);

    for (uint64_t i = 0; i < modDepsSize; ++i)
    {
        TRY_READ_VAL(isHeaderUnit, readBool, message, bytesRead);
        TRY_READ_VAL(modDepFile, readProcessMappingOfBMIFile, message, bytesRead);
        TRY_READ_VAL(modDepIsSytem, readBool, message, bytesRead);
        if (isHeaderUnit)
        {
            if (const auto &r = readLogicalNames(message, bytesRead, modDepFile, FileType::HEADER_UNIT, modDepIsSytem);
                !r)
            {
                return tl::unexpected(r.error());
            }
        }
        else
        {
            if (const auto &r = readLogicalNames(message, bytesRead, modDepFile, FileType::MODULE, modDepIsSytem); !r)
            {
                return tl::unexpected(r.error());
            }
        }
    }

    if (message.size() != bytesRead)
    {
        return tl::unexpected(getErrorString(ErrorCategory::PARSING_ERROR));
    }
    return {};
}

tl::expected<void, std::string> IPCManagerCompiler::receiveBTCNonModule(const CTBNonModule &nonModule)
{
    std::string buffer = getBufferWithType(CTB::NON_MODULE);
    buffer.push_back(nonModule.isHeaderUnit);
    writeString(buffer, nonModule.logicalName);
    writeUInt32(buffer, buffer.size());
    buffer.append(delimiter, strlen(delimiter));
    // This call sends the CTBNonModule to the build-system.
    if (const auto &r = writeInternal(buffer); !r)
    {
        return tl::unexpected(r.error());
    }

    char stackBuffer[4096];
    auto received = readInternal(stackBuffer);

    if (!received)
    {
        return tl::unexpected(received.error());
    }

    std::string_view readCompilerMessage = *received;
    uint64_t bytesRead = 0;

    TRY_READ_VAL(isHeaderUnit, readBool, readCompilerMessage, bytesRead);
    TRY_READ_VAL(isSystem, readBool, readCompilerMessage, bytesRead);
    TRY_READ_VAL(headerFilesSize, readUInt32, readCompilerMessage, bytesRead);

    for (uint64_t i = 0; i < headerFilesSize; ++i)
    {
        TRY_READ_VAL(logicalName, readString, readCompilerMessage, bytesRead);
        TRY_READ_VAL(filePath, readPath, readCompilerMessage, bytesRead);
        TRY_READ_VAL(isSystemHeaderFile, readBool, readCompilerMessage, bytesRead);

        responses.emplace(logicalName, Response{filePath, {}, FileType::HEADER_FILE, isSystemHeaderFile});
    }

    std::string *str = new std::string(nonModule.logicalName);
    allocations.emplace_back(str);
    if (!isHeaderUnit)
    {
        TRY_READ_VAL(filePath, readPath, readCompilerMessage, bytesRead);
        responses.emplace(*str, Response{filePath, {}, FileType::HEADER_FILE, isSystem});
        if (readCompilerMessage.size() != bytesRead)
        {
            return tl::unexpected(getErrorString(ErrorCategory::PARSING_ERROR));
        }
        return {};
    }

    TRY_READ_VAL(file, readProcessMappingOfBMIFile, readCompilerMessage, bytesRead);
    responses.emplace(*str, Response{file.file.filePath, file.mapping, FileType::HEADER_UNIT, isSystem});

    TRY_READ(logicalNames, readLogicalNames, readCompilerMessage, bytesRead, file, FileType::HEADER_UNIT, isSystem);

    TRY_READ_VAL(huDepsSize, readUInt32, readCompilerMessage, bytesRead);
    for (uint64_t i = 0; i < huDepsSize; ++i)
    {
        TRY_READ_VAL(huDepFile, readProcessMappingOfBMIFile, readCompilerMessage, bytesRead);
        TRY_READ_VAL(huDepIsSystem, readBool, readCompilerMessage, bytesRead);
        TRY_READ(huDeplogicalNames, readLogicalNames, readCompilerMessage, bytesRead, huDepFile, FileType::HEADER_UNIT,
                 huDepIsSystem);
    }

    if (readCompilerMessage.size() != bytesRead)
    {
        return tl::unexpected(getErrorString(ErrorCategory::PARSING_ERROR));
    }
    return {};
}

tl::expected<Response, std::string> IPCManagerCompiler::findResponse(const std::string_view logicalName,
                                                                     const FileType type)
{
#ifdef _WIN32
    std::string logicalName2{logicalName};
    if (type != FileType::MODULE)
    {
        for (char &c : logicalName2)
        {
            c = std::tolower(c);
        }
    }
#else
    std::string logicalName2{logicalName};
#endif

    if (const auto &it = responses.find(logicalName2);
        // This requests from the build-system if we don't have an entry for the logicalName or if there is a type
        // mismatch between the request and the response. Only allowed mismatch is if the request is of header-file and
        // the response is a header-unit instead. For other mismatches compiler will request the build-system which will
        // give not found error. HMake at config-time checks for the logicalName collision and also that a file is not
        // registered as 2 of header-file, header-unit and module.
        it == responses.end() ||
        (it->second.type != type && (it->second.type != FileType::HEADER_UNIT || type != FileType::HEADER_FILE)))
    {
        if (isMocking)
        {
            return tl::unexpected("Could not find entry in mocking-mode");
        }

        if (type == FileType::MODULE)
        {
            CTBModule ctbModule;
            ctbModule.moduleName = logicalName2;
            if (const auto &r2 = receiveBTCModule(ctbModule); !r2)
            {
                return tl::unexpected(r2.error());
            }
        }
        else
        {
            CTBNonModule ctbNonModule;
            ctbNonModule.logicalName = logicalName2;
            ctbNonModule.isHeaderUnit = type == FileType::HEADER_UNIT;
            if (const auto &r2 = receiveBTCNonModule(ctbNonModule); !r2)
            {
                return tl::unexpected(r2.error());
            }
        }

        return responses.at(logicalName2);
    }
    else
    {
        return it->second;
    }
}

static tl::expected<std::string, std::string> fileToString(const std::string_view fileName)
{
    std::ifstream file(std::string(fileName), std::ios::binary);
    if (!file)
        return tl::unexpected(std::string("Could not open IPC mock file: ") + std::string(fileName));
    std::string fileBuffer{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad())
        return tl::unexpected(std::string("Could not read IPC mock file: ") + std::string(fileName));
    return fileBuffer;
}

tl::expected<void, std::string> IPCManagerCompiler::readEntriesFromFile(const std::string_view filePath)
{
    auto contents = fileToString(filePath);
    if (!contents)
        return tl::unexpected(contents.error());
    scanCacheFileData = std::move(*contents);

    uint64_t bytesRead = 0;
    TRY_READ_VAL(entriesSize, readUInt32, scanCacheFileData, bytesRead);
    for (uint64_t i = 0; i < entriesSize; ++i)
    {
        TRY_READ_VAL(responseKey, readString, scanCacheFileData, bytesRead);

        TRY_READ_VAL(valueFilePath, readPath, scanCacheFileData, bytesRead);
        TRY_READ_VAL(fileType, readUInt8, scanCacheFileData, bytesRead);
        if (fileType > static_cast<uint8_t>(FileType::HEADER_FILE))
            return tl::unexpected(getErrorString(ErrorCategory::PARSING_ERROR));
        TRY_READ_VAL(isSystem, readBool, scanCacheFileData, bytesRead);

        Mapping mapping{};
        if (auto it = filePathProcessMapping.find(std::string(valueFilePath)); it == filePathProcessMapping.end())
        {
            if (static_cast<FileType>(fileType) != FileType::HEADER_FILE)
            {
                TRY_READ_VAL(fileMapping, readBMIFile, (BMIFile{valueFilePath}));
                mapping = fileMapping;
                filePathProcessMapping.emplace(valueFilePath, mapping);
            }
        }
        else
        {
            mapping = it->second;
        }
        responses.emplace(responseKey, Response{valueFilePath, mapping, static_cast<FileType>(fileType), isSystem});
    }

    if (bytesRead != scanCacheFileData.size())
        return tl::unexpected(getErrorString(ErrorCategory::PARSING_ERROR));

    mockFilePath = filePath;
    isMocking = true;
    return {};
}

tl::expected<Mapping, std::string> IPCManagerCompiler::readBMIFile(const BMIFile &file)
{
    // Own a terminated path; callers may supply arbitrary string_views.
    const std::string path(file.filePath);
#ifdef _WIN32
    const HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return tl::unexpected(getErrorString());

    LARGE_INTEGER size;
    if (!GetFileSizeEx(handle, &size))
    {
        const std::string error = getErrorString();
        CloseHandle(handle);
        return tl::unexpected(error);
    }
    if (size.QuadPart <= 0 || static_cast<uint64_t>(size.QuadPart) > (std::numeric_limits<size_t>::max)() ||
        (file.fileSize != UINT32_MAX && static_cast<uint64_t>(size.QuadPart) != file.fileSize))
    {
        CloseHandle(handle);
        return tl::unexpected(std::string("Invalid BMI file size: ") + path);
    }

    // No name or build-system-owned object is needed. All readers open the completed file.
    const HANDLE mapping = CreateFileMappingA(handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping)
    {
        const std::string error = getErrorString();
        CloseHandle(handle);
        return tl::unexpected(error);
    }
    CloseHandle(handle);

    const void *view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!view)
    {
        const std::string error = getErrorString();
        CloseHandle(mapping);
        return tl::unexpected(error);
    }
    std::shared_ptr<const void> owner(view, [mapping](const void *address) {
        UnmapViewOfFile(address);
        CloseHandle(mapping);
    });
    return Mapping{{static_cast<const char *>(view), static_cast<size_t>(size.QuadPart)}, std::move(owner)};
#else
    const int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1)
        return tl::unexpected(getErrorString());

    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        const std::string error = getErrorString();
        close(fd);
        return tl::unexpected(error);
    }
    if (st.st_size <= 0 || static_cast<uint64_t>(st.st_size) > (std::numeric_limits<size_t>::max)() ||
        (file.fileSize != UINT32_MAX && static_cast<uint64_t>(st.st_size) != file.fileSize))
    {
        close(fd);
        return tl::unexpected(std::string("Invalid BMI file size: ") + path);
    }
    const size_t size = static_cast<size_t>(st.st_size);
    int flags = MAP_SHARED;
#ifdef MAP_POPULATE
    flags |= MAP_POPULATE;
#endif
    void *view = mmap(nullptr, size, PROT_READ, flags, fd, 0);
    if (view == MAP_FAILED)
    {
        const std::string error = getErrorString();
        close(fd);
        return tl::unexpected(error);
    }
    close(fd);
    std::shared_ptr<const void> owner(view, [size](const void *address) { munmap(const_cast<void *>(address), size); });
    return Mapping{{static_cast<const char *>(view), size}, std::move(owner)};
#endif
}

bool operator==(const CTBNonModule &lhs, const CTBNonModule &rhs)
{
    return lhs.isHeaderUnit == rhs.isHeaderUnit && lhs.logicalName == rhs.logicalName;
}
} // namespace P2978
