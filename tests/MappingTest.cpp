#include "IPCManagerCompiler.hpp"
#include "TestProcess.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace P2978;
namespace fs = std::filesystem;

struct CompilerTest
{
    static auto read(const BMIFile &file)
    {
        return IPCManagerCompiler::readBMIFile(file);
    }
    static Mapping cached(IPCManagerCompiler &manager, const std::string &path)
    {
        std::string message;
        Manager::writePath(message, path);
        Manager::writeUInt32(message, UINT32_MAX);
        uint64_t offset = 0;
        auto result = manager.readProcessMappingOfBMIFile(message, offset);
        if (!result)
        {
            std::cerr << result.error() << '\n';
            std::exit(1);
        }
        return result->mapping;
    }
};

static void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

static void fail(const std::string &message)
{
    require(false, message.c_str());
}

static void finish(ipc2978_test::TestProcess &process)
{
    std::string output;
    require(!process.readCompilerMessage(output), "Unexpected IPC request from mapping helper");
    process.reapProcess();
    require(process.exitStatus == 0, "Mapping helper failed");
}

int main(int argc, char **argv)
{
    const std::string content(8192, 'x'); // Includes a page-aligned file size.
    if (argc == 3)
    {
        const std::string path = argv[2];
        if (std::string_view(argv[1]) == "--produce")
        {
            std::ofstream out(path, std::ios::binary);
            out << content;
            out.close();
            require(bool(out), "Could not publish BMI");
        }
        else
        {
            const auto mapping = CompilerTest::read({path});
            require(bool(mapping), "Consumer could not open BMI after producer exit");
            require(mapping->file == content, "Consumer read incorrect BMI bytes");
        }
        return 0;
    }

    const fs::path directory =
        fs::temp_directory_path() /
        ("ipc2978-mapping-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(directory);
    const std::string path = (directory / "module with spaces.pcm").string();
    const std::string executable = fs::absolute(argv[0]).string();
    auto command = [&](const char *mode) { return '"' + executable + "\" " + mode + " \"" + path + '"'; };

    // No process creates or retains a central mapping. The producer exits before any reader starts.
    ipc2978_test::TestProcess producer{fail};
    producer.startAsyncProcess(command("--produce").c_str());
    finish(producer);
    std::vector<std::unique_ptr<ipc2978_test::TestProcess>> consumers;
    for (uint64_t i = 0; i < 4; ++i)
    {
        consumers.emplace_back(std::make_unique<ipc2978_test::TestProcess>(fail));
        consumers.back()->startAsyncProcess(command("--consume").c_str());
    }
    for (uint64_t i = 0; i < consumers.size(); ++i)
        finish(*consumers[i]);

    std::weak_ptr<const void> lifetime;
    {
        IPCManagerCompiler manager;
        auto first = CompilerTest::cached(manager, path);
        auto alias = CompilerTest::cached(manager, path);
        require(first.owner == alias.owner, "Repeated BMI opened a second mapping");
        require(manager.filePathProcessMapping.size() == 1, "Duplicate BMI cache entry");
        lifetime = first.owner;
        manager.filePathProcessMapping.clear();
        first = {};
        require(!lifetime.expired() && alias.file == content, "Alias lost its mapping owner");
    }
    require(lifetime.expired(), "Mapping survived its last owner");
    require(!CompilerTest::read({path, 1}), "Incorrect known BMI size accepted");
    const std::string missing = (directory / "missing").string();
    require(!CompilerTest::read({missing}), "Missing BMI accepted");
    const std::string empty = (directory / "empty").string();
    std::ofstream(empty).close();
    require(!CompilerTest::read({empty}), "Empty BMI accepted");

#ifndef _WIN32
    require(!Manager::writeAll(-1, "x", 1), "Failed syscall treated as an unsigned byte count");
#endif

    // Wire integers remain four bytes even though local parsing offsets are uint64_t.
    std::string encoded;
    Manager::writeUInt32(encoded, UINT32_MAX);
    require(encoded == std::string(4, '\xff'), "IPC integer is not the original four-byte encoding");
    uint64_t offset = 0;
    const auto decoded = Manager::readUInt32(encoded, offset);
    require(decoded && *decoded == UINT32_MAX && offset == 4, "32-bit IPC integer decoded incorrectly");
    offset = UINT64_MAX;
    require(!Manager::readUInt32(encoded, offset), "Overflowing local offset accepted");
    offset = 0;
    require(!Manager::readString(encoded, offset), "Overflowing string size accepted");
    encoded.clear();
    Manager::writeString(encoded, "unterminated");
    offset = 0;
    require(!Manager::readPath(encoded, offset), "Unterminated IPC path accepted");

    const std::string mock = (directory / "dependencies.bin").string();
    encoded.clear();
    Manager::writeUInt32(encoded, 2);
    for (const char *name : {"Module", "Alias"})
    {
        Manager::writeString(encoded, name);
        Manager::writePath(encoded, path);
        encoded.push_back(static_cast<uint8_t>(FileType::MODULE));
        encoded.push_back(false);
    }
    std::ofstream(mock, std::ios::binary) << encoded;
    {
        IPCManagerCompiler manager;
        require(bool(manager.readEntriesFromFile(mock)), "Could not load mock dependencies");
        auto first = manager.findResponse("Module", FileType::MODULE);
        auto alias = manager.findResponse("Alias", FileType::MODULE);
        require(first && alias && first->mapping.file == content, "Mock BMI contents differ");
        require(first->mapping.owner == alias->mapping.owner && manager.filePathProcessMapping.size() == 1,
                "Mock aliases did not reuse their mapping");
        require(!manager.findResponse("Missing", FileType::MODULE), "Mock lookup attempted live IPC");
    }
    {
        IPCManagerCompiler manager;
        require(!manager.readEntriesFromFile(missing), "Missing mock file accepted");
    }
    encoded.pop_back();
    std::ofstream(mock, std::ios::binary) << encoded;
    {
        IPCManagerCompiler manager;
        require(!manager.readEntriesFromFile(mock), "Truncated mock file accepted");
    }
    fs::remove_all(directory);
}
