
#include "IPCManagerBS.hpp"
#include "Testing.hpp"

#include <Windows.h>
#include <filesystem>
#include <print>

using namespace std::filesystem;

using std::print;
using namespace N2978;

// Copied From Ninja code-base.
/// Wraps a synchronous execution of a CL subprocess.
struct CLWrapper
{
    CLWrapper() : env_block_(nullptr)
    {
    }

    /// Set the environment block (as suitable for CreateProcess) to be used
    /// by Run().
    void SetEnvBlock(void *env_block)
    {
        env_block_ = env_block;
    }

    /// Start a process and gather its raw output.  Returns its exit code.
    /// Crashes (calls Fatal()) on error.
    void Run(const std::string &command) const;

    void *env_block_;
};

// TODO
//  Error should throw and not exit
void Fatal(const char *msg, ...)
{
    va_list ap;
    fprintf(stderr, "ninja: fatal: ");
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");
#ifdef _WIN32
    // On Windows, some tools may inject extra threads.
    // exit() may block on locks held by those threads, so forcibly exit.
    fflush(stderr);
    fflush(stdout);
    ExitProcess(1);
#else
    exit(1);
#endif
}

string GetLastErrorString()
{
    DWORD err = GetLastError();

    char *msg_buf;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                   err, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (char *)&msg_buf, 0, nullptr);

    if (msg_buf == nullptr)
    {
        char fallback_msg[128] = {0};
        snprintf(fallback_msg, sizeof(fallback_msg), "GetLastError() = %d", err);
        return fallback_msg;
    }

    string msg = msg_buf;
    LocalFree(msg_buf);
    return msg;
}

void Win32Fatal(const char *function, const char *hint = nullptr)
{
    if (hint)
    {
        Fatal("%s: %s (%s)", function, GetLastErrorString().c_str(), hint);
    }
    else
    {
        Fatal("%s: %s", function, GetLastErrorString().c_str());
    }
}

HANDLE stdout_read, stdout_write;
PROCESS_INFORMATION process_info = {};
void CLWrapper::Run(const string &command) const
{
    SECURITY_ATTRIBUTES security_attributes = {};
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;

    if (!CreatePipe(&stdout_read, &stdout_write, &security_attributes, 0))
        Win32Fatal("CreatePipe");

    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0))
        Win32Fatal("SetHandleInformation");

    STARTUPINFOA startup_info = {};
    startup_info.cb = sizeof(STARTUPINFOA);
    startup_info.hStdError = stdout_write;
    startup_info.hStdOutput = stdout_write;
    startup_info.dwFlags |= STARTF_USESTDHANDLES;

    if (!CreateProcessA(nullptr, (char *)command.c_str(), nullptr, nullptr,
                        /* inherit handles */ TRUE, 0, env_block_, nullptr, &startup_info, &process_info))
    {
        Win32Fatal("CreateProcess");
    }

    if (!CloseHandle(stdout_write))
    {
        Win32Fatal("CloseHandle");
    }
}

int printOutputAndClosePipes()
{

    string output;
    // Read all output of the subprocess.
    DWORD read_len = 1;
    while (read_len)
    {
        char buf[64 << 10];
        read_len = 0;
        if (!::ReadFile(stdout_read, buf, sizeof(buf), &read_len, nullptr) && GetLastError() != ERROR_BROKEN_PIPE)
        {
            Win32Fatal("ReadFile");
        }
        output.append(buf, read_len);
    }

    // Wait for it to exit and grab its exit code.
    if (WaitForSingleObject(process_info.hProcess, INFINITE) == WAIT_FAILED)
        Win32Fatal("WaitForSingleObject");
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(process_info.hProcess, &exit_code))
        Win32Fatal("GetExitCodeProcess");

    if (!CloseHandle(stdout_read) || !CloseHandle(process_info.hProcess) || !CloseHandle(process_info.hThread))
    {
        Win32Fatal("CloseHandle");
    }

    return exit_code;
}

int main()
{
    // compile main.cpp which imports B.cpp which imports A.cpp.

    if (system(R"(.\clang.exe -std=c++20 mod2.cppm -c -fmodule-output="mod2.pcm"  -fmodules-reduced-bmi -o  mod2.o)") !=
        EXIT_SUCCESS)
    {
        print("could not run the first command\n");
    }

    if (system(
            R"(.\clang.exe -std=c++20 mod.cppm -c -fmodule-output="mod.pcm"  -fmodules-reduced-bmi -o  mod.o  -fmodule-file=mod2="./mod2.pcm")") !=
        EXIT_SUCCESS)
    {
        print("could not run the second command command\n");
    }

    string current = current_path().generic_string() + '/';
    string mainFilePath = current + "main.o";
    string modFilePath = current + "mod.pcm";
    string mod1FilePath = current + "mod1.pcm";
    string mod2FilePath = current + "mod2.pcm";

    bool first = true;
    if (const auto &r = makeIPCManagerBS(std::move(mainFilePath)); r)
    {
        const IPCManagerBS &manager = *r;

        string compileCommand = R"(.\clang.exe -std=c++20 main.cpp  -o main.o -noScanIPC")";

        bool loopExit = false;
        while (true)
        {
            CTB type;
            char buffer[320];
            if (const auto &r2 = manager.receiveMessage(buffer, type); !r2)
            {
                print("{}", "creating manager failed" + r2.error() + "\n");
            }

            switch (type)
            {

            case CTB::MODULE: {
                const auto &ctbModule = reinterpret_cast<CTBModule &>(buffer);
                printMessage(ctbModule, false);

                if (first)
                {
                    first = false;
                    if (ctbModule.moduleName != "mod")
                    {
                        print("Message Name not mod\n");
                        exit(EXIT_FAILURE);
                    }

                    BMIFile mod;
                    mod.filePath = modFilePath;
                    mod.fileSize = 0;

                    BMIFile mod2;
                    mod2.filePath = mod2FilePath;
                    mod2.fileSize = 0;

                    BTCModule b;
                    b.requested = std::move(mod);
                    b.deps.emplace_back(std::move(mod2));

                    manager.sendMessage(b);
                    printMessage(b, true);
                }
                else
                {
                    loopExit = true;
                    if (ctbModule.moduleName != "mod1")
                    {
                        print("Message Name not mod1\n");
                        exit(EXIT_FAILURE);
                    }

                    BMIFile mod1;
                    mod1.filePath = mod1FilePath;
                    mod1.fileSize = 0;

                    BTCModule b;
                    b.requested = std::move(mod1);

                    manager.sendMessage(b);
                    printMessage(b, true);
                }
            }

            break;

            case CTB::NON_MODULE: {
            }

            case CTB::LAST_MESSAGE: {
            }

                print("Unexpected message received");
            }

            if (loopExit)
            {
                break;
            }
        }
    }
    else
    {
        print("{}", "creating manager failed" + r.error() + "\n");
    }

    return printOutputAndClosePipes();
}