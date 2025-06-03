
#include "IPCManagerCompiler.hpp"
#include "Manager.hpp"
#include "Messages.hpp"

#include "fmt/printf.h"
#include <string>
#include <strsafe.h>
#include <tchar.h>
#include <windows.h>

using std::string, fmt::print;

namespace N2978
{

void IPCManagerCompiler::receiveBTCLastMessage() const
{
    char buffer[BUFFERSIZE];
    uint32_t bytesRead;
    read(buffer, bytesRead);

    uint32_t bytesProcessed = 0;
    bytesProcessed = 1;
    if (buffer[0] != false)
    {
        print("Incorrect Last Message Received\n");
        if (bytesRead != bytesProcessed)
        {
            print("BytesRead {} not equal to BytesProcessed {} in receiveMessage.\n", bytesRead, bytesProcessed);
        }
    }
}

void IPCManagerCompiler::checkBytesReadEqualBytesProcessed(uint32_t bytesRead, uint32_t bytesProcessed)
{
    if (bytesRead != bytesProcessed)
    {
        print("BytesRead {} not equal to BytesProcessed {} in receiveMessage.\n", bytesRead, bytesProcessed);
    }
}

IPCManagerCompiler::IPCManagerCompiler(const string &objFilePath) : pipeName(R"(\\.\pipe\)" + objFilePath)
{
}

void IPCManagerCompiler::connectToBuildSystem()
{
    if (connectedToBuildSystem)
    {
        return;
    }

    hPipe = CreateFile(pipeName.data(), // pipe name
                       GENERIC_READ |   // read and write access
                           GENERIC_WRITE,
                       0,             // no sharing
                       nullptr,       // default security attributes
                       OPEN_EXISTING, // opens existing pipe
                       0,             // default attributes
                       nullptr);      // no template file

    // Break if the pipe handle is valid.

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        print("Could not open pipe {} to build system\n{}\n", pipeName, GetLastError());
    }

    connectedToBuildSystem = true;
}

BTCModule IPCManagerCompiler::receiveBTCModule(const CTBModule &moduleName)
{
    connectToBuildSystem();
    vector<char> buffer = getBufferWithType(CTB::MODULE);
    writeString(buffer, moduleName.moduleName);
    write(buffer);
    return receiveMessage<BTCModule>();
}

BTCNonModule IPCManagerCompiler::receiveBTCNonModule(const CTBNonModule &nonModule)
{
    connectToBuildSystem();
    vector<char> buffer = getBufferWithType(CTB::NON_MODULE);
    buffer.emplace_back(nonModule.isHeaderUnit);
    writeString(buffer, nonModule.str);
    write(buffer);
    return receiveMessage<BTCNonModule>();
}

void IPCManagerCompiler::sendCTBLastMessage(const CTBLastMessage &lastMessage)
{
    connectToBuildSystem();
    vector<char> buffer = getBufferWithType(CTB::LAST_MESSAGE);
    buffer.emplace_back(lastMessage.exitStatus);
    writeVectorOfStrings(buffer, lastMessage.headerFiles);
    writeString(buffer, lastMessage.output);
    writeString(buffer, lastMessage.errorOutput);
    writeString(buffer, lastMessage.logicalName);
    write(buffer);
}

void IPCManagerCompiler::sendCTBLastMessage(const CTBLastMessage &lastMessage, const string &bmiFile,
                                            const string &filePath)
{
    const HANDLE hFile = CreateFile(filePath.c_str(), GENERIC_READ | GENERIC_WRITE,
                                    0, // no sharing during setup
                                    nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        print("Could not create file {}.\n{}\n", filePath, GetLastError());
    }

    LARGE_INTEGER fileSize;
    fileSize.QuadPart = bmiFile.size();
    // 3) Create a RW mapping of that file:
    const HANDLE hMap =
        CreateFileMapping(hFile, nullptr, PAGE_READWRITE, fileSize.HighPart, fileSize.LowPart, filePath.c_str());
    if (!hMap)
    {
        CloseHandle(hFile);
        print("Could not create file mapping of the file {}.\n{}\n", filePath, GetLastError());
    }

    void *pView = MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, bmiFile.size());
    if (!pView)
    {
        CloseHandle(hFile);
        CloseHandle(hMap);
        print("Could not map view for the file mapping of the file {}.\n{}\n", filePath, GetLastError());
    }

    memcpy(pView, bmiFile.c_str(), bmiFile.size());

    if (!FlushViewOfFile(pView, bmiFile.size()))
    {
        UnmapViewOfFile(pView);
        CloseHandle(hFile);
        CloseHandle(hMap);
        print("Could not flush the file mapping of file {}.\n{}\n", filePath, GetLastError());
    }

    UnmapViewOfFile(pView);

    CloseHandle(hFile);

    sendCTBLastMessage(lastMessage);
    if (lastMessage.exitStatus == EXIT_SUCCESS)
    {
        receiveBTCLastMessage();
    }

    CloseHandle(hMap);
}

string_view IPCManagerCompiler::readSharedMemoryBMIFile(const BMIFile &file)
{
    // 1) Open the existing file‐mapping object (must have been created by another process)
    const HANDLE mapping = OpenFileMapping(FILE_MAP_READ,       // read‐only access
                                           FALSE,               // do not inherit handle
                                           file.filePath.data() // name of mapping
    );

    if (mapping == nullptr)
    {
        print("Could not open file mapping of file {}.\n{}\n", file.filePath, GetLastError());
    }

    // 2) Map a view of the file into our address space
    const LPVOID view = MapViewOfFile(mapping,       // handle to mapping object
                                      FILE_MAP_READ, // read‐only view
                                      0,             // file offset high
                                      0,             // file offset low
                                      file.fileSize  // number of bytes to map (0 maps the whole file)
    );

    if (view == nullptr)
    {
        print("Could not open view of file mapping of file {}.\n", file.filePath);
        CloseHandle(mapping);
    }

    MemoryMappedBMIFile f;
    f.mapping = mapping;
    f.view = view;
    memoryMappedBMIFiles.emplace_back(std::move(f));
    return {static_cast<char *>(view), file.fileSize};
}
} // namespace N2978