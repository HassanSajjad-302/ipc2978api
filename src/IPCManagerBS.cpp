#include "IPCManagerBS.hpp"
#include "Manager.hpp"
#include "Messages.hpp"
#include "expected.hpp"
#include <cstring>
#include <string>

#ifdef _WIN32
#include <Windows.h>
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

tl::expected<void, std::string> IPCManagerBS::writeInternal(const std::string_view buffer) const
{
#ifdef _WIN32
    return writeAll(reinterpret_cast<HANDLE>(writeFd), buffer);
#else
    if (const auto &r = writeAll(writeFd, buffer.data(), buffer.size()); !r)
    {
        return tl::unexpected(r.error());
    }
#endif
    return {};
}

IPCManagerBS::IPCManagerBS(const uint64_t writeFd_) : writeFd(writeFd_)
{
}

tl::expected<void, std::string> IPCManagerBS::receiveMessage(char (&ctbBuffer)[320], CTB &messageType,
                                                             const std::string_view serverReadString)
{
    if (serverReadString.empty())
    {
        return tl::unexpected(getErrorString(ErrorCategory::PARSING_ERROR));
    }

    uint64_t bytesRead = 1;

    // read call fails if zero byte is read, so safe to process 1 byte
    switch (static_cast<CTB>(serverReadString[0]))
    {

    case CTB::MODULE: {
        TRY_READ_VAL(r, readString, serverReadString, bytesRead);

        messageType = CTB::MODULE;
        getInitializedObjectFromBuffer<CTBModule>(ctbBuffer).moduleName = r;
    }
    break;

    case CTB::NON_MODULE: {
        TRY_READ_VAL(r, readBool, serverReadString, bytesRead);
        TRY_READ_VAL(r2, readString, serverReadString, bytesRead);
        messageType = CTB::NON_MODULE;
        auto &[isHeaderUnit, str] = getInitializedObjectFromBuffer<CTBNonModule>(ctbBuffer);
        isHeaderUnit = r;
        str = r2;
    }
    break;

    default:
        return tl::unexpected(getErrorString(ErrorCategory::UNKNOWN_CTB_TYPE));
    }

    if (serverReadString.size() != bytesRead)
    {
        return tl::unexpected(getErrorString(serverReadString.size(), bytesRead));
    }

    return {};
}

tl::expected<void, std::string> IPCManagerBS::sendMessage(const BTCModule &moduleFile) const
{
    std::string buffer;
    writeBMIFile(buffer, moduleFile.requested);
    buffer.push_back(moduleFile.isSystem);
    writeVectorOfModuleDep(buffer, moduleFile.modDeps);
    buffer.append(delimiter, strlen(delimiter));
    if (const auto &r = writeInternal(buffer); !r)
    {
        return tl::unexpected(r.error());
    }
    return {};
}

tl::expected<void, std::string> IPCManagerBS::sendMessage(const BTCNonModule &nonModule) const
{
    std::string buffer;
    buffer.push_back(nonModule.isHeaderUnit);
    buffer.push_back(nonModule.isSystem);
    writeVectorOfHeaderFiles(buffer, nonModule.headerFiles);
    writePath(buffer, nonModule.filePath);
    if (nonModule.isHeaderUnit)
    {
        writeUInt32(buffer, nonModule.fileSize);
        writeVectorOfStrings(buffer, nonModule.logicalNames);
        writeVectorOfHuDeps(buffer, nonModule.huDeps);
    }
    buffer.append(delimiter, strlen(delimiter));
    if (const auto &r = writeInternal(buffer); !r)
    {
        return tl::unexpected(r.error());
    }
    return {};
}

} // namespace P2978
