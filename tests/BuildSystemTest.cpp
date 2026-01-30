#include "IPCManagerBS.hpp"
#include "IPCManagerCompiler.hpp"
#include "Testing.hpp"
#include "fmt/printf.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#else
#include "sys/epoll.h"
#include "sys/wait.h"
#include "wordexp.h"
#include <unistd.h>
#endif

using fmt::print, std::string_view;

enum class ProcessState
{
    LAUNCHED,
    OUTPUT_CONNECTED,
    COMPLETED,
    CONNECTED,
    IPCFD_CLOSED,
    OUTPUTFD_CLOSED,
};

struct RunCommand
{
    string output;
    uint64_t pid;
    uint64_t stdoutPipe;
    uint64_t stdinPipe;
    int exitStatus;
#ifdef _WIN32
    ProcessState processState = ProcessState::LAUNCHED;
#else
    ProcessState processState = ProcessState::OUTPUT_CONNECTED;
#endif

    RunCommand() = default;
    uint64_t startAsyncProcess(const char *command);
    bool wasProcessLaunchIncomplete(uint64_t index);
    void reapProcess();
};

#ifdef _WIN32
#include <Windows.h>
#include <chrono>

// Copied partially from Ninja
uint64_t RunCommand::startAsyncProcess(const char *command, Builder &builder, BTarget *bTarget)
{
    // One BTarget can launch multiple processes so we also append the clock::now().
    const string pipe_name =
        FORMAT("\\\\.\\pipe\\{}{}", bTarget->id, std::chrono::high_resolution_clock::now().time_since_epoch().count());
    HANDLE pipe_ = CreateNamedPipeA(pipe_name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE,
                                    PIPE_UNLIMITED_INSTANCES, 0, 0, INFINITE, NULL);
    if (pipe_ == INVALID_HANDLE_VALUE)
    {
        printErrorMessage(N2978::getErrorString());
    }

    const uint64_t eventDataIndex = builder.registerEventData(bTarget, (uint64_t)pipe_);
    if (!CreateIoCompletionPort(pipe_, (HANDLE)builder.serverFd, eventDataIndex, 0))
    {
        printErrorMessage(N2978::getErrorString());
    }

    if (CompletionKey &k = eventData[eventDataIndex];
        !ConnectNamedPipe(pipe_, &reinterpret_cast<OVERLAPPED &>(k.overlappedBuffer)) &&
        GetLastError() != ERROR_IO_PENDING)
    {
        printErrorMessage(N2978::getErrorString());
    }

    // Get the write end of the pipe as a handle inheritable across processes.
    HANDLE output_write_handle = CreateFileA(pipe_name.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE child_pipe;
    if (!DuplicateHandle(GetCurrentProcess(), output_write_handle, GetCurrentProcess(), &child_pipe, 0, TRUE,
                         DUPLICATE_SAME_ACCESS))
    {
        printErrorMessage(N2978::getErrorString());
    }
    CloseHandle(output_write_handle);

    SECURITY_ATTRIBUTES security_attributes;
    memset(&security_attributes, 0, sizeof(SECURITY_ATTRIBUTES));
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;
    // Must be inheritable so subprocesses can dup to children.
    HANDLE nul = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             &security_attributes, OPEN_EXISTING, 0, NULL);
    if (nul == INVALID_HANDLE_VALUE)
    {
        printErrorMessage(N2978::getErrorString());
    }

    STARTUPINFOA startup_info;
    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb = sizeof(STARTUPINFO);

    bool use_console_ = false;
    if (!use_console_)
    {
        startup_info.dwFlags = STARTF_USESTDHANDLES;
        startup_info.hStdInput = nul;
        startup_info.hStdOutput = child_pipe;
        startup_info.hStdError = child_pipe;
    }
    // In the console case, child_pipe is still inherited by the child and closed
    // when the subprocess finishes, which then notifies ninja.

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    // Ninja handles ctrl-c, except for subprocesses in console pools.
    DWORD process_flags = use_console_ ? 0 : CREATE_NEW_PROCESS_GROUP;

    // Do not prepend 'cmd /c' on Windows, this breaks command
    // lines greater than 8,191 chars.
    if (!CreateProcessA(NULL, (char *)command, NULL, NULL,
                        /* inherit handles */ TRUE, process_flags, NULL, NULL, &startup_info, &process_info))
    {
        printErrorMessage(FORMAT("CreateProcessA failed for Command\n{}\nError\n", command, N2978::getErrorString()));
    }

    // Close pipe channel only used by the child.
    if (child_pipe)
    {
        CloseHandle(child_pipe);
    }
    CloseHandle(nul);

    CloseHandle(process_info.hThread);

    stdoutPipe = (uint64_t)pipe_;
    pid = (uint64_t)process_info.hProcess;

    return eventDataIndex;
}

bool RunCommand::wasProcessLaunchIncomplete(const uint64_t index)
{
#ifdef _WIN32
    if (processState == ProcessState::LAUNCHED)
    {
        CompletionKey &k = eventData[index];
        uint64_t bytesRead = 0;
        const bool result =
            GetOverlappedResult((HANDLE)k.handle, &(OVERLAPPED &)k.overlappedBuffer, (LPDWORD)&bytesRead, false);
        if (result)
        {
            processState = ProcessState::OUTPUT_CONNECTED;
            return true;
        }
        if (GetLastError() == ERROR_BROKEN_PIPE)
        {
            processState = ProcessState::COMPLETED;
            return true;
        }
        printErrorMessage(N2978::getErrorString());
    }
    return false;
#else
    return false;
#endif
}

void RunCommand::reapProcess() const
{
    if (WaitForSingleObject((HANDLE)pid, INFINITE) == WAIT_FAILED)
    {
        printErrorMessage(N2978::getErrorString());
    }
    if (!GetExitCodeProcess((HANDLE)pid, (LPDWORD)&exitStatus))
    {
        printErrorMessage(N2978::getErrorString());
    }

    if (!CloseHandle((HANDLE)pid) || !CloseHandle((HANDLE)stdoutPipe))
    {
        printErrorMessage(N2978::getErrorString());
    }
}

void RunCommand::killModuleProcess(const string &processName) const
{
    // Exit code you want to assign to the terminated process
    DWORD exitCode = 1;

    if (!TerminateProcess((HANDLE)pid, exitCode))
    {
        printErrorMessage(FORMAT("Killing module process {} failed.\n", processName));
    }

    CloseHandle((HANDLE)pid); // Clean up when you’re done
}

#else

uint64_t RunCommand::startAsyncProcess(const char *command)
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

    stdoutPipe = stdoutPipesLocal[0];
    stdinPipe = stdinPipesLocal[1];

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
    return stdoutPipe;
}

bool RunCommand::wasProcessLaunchIncomplete(uint64_t index)
{
    return false;
}

void RunCommand::reapProcess()
{
    if (waitpid(pid, &exitStatus, 0) < 0)
    {
        exitFailure(getErrorString());
    }
    if (close(stdoutPipe) == -1)
    {
        exitFailure(getErrorString());
    }
}

#endif

bool first = true;
std::thread *thr;

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

bool ends_with(const std::string &str, const std::string &suffix)
{
    if (suffix.size() > str.size())
    {
        return false;
    }
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void readCompilerMessage(const uint64_t serverFd, const IPCManagerBS &manager, char (&buffer)[320], CTB &type)
{
    std::string str;

#ifdef _WIN32
    HANDLE hIOCP = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(serverFd));
    HANDLE hPipe = reinterpret_cast<HANDLE>(manager.fd);

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
            exitFailure(getErrorString());
        }

        bytesRead = 0;

        // Wait for the read to complete.
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED completedOverlapped = nullptr;

        if (!GetQueuedCompletionStatus(hIOCP, &bytesRead, &completionKey, &completedOverlapped, INFINITE))
        {
            exitFailure(getErrorString());
        }

        // Verify completion is for our pipe
        if (completionKey != (ULONG_PTR)hPipe)
        {
            exitFailure("Unexpected completion key");
        }

        if (bytesRead == 0)
        {
            exitFailure("Pipe closed or no data read");
        }

        // Append read data to string
        for (DWORD i = 0; i < bytesRead; ++i)
        {
            str.push_back(buffer[i]);
        }

        // Check for terminator
        if (str[str.size() - 1] == ';')
        {
            str.pop_back();
            break;
        }
    }
#else

    epoll_event ev{};
    ev.events = EPOLLIN;
    if (epoll_ctl(serverFd, EPOLL_CTL_ADD, manager.fd, &ev) == -1)
    {
        exitFailure(getErrorString());
    }

    epoll_wait(serverFd, &ev, 1, -1);
    while (true)
    {
        char buffer[4096];
        const int readCount = read(manager.fd, buffer, 4096);
        if (readCount == 0 || readCount == -1)
        {
            exitFailure(getErrorString());
        }
        for (uint32_t i = 0; i < readCount; ++i)
        {
            str.push_back(buffer[i]);
        }

        if (ends_with(str, delimiter))
        {
            break;
        }
    }

    if (epoll_ctl(serverFd, EPOLL_CTL_DEL, manager.fd, &ev) == -1)
    {
        exitFailure(getErrorString());
    }
#endif
    string *str2 = new string(std::move(str));
    const_cast<string_view &>(manager.serverReadString) = string_view{str2->data(), str2->size() - strlen(delimiter)};
    if (const auto &r2 = manager.receiveMessage(buffer, type); !r2)
    {
        exitFailure(r2.error());
    }
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

void completeConnection(IPCManagerBS &manager, int serverFd)
{
#ifdef _WIN32
    HANDLE hIOCP = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(serverFd));
    HANDLE hPipe = reinterpret_cast<HANDLE>(manager.fd);

    if (const auto &r2 = manager.completeConnection(); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        if (!*r2)
        {
            // Connection is pending (ERROR_IO_PENDING)
            // Wait for IOCP to signal completion

            DWORD bytesTransferred = 0;
            ULONG_PTR completionKey = 0;
            LPOVERLAPPED overlapped = nullptr;

            if (!GetQueuedCompletionStatus(hIOCP, &bytesTransferred, &completionKey, &overlapped,
                                           INFINITE)) // Wait indefinitely
            {
                exitFailure(getErrorString());
            }

            // Verify this completion is for our pipe
            if (completionKey != (ULONG_PTR)hPipe)
            {
                exitFailure("Unexpected completion key");
            }

            // Connection is now complete - no need to call completeConnection again
            // The ConnectNamedPipe operation has finished
        }
    }
#else
    /*if (const auto &r2 = manager.completeConnection(); !r2)
    {
        exitFailure(r2.error());
    }
    else
    {
        if (!*r2)
        {
            epoll_event ev{};
            ev.events = EPOLLIN;
            if (epoll_ctl(serverFd, EPOLL_CTL_ADD, manager.fd, &ev) == -1)
            {
                exitFailure(getErrorString());
            }

            epoll_event ev2{};
            if (epoll_wait(serverFd, &ev2, 1, -1) == -1)
            {
                exitFailure(getErrorString());
            }
            if (epoll_ctl(serverFd, EPOLL_CTL_DEL, manager.fd, &ev) == -1)
            {
                exitFailure(getErrorString());
            }
            if (const auto &r3 = manager.completeConnection(); !r3)
            {
                exitFailure(r3.error());
            }
        }
    }*/
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
    compilerTest.startAsyncProcess(COMPILER_TEST);

    IPCManagerBS manager{compilerTest.stdoutPipe};

    CTB type;
    char buffer[320];
    while (true)
    {
        bool loopExit = false;

        readCompilerMessage(serverFd, manager, buffer, type);

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
            print("First ");
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

    IPCManagerBS oldManager = manager;
    // This tests the scenario where build-system has not yet called the receiveMessage after calling
    // the makeIPCManagerBS() while compiler process already exited and sent the CTBLastMessage.
    // To ensure that we make build-system manager before CompilerTest making compiler manager,
    // we make it here.

    // manager = makeBuildSystemManager((std::filesystem::current_path() / "test1").string(), serverFd);

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

        // After receiving the next message, CompilerTest will check that bmi2.txt is same as the received mapping.

        constexpr BTCLastMessage btcLastMessage;

        if (const auto &r2 = oldManager.sendMessage(btcLastMessage); !r2)
        {
            exitFailure(r2.error());
        }
        print("Reply to Second CTBLastMessage ");
        printMessage(btcLastMessage, true);
    }

    // we delay the receiveMessage. Compiler in this duration has exited with error after connecting.
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // CompilerTest would have exited by now
    readCompilerMessage(serverFd, manager, buffer, type);

    if (const auto &r2 = IPCManagerBS::closeBMIFileMapping(bmi2Mapping); !r2)
    {
        exitFailure(r2.error());
    }

    if (type != CTB::LAST_MESSAGE)
    {
        print("Test failed. Expected CTB::LAST_MESSAGE.\n");
    }

    print("Received CTBLastMessage on new manager.");
    printMessage(reinterpret_cast<CTBLastMessage &>(buffer), false);

    return EXIT_SUCCESS;
}

int main()
{
    first = true;
    runTest();
    thr->join();
    first = false;
    runTest();
    thr->join();
}