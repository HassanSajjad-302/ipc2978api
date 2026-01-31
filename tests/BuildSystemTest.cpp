#include "IPCManagerBS.hpp"
#include "IPCManagerCompiler.hpp"
#include "Testing.hpp"
#include "fmt/printf.h"
#include <chrono>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#else
#include "sys/epoll.h"
#include "sys/wait.h"
#include "wordexp.h"
#include <unistd.h>
#endif

using fmt::print, std::string_view;

struct RunCommand
{
    string output;
    uint64_t pid;
    uint64_t readPipe;
    uint64_t writePipe;
    int exitStatus;
    RunCommand() = default;
    uint64_t startAsyncProcess(const char *command, uint64_t serverFd);
    void reapProcess() const;
};

#ifdef _WIN32

// Copied partially from Ninja
uint64_t RunCommand::startAsyncProcess(const char *command, uint64_t serverFd)
{
    // One BTarget can launch multiple processes so we also append the clock::now().
    const string read_pipe_name = R"(\\.\pipe\read{}{})";
    const string write_pipe_name = R"(\\.\pipe\write{}{})";

    // ===== CREATE READ PIPE (for reading child's stdout/stderr - ASYNC) =====
    HANDLE readPipe_ = CreateNamedPipeA(read_pipe_name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                                        PIPE_TYPE_BYTE, PIPE_UNLIMITED_INSTANCES, 0, 0, INFINITE, NULL);
    if (readPipe_ == INVALID_HANDLE_VALUE)
    {
        exitFailure(getErrorString());
    }

    // Register read pipe with IOCP for async operations
    if (!CreateIoCompletionPort(readPipe_, (HANDLE)serverFd, (ULONG_PTR)readPipe_, 0))
    {
        exitFailure(getErrorString());
    }

    OVERLAPPED readOverlappedIO = {};
    if (!ConnectNamedPipe(readPipe_, &readOverlappedIO) && GetLastError() != ERROR_IO_PENDING)
    {
        exitFailure(getErrorString());
    }

    // Get the write end for child's stdout/stderr
    HANDLE outputWriteHandle = CreateFileA(read_pipe_name.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (outputWriteHandle == INVALID_HANDLE_VALUE)
    {
        exitFailure(getErrorString());
    }

    {
        char buffer[4096];
        DWORD bytesRead = 0;
        OVERLAPPED overlapped = {0};

        // Wait for the read to complete.
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED completedOverlapped = nullptr;
        if (!GetQueuedCompletionStatus((HANDLE)serverFd, &bytesRead, &completionKey, &completedOverlapped, INFINITE))
        {
            exitFailure(getErrorString());
        }
    }

    HANDLE childStdoutPipe;
    if (!DuplicateHandle(GetCurrentProcess(), outputWriteHandle, GetCurrentProcess(), &childStdoutPipe, 0, TRUE,
                         DUPLICATE_SAME_ACCESS))
    {
        exitFailure(getErrorString());
    }
    CloseHandle(outputWriteHandle);

    // ===== CREATE WRITE PIPE (for writing to child's stdin - SYNC) =====
    // Note: No FILE_FLAG_OVERLAPPED - this makes it synchronous
    HANDLE writePipe_ = CreateNamedPipeA(write_pipe_name.c_str(),
                                         PIPE_ACCESS_OUTBOUND, // No FILE_FLAG_OVERLAPPED
                                         PIPE_TYPE_BYTE, PIPE_UNLIMITED_INSTANCES, 0, 0, INFINITE, NULL);
    if (writePipe_ == INVALID_HANDLE_VALUE)
    {
        exitFailure(getErrorString());
    }

    // Get the read end for child's stdin
    HANDLE inputReadHandle = CreateFileA(write_pipe_name.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (inputReadHandle == INVALID_HANDLE_VALUE)
    {
        exitFailure(getErrorString());
    }

    HANDLE childStdinPipe;
    if (!DuplicateHandle(GetCurrentProcess(), inputReadHandle, GetCurrentProcess(), &childStdinPipe, 0, TRUE,
                         DUPLICATE_SAME_ACCESS))
    {
        exitFailure(getErrorString());
    }
    CloseHandle(inputReadHandle);

    STARTUPINFOA startup_info;
    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb = sizeof(STARTUPINFO);

    bool use_console_ = false;
    if (!use_console_)
    {
        startup_info.dwFlags = STARTF_USESTDHANDLES;
        startup_info.hStdInput = childStdinPipe;   // Child reads from this
        startup_info.hStdOutput = childStdoutPipe; // Child writes to this
        startup_info.hStdError = childStdoutPipe;  // Child writes to this
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    // Ninja handles ctrl-c, except for subprocesses in console pools.
    DWORD process_flags = use_console_ ? 0 : CREATE_NEW_PROCESS_GROUP;

    // Do not prepend 'cmd /c' on Windows, this breaks command
    // lines greater than 8,191 chars.
    if (!CreateProcessA(NULL, (char *)command, NULL, NULL,
                        /* inherit handles */ TRUE, process_flags, NULL, NULL, &startup_info, &process_info))
    {
        exitFailure(getErrorString());
    }

    // Close pipe channels only used by the child.
    CloseHandle(childStdoutPipe);
    CloseHandle(childStdinPipe);

    CloseHandle(process_info.hThread);

    readPipe = (uint64_t)readPipe_;   // Parent reads child's output from this (ASYNC)
    writePipe = (uint64_t)writePipe_; // Parent writes to child's input via this (SYNC)
    pid = (uint64_t)process_info.hProcess;

    return readPipe;
}

void RunCommand::reapProcess() const
{
    if (WaitForSingleObject((HANDLE)pid, INFINITE) == WAIT_FAILED)
    {
        exitFailure(getErrorString());
    }

    if (!GetExitCodeProcess((HANDLE)pid, (LPDWORD)&exitStatus))
    {
        exitFailure(getErrorString());
    }

    if (!CloseHandle((HANDLE)pid) || !CloseHandle((HANDLE)readPipe) || !CloseHandle((HANDLE)writePipe))
    {
        exitFailure(getErrorString());
    }
}

#else

uint64_t RunCommand::startAsyncProcess(const char *command, uint64_t serverFd)
{
    // Create pipes for stdout and stderr
    int stdoutPipesLocal[2];
    if (pipe(stdoutPipesLocal) == -1)
    {
        exitFailure(getErrorString());
    }

    // Create pipe for stdin
    int stdinPipesLocal[2];
    if (pipe(stdinPipesLocal) == -1)
    {
        exitFailure(getErrorString());
    }

    readPipe = stdoutPipesLocal[0];
    writePipe = stdinPipesLocal[1];

    pid = fork();
    if (pid == -1)
    {
        exitFailure(getErrorString());
    }
    if (pid == 0)
    {
        // Child process

        // Redirect stdin from the pipe
        dup2(stdinPipesLocal[0], STDIN_FILENO);

        // Redirect stdout and stderr to the pipes
        dup2(stdoutPipesLocal[1], STDOUT_FILENO); // Redirect stdout to stdout_pipe
        dup2(stdoutPipesLocal[1], STDERR_FILENO); // Redirect stderr to stderr_pipe

        // Close unused pipe ends
        close(stdoutPipesLocal[0]);
        close(stdoutPipesLocal[1]);
        close(stdinPipesLocal[0]);
        close(stdinPipesLocal[1]);

        wordexp_t p;
        if (wordexp(command, &p, 0) != 0)
        {
            perror("wordexp");
            _exit(127);
        }

        // p.we_wordv is a NULL-terminated argv suitable for exec*
        char **argv = p.we_wordv;

        // Use execvp so PATH is searched and environment is inherited
        execvp(argv[0], argv);

        // If execvp returns, it failed:
        perror("execvp");
        wordfree(&p);
        _exit(127);
    }

    // Parent process
    // Close unused pipe ends
    close(stdoutPipesLocal[1]);
    close(stdinPipesLocal[0]);
    return readPipe;
}

void RunCommand::reapProcess() const
{
    if (waitpid(pid, const_cast<int *>(&exitStatus), 0) < 0)
    {
        exitFailure(getErrorString());
    }
    if (close(readPipe) == -1)
    {
        exitFailure(getErrorString());
    }
    if (close(writePipe) == -1)
    {
        exitFailure(getErrorString());
    }
}
#endif

string compilerTestPrunedOutput;

#ifdef _WIN32
#define COMPILER_TEST ".\\CompilerTest.exe"
#else
#define COMPILER_TEST "./CompilerTest"
#endif

struct BuildSystemTest
{

    //  Compiler can use this function to read the BMI file. BMI should be read using this function to conserve memory.
    static tl::expected<ProcessMappingOfBMIFile, std::string> readSharedMemoryBMIFile(const BMIFile &file)
    {
        return IPCManagerCompiler::readSharedMemoryBMIFile(file);
    }
};

bool endsWith(const std::string &str, const std::string &suffix)
{
    if (suffix.size() > str.size())
    {
        return false;
    }
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void readCompilerMessage(const uint64_t serverFd, const uint64_t readFd)
{
#ifdef _WIN32
    HANDLE hIOCP = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(serverFd));
    HANDLE hPipe = reinterpret_cast<HANDLE>(readFd);

    while (true)
    {
        char buffer[4096];
        DWORD bytesRead = 0;
        OVERLAPPED overlapped = {0};

        // Initiate async read. Even if it is completed successfully, we will get the completion packet. We don't get
        // the packet only if it fails immediately with error other than ERROR_IO_PENDING
        BOOL result = ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, &overlapped);

        DWORD error = GetLastError();
        if (!result && error != ERROR_IO_PENDING)
        {
            if (error == ERROR_BROKEN_PIPE)
            {
                // read complete
                return;
            }
            exitFailure(getErrorString());
        }

        bytesRead = 0;

        // Wait for the read to complete.
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED completedOverlapped = nullptr;

        if (!GetQueuedCompletionStatus(hIOCP, &bytesRead, &completionKey, &completedOverlapped, INFINITE))
        {
            if (GetLastError() == ERROR_BROKEN_PIPE)
            {
                // completed
                return;
            }
            exitFailure(getErrorString());
        }

        // Verify completion is for our pipe
        if (completionKey != (ULONG_PTR)hPipe)
        {
            exitFailure("Unexpected completion key");
        }

        if (bytesRead == 0)
        {
            // completed
            return;
        }

        // Append read data to string
        for (DWORD i = 0; i < bytesRead; ++i)
        {
            compilerTestPrunedOutput.push_back(buffer[i]);
        }

        // Check for terminator
        if (endsWith(compilerTestPrunedOutput, delimiter))
        {
            return;
        }
    }
#else

    epoll_event ev{};
    ev.events = EPOLLIN;
    if (epoll_ctl(serverFd, EPOLL_CTL_ADD, readFd, &ev) == -1)
    {
        exitFailure(getErrorString());
    }

    epoll_wait(serverFd, &ev, 1, -1);
    while (true)
    {
        char buffer[4096];
        const int readCount = read(readFd, buffer, 4096);
        if (readCount == 0)
        {
            return;
        }
        if (readCount == -1)
        {
            exitFailure(getErrorString());
        }
        for (uint32_t i = 0; i < readCount; ++i)
        {
            compilerTestPrunedOutput.push_back(buffer[i]);
        }

        if (endsWith(compilerTestPrunedOutput, delimiter))
        {
            break;
        }
    }

    if (epoll_ctl(serverFd, EPOLL_CTL_DEL, readFd, &ev) == -1)
    {
        exitFailure(getErrorString());
    }
#endif
}

void pruneCompilerOutput(IPCManagerBS &manager, char (&buffer)[320], CTB &type)
{
    // Prune the compiler output. and make a new string of the compiler-message output.
    const uint32_t prunedSize = compilerTestPrunedOutput.size();
    if (prunedSize < 4 + strlen(delimiter))
    {
        exitFailure("received string only has delimiter but not the size of payload\n");
    }

    const uint32_t payloadSize =
        *reinterpret_cast<uint32_t *>(compilerTestPrunedOutput.data() + (prunedSize - (4 + strlen(delimiter))));
    const char *payloadStart = compilerTestPrunedOutput.data() + (prunedSize - (4 + strlen(delimiter) + payloadSize));
    manager.serverReadString = string_view(payloadStart, payloadSize);
    if (const auto &r2 = manager.receiveMessage(buffer, type); !r2)
    {
        exitFailure(r2.error());
    }
    compilerTestPrunedOutput.resize(prunedSize - (4 + strlen(delimiter) + payloadSize));
}

void closeHandle(const uint64_t fd)
{
#ifdef _WIN32
    // CloseHandle returns non-zero on success, so we need to check for failure (zero)
    if (!CloseHandle(reinterpret_cast<HANDLE>(fd)))
    {
        exitFailure(getErrorString());
    }
#else
    if (close(fd) == -1)
    {
        exitFailure(getErrorString());
    }
#endif
}

uint64_t createMultiplex()
{
#ifdef _WIN32
    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, // handle to associate
                                         nullptr,              // existing IOCP handle
                                         0,                    // completion key (use pipe handle)
                                         0                     // number of concurrent threads (0 = default)
    );
    if (iocp == nullptr)
    {
        exitFailure(getErrorString());
    }
    return reinterpret_cast<uint64_t>(iocp);
#else
    return epoll_create1(0);
#endif
}

int runTest()
{
    CTBLastMessage lastMessage;
    const uint64_t serverFd = createMultiplex();

    RunCommand compilerTest;
    compilerTest.startAsyncProcess(COMPILER_TEST, serverFd);
    IPCManagerBS manager{compilerTest.writePipe};

    CTB type;
    char buffer[320];
    while (true)
    {
        bool loopExit = false;

        readCompilerMessage(serverFd, compilerTest.readPipe);
        if (!endsWith(compilerTestPrunedOutput, delimiter))
        {
            exitFailure("early exit by CompilerTest");
        }
        pruneCompilerOutput(manager, buffer, type);

        switch (type)
        {

            // Compiler will only send CTBNonModule. CTBModule is not tested as the build-system needs to create a
            // shared memory-file using IPCManangerBS::createSharedMemoryBMIFile() mapping before sending a BMIFile.
            // Similarly, IPCManagerCompiler on receiving a BMIFile will call
            // IPCManagerCompiler::readSharedMemoryBMIFile and populate internal data structures. To accurately test, we
            // will have to create lots of on-disk files. The following tests the setup and communication mechanism
            // while the shared-memory file is tested separately. However, Message serialization and deserialization is
            // not fully tested.
        case CTB::NON_MODULE: {
            const auto &ctbNonModule = reinterpret_cast<CTBNonModule &>(buffer);
            printMessage(ctbNonModule, false);
            BTCNonModule nonModule = getBTCNonModule();
            if (const auto &r2 = manager.sendMessage(nonModule); !r2)
            {
                exitFailure(r2.error());
            }
            printMessage(nonModule, true);
        }

        break;

        case CTB::LAST_MESSAGE: {
            lastMessage = reinterpret_cast<CTBLastMessage &>(buffer);
            if (lastMessage.logicalName != "module_cat")
            {
                exitFailure(fmt::format("CTBLastMessage logical-name not module_cat. logical-name is {}.",
                                        lastMessage.logicalName));
            }
            printMessage(lastMessage, false);
            loopExit = true;
        }

        break;
        }

        if (loopExit)
        {
            break;
        }
    }

    // We have received a message for memory mapped BMI File. We will first create the server memory mapping. And then
    // close that mapping. And then create the client memory mapping, print out the file contents. And then close that
    // mapping. And then finally send the BTCLastMessage. This makes code coverage 100%.

    BMIFile bmi;
    bmi.filePath = (std::filesystem::current_path() / "bmi.txt").generic_string();
    bmi.fileSize = lastMessage.fileSize;
    // Creates server mapping to already created mapping
    if (const auto &r2 = IPCManagerBS::createSharedMemoryBMIFile(bmi); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        // closes the server mapping
        if (const auto &r3 = IPCManagerBS::closeBMIFileMapping(r2.value()); !r3)
        {
            exitFailure(r3.error());
        }
    }

    // creates compiler mapping to already created mapping and read contents.
    if (const auto &r2 = BuildSystemTest::readSharedMemoryBMIFile(bmi); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        if (const auto &processMappingOfBMIFile = r2.value();
            fileToString(bmi.filePath) != processMappingOfBMIFile.file)
        {
            exitFailure(fmt::format("File Contents not similar for {}", bmi.filePath));
        }
        if (const auto &r3 = IPCManagerCompiler::closeBMIFileMapping(r2.value()); !r3)
        {
            exitFailure(r3.error());
        }
    }

    ProcessMappingOfBMIFile bmi2Mapping;
    {
        // We don't assign the file.fileSize this-time. This IPCManagerBS::createSharedMemoryBMIFile will return it as
        // an out variable. This is used when build-system has to make a mapping for a prebuilt file. This case of
        // opening mapping first time is little different from creating mapping when another process (the compiler) has
        // already created one. Build-system preserves the out variable file.fileSize as it is passed to the later
        // compilations to save them from one system call.

        BMIFile bmi2;
        bmi2.filePath = (std::filesystem::current_path() / "bmi2.txt").generic_string();
        string bmi2Content = getRandomString();
        std::ofstream(bmi2.filePath) << bmi2Content;

        // creates server mapping to a new file.
        if (const auto &r2 = IPCManagerBS::createSharedMemoryBMIFile(bmi2); !r2)
        {
            exitFailure(r2.error());
        }
        else
        {
            // This will be closed later-on after receiving the lastMessage. We need to close since the test is being
            // repeated.
            bmi2Mapping = r2.value();
        }

        if (bmi2.fileSize != bmi2Content.size())
        {
            exitFailure(fmt::format("file.fileSize is different from fileContent.size\n"));
        }

        // After receiving the next message, CompilerTest will check that bmi2.txt is same as the received mapping. This
        // is tested to ensure that if the build-system is making the bmi first time is working correctly.
        constexpr BTCLastMessage btcLastMessage;

        if (const auto &r2 = manager.sendMessage(btcLastMessage); !r2)
        {
            exitFailure(r2.error());
        }
        print("Reply to Second CTBLastMessage ");
        printMessage(btcLastMessage, true);
    }

    // As CompilerTest will output some print statements.
    readCompilerMessage(serverFd, compilerTest.readPipe);

    compilerTest.reapProcess();
    if (compilerTest.exitStatus != EXIT_SUCCESS)
    {
        print("CompilerTest did not exit successfully. ExitCode {}\n", compilerTest.exitStatus);
    }

    // Only after reaping process, we close the file mapping, so the CompilerTest check the mapping content with the
    // file content.
    if (const auto &r2 = IPCManagerBS::closeBMIFileMapping(bmi2Mapping); !r2)
    {
        exitFailure(r2.error());
    }

    return EXIT_SUCCESS;
}

int main()
{
    runTest();
    fmt::println("\n\n\nCompilerTest Output\n\n\n {}", compilerTestPrunedOutput);
    compilerTestPrunedOutput.clear();
    runTest();
    fmt::println("\n\n\nCompilerTest Output\n\n\n {}", compilerTestPrunedOutput);
}