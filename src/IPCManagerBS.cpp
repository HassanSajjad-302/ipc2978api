#include "IPCManagerBS.hpp"
#include "Manager.hpp"
#include "Messages.hpp"
#include "expected.hpp"
#include <string>
#include <sys/stat.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include "rapidhash.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif
using std::string;

namespace N2978
{

tl::expected<IPCManagerBS, string> makeIPCManagerBS(const string &BMIIfHeaderUnitObjOtherwisePath)
{
#ifdef _WIN32
    objOrCompilerFilePath = R"(\\.\pipe\)" + objOrCompilerFilePath;
    void *hPipe = CreateNamedPipeA(objOrCompilerFilePath.c_str(),     // pipe name
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
        return tl::unexpected(getErrorString());
    }
    return IPCManagerBS(hPipe);

#else

    // Named Pipes are used but Unix Domain sockets could have been used as well. The tradeoff is that a file is created
    // and there needs to be bind, listen, accept calls which means that an extra fd is created is temporarily on the
    // server side. it can be closed immediately after.

    const int fdSocket = socket(AF_UNIX, SOCK_STREAM, 0);

    // Create server socket
    if (fdSocket == -1)
    {
        return tl::unexpected(getErrorString());
    }

    // Prepare address structure
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    // We use file hash to make a file path smaller, since there is a limit of NAME_MAX that is generally 108 bytes.
    // TODO
    // Have an option to receive this path in constructor to make it compatible with Android and IOS.
    string prependDir = "/home/hassan/";
    const uint64_t hash = rapidhash(BMIIfHeaderUnitObjOtherwisePath.c_str(), BMIIfHeaderUnitObjOtherwisePath.size());
    prependDir.append(toString(hash));
    std::copy(prependDir.begin(), prependDir.end(), addr.sun_path);

    // Remove any existing socket
    unlink(prependDir.c_str());

    // Bind socket to the file system path
    if (bind(fdSocket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1)
    {
        return tl::unexpected(getErrorString());
    }
    if (chmod(prependDir.c_str(), 0666) == -1)
    {
        return tl::unexpected(getErrorString());
    }

    // Listen for incoming connections
    if (listen(fdSocket, 1) == -1)
    {
        close(fdSocket);
        return tl::unexpected(getErrorString());
    }

    return IPCManagerBS(fdSocket);
#endif
}

#ifdef _WIN32
IPCManagerBS::IPCManagerBS(void *hPipe_)
{
    hPipe = hPipe_;
}
#else
IPCManagerBS::IPCManagerBS(const int fdSocket_)
{
    fdSocket = fdSocket_;
}
#endif

tl::expected<void, string> IPCManagerBS::receiveMessage(char (&ctbBuffer)[320], CTB &messageType) const
{
    if (!connectedToCompiler)
    {
#ifdef _WIN32
        if (!ConnectNamedPipe(hPipe, nullptr))
        {
            // Is the client already connected?
            if (GetLastError() != ERROR_PIPE_CONNECTED)
            {
                return tl::unexpected(getErrorString());
            }
        }
#else
        const int fd = accept(fdSocket, nullptr, nullptr);
        if (fd == -1)
        {
            close(fdSocket);
            return tl::unexpected(getErrorString());
        }
        const_cast<int &>(fdSocket) = fd;
#endif
        const_cast<bool &>(connectedToCompiler) = true;
    }

    // Read from the pipe.
    char buffer[BUFFERSIZE];
    uint32_t bytesRead;
    if (const auto &r = readInternal(buffer); !r)
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
    if (const auto &r = writeInternal(buffer); !r)
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
    if (const auto &r = writeInternal(buffer); !r)
    {
        return tl::unexpected(r.error());
    }
    return {};
}

tl::expected<void, string> IPCManagerBS::sendMessage(const BTCLastMessage &) const
{
    vector<char> buffer;
    buffer.emplace_back(false);
    if (const auto &r = writeInternal(buffer); !r)
    {
        return tl::unexpected(r.error());
    }
    return {};
}

tl::expected<MemoryMappedBMIFile, string> IPCManagerBS::createSharedMemoryBMIFile(const BMIFile &bmiFile)
{
    MemoryMappedBMIFile sharedFile;
#ifdef _WIN32
    // 1) Open the existing file‐mapping object (must have been created by another process)
    sharedFile.mapping = OpenFileMappingA(FILE_MAP_READ,             // read‐only access
                                          FALSE,                     // do not inherit handle
                                          sharedFile.filePath.data() // name of mapping
    );

    if (sharedFile.mapping == nullptr)
    {
        return tl::unexpected(getErrorString());
    }

    return sharedFile;
#else
    const int fd = open(bmiFile.filePath.data(), O_RDONLY);
    if (fd == -1)
    {
        return tl::unexpected(getErrorString());
    }
    sharedFile.mapping = mmap(nullptr, bmiFile.fileSize, PROT_READ, MAP_SHARED | MAP_POPULATE, fd, 0);
    if (close(fd) == -1)
    {
        return tl::unexpected(getErrorString());
    }
    if (sharedFile.mapping == MAP_FAILED)
    {
        return tl::unexpected(getErrorString());
    }
    sharedFile.mappingSize = bmiFile.fileSize;
    return sharedFile;
#endif
}

tl::expected<void, string> IPCManagerBS::deleteBMIFileMapping(const MemoryMappedBMIFile &memoryMappedBMIFile)
{
#ifdef _WIN32
    CloseHandle(memoryMappedBMIFile.mapping);
#else
    if (munmap(memoryMappedBMIFile.mapping, memoryMappedBMIFile.mappingSize) == -1)
    {
        return tl::unexpected(getErrorString());
    }
#endif
    return {};
}
} // namespace N2978