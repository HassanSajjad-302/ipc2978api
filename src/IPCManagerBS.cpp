#include "IPCManagerBS.hpp"
#include "Manager.hpp"
#include "Messages.hpp"
#include "expected.hpp"

#include <string>
#include <Windows.h>

using std::string;

namespace N2978
{

tl::expected<IPCManagerBS, string> makeIPCManagerBS(string objOrCompilerFilePath)
{
    objOrCompilerFilePath = R"(\\.\pipe\)" + objOrCompilerFilePath;
    void *hPipe = CreateNamedPipeA(objOrCompilerFilePath.c_str(),               // pipe name
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
        return tl::unexpected(string{});
    }
    return IPCManagerBS(hPipe);
}

IPCManagerBS::IPCManagerBS(void *hPipe_)
{
    hPipe = hPipe_;
}

tl::expected<void, string> IPCManagerBS::connectToCompiler() const
{
    if (connectedToCompiler)
    {
        return {};
    }

    if (!ConnectNamedPipe(hPipe, nullptr))
    {
        switch (GetLastError())
        {
            // Client is already connected, so signal an event.
        case ERROR_PIPE_CONNECTED:
            break;

        // If an error occurs during the connect operation...
        default:
            return tl::unexpected(string{});
        }
    }
    const_cast<bool &>(connectedToCompiler) = true;
    return {};
}

tl::expected<void, string> IPCManagerBS::receiveMessage(char (&ctbBuffer)[320], CTB &messageType) const
{
    if (const auto &r = connectToCompiler(); !r)
    {
        IPCErr(r.error())
    }

    // Read from the pipe.
    char buffer[BUFFERSIZE];
    uint32_t bytesRead;
    if (const auto &r = read(buffer); !r)
    {
        IPCErr(r.error())
    }
    else
    {
        bytesRead = *r;
    }

    uint32_t bytesProcessed = 1;

    // read call fails if zero byte is read, so safe to process 1 byte
    switch (static_cast<CTB>(buffer[0]))
    {

    case CTB::MODULE: {

        const auto &r = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (!r)
        {
            IPCErr(r.error())
        }

        messageType = CTB::MODULE;
        getInitializedObjectFromBuffer<CTBModule>(ctbBuffer).moduleName = *r;
    }

    break;

    case CTB::NON_MODULE: {

        const auto &r = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        if (!r)
        {
            return tl::unexpected(r.error());
        }

        const auto &r2 = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (!r2)
        {
            return tl::unexpected(r.error());
        }

        messageType = CTB::NON_MODULE;
        auto &[isHeaderUnit, str] = getInitializedObjectFromBuffer<CTBNonModule>(ctbBuffer);
        isHeaderUnit = *r;
        str = *r2;
    }

    break;

    case CTB::LAST_MESSAGE: {

        const auto &exitStatusExpected = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        if (!exitStatusExpected)
        {
            IPCErr(exitStatusExpected.error())
        }

        const auto &headerFilesExpected = readVectorOfStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (!headerFilesExpected)
        {
            IPCErr(headerFilesExpected.error())
        }

        const auto &outputExpected = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (!outputExpected)
        {
            IPCErr(outputExpected.error())
        }

        const auto &errorOutputExpected = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (!errorOutputExpected)
        {
            IPCErr(errorOutputExpected.error())
        }

        const auto &logicalNameExpected = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (!logicalNameExpected)
        {
            IPCErr(logicalNameExpected.error())
        }

        const auto &fileSizeExpected = readUInt32FromPipe(buffer, bytesRead, bytesProcessed);
        if (!fileSizeExpected)
        {
            IPCErr(fileSizeExpected.error())
        }

        messageType = CTB::LAST_MESSAGE;

        auto &[exitStatus, headerFiles, output, errorOutput, logicalName, fileSize] =
            getInitializedObjectFromBuffer<CTBLastMessage>(ctbBuffer);

        exitStatus = *exitStatusExpected;
        headerFiles = *headerFilesExpected;
        output = *outputExpected;
        errorOutput = *errorOutputExpected;
        logicalName = *logicalNameExpected;
        fileSize = *fileSizeExpected;
    }
    break;

    default:

        IPCErr(ErrorCategory::UNKNOWN_CTB_TYPE)
    }

    if (bytesRead != bytesProcessed)
    {
        IPCErr(bytesRead, bytesProcessed)
    }

    return {};
}

tl::expected<void, string> IPCManagerBS::sendMessage(const BTCModule &moduleFile) const
{
    vector<char> buffer;
    writeMemoryMappedBMIFile(buffer, moduleFile.requested);
    writeVectorOfModuleDep(buffer, moduleFile.deps);
    if (const auto &r = write(buffer); !r)
    {
        return tl::unexpected(r.error());
    }
    return {};
}

tl::expected<void, string> IPCManagerBS::sendMessage(const BTCNonModule &nonModule) const
{
    vector<char> buffer;
    buffer.emplace_back(nonModule.isHeaderUnit);
    writeString(buffer, nonModule.filePath);
    buffer.emplace_back(nonModule.angled);
    writeUInt32(buffer, nonModule.fileSize);
    writeVectorOfHuDep(buffer, nonModule.deps);
    if (const auto &r = write(buffer); !r)
    {
        return tl::unexpected(r.error());
    }
    return {};
}

tl::expected<void, string> IPCManagerBS::sendMessage(const BTCLastMessage &) const
{
    vector<char> buffer;
    buffer.emplace_back(false);
    if (const auto &r = write(buffer); !r)
    {
        return tl::unexpected(r.error());
    }
    return {};
}

tl::expected<void *, string> IPCManagerBS::createSharedMemoryBMIFile(const string &bmiFilePath)
{
    // 1) Open the existing file‐mapping object (must have been created by another process)
    const HANDLE mapping = OpenFileMappingA(FILE_MAP_READ,     // read‐only access
                                           FALSE,             // do not inherit handle
                                           bmiFilePath.data() // name of mapping
    );

    if (mapping == nullptr)
    {
        return tl::unexpected(string{});
    }

    return mapping;
}
} // namespace N2978