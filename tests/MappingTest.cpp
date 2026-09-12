#include "IPCManagerCompiler.hpp"
#include "TestProcess.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

using namespace P2978;
namespace fs = std::filesystem;

struct CompilerTest
{
    static auto read(std::string_view filePath)
    {
        return IPCManagerCompiler::readBMIFile(filePath);
    }
    static std::string_view cached(IPCManagerCompiler &manager, const std::string &path)
    {
        auto result = manager.getOrMapBMIFile(path);
        if (!result)
        {
            std::cerr << result.error() << '\n';
            std::exit(1);
        }
        return *result;
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
    if (process.exitStatus != 0)
        std::cerr << output;
    require(process.exitStatus == 0, "Mapping helper failed");
}

static void checkMappings(const fs::path &directory, const std::string &content)
{
    const std::string path = (directory / "module with spaces.pcm").string();
    std::string_view survivingContents;
    {
        IPCManagerCompiler manager;
        auto first = CompilerTest::cached(manager, path);
        auto alias = CompilerTest::cached(manager, path);
        require(first.data() == alias.data() && first == content, "Repeated BMI opened a second mapping");
        const auto found = manager.findBMIContents(path);
        require(found && found->data() == first.data(), "BMI lookup did not return cached contents");
        require(!manager.findBMIContents("uncached.pcm"), "Unknown BMI lookup succeeded");
        survivingContents = first;
    }
    require(survivingContents == content, "Manager destruction invalidated process-lifetime BMI contents");
    const std::string missing = (directory / "missing").string();
    require(!CompilerTest::read(missing), "Missing BMI accepted");
    const std::string empty = (directory / "empty").string();
    std::ofstream(empty).close();
    require(!CompilerTest::read(empty), "Empty BMI accepted");

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

    // A dependency's system flag and alias count follow its path directly; no BMI size is transmitted.
    ModuleDep dependency;
    dependency.filePath = path;
    dependency.isSystem = true;
    dependency.logicalNames = {"Module"};
    encoded.clear();
    Manager::writeModuleDep(encoded, dependency);
    require(encoded.size() == 1 + sizeof(uint32_t) + path.size() + 1 + 1 + sizeof(uint32_t) + sizeof(uint32_t) +
                                  std::string_view("Module").size(),
            "Module dependency still contains a BMI size field");
    offset = 0;
    const auto isHeaderUnit = Manager::readBool(encoded, offset);
    const auto decodedPath = Manager::readPath(encoded, offset);
    const auto isSystem = Manager::readBool(encoded, offset);
    const auto aliasCount = Manager::readUInt32(encoded, offset);
    const auto aliasName = Manager::readString(encoded, offset);
    require(isHeaderUnit && !*isHeaderUnit && decodedPath && *decodedPath == path && isSystem && *isSystem &&
                aliasCount && *aliasCount == 1 && aliasName && *aliasName == "Module" && offset == encoded.size(),
            "Module dependency fields do not immediately follow its path");

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
    const std::string replacement = (directory / "replacement.bin").string();
    std::ofstream(replacement, std::ios::binary) << std::string(4, '\0');
    {
        IPCManagerCompiler manager;
        require(bool(manager.readEntriesFromFile(mock)), "Could not load mock dependencies");
        auto first = manager.findResponse("Module", FileType::MODULE);
        auto alias = manager.findResponse("Alias", FileType::MODULE);
        require(first && alias && first->bmiContents == content, "Mock BMI contents differ");
        require(first->bmiContents.data() == alias->bmiContents.data(), "Mock aliases did not reuse their mapping");
        require(!manager.findResponse("Missing", FileType::MODULE), "Mock lookup attempted live IPC");
        require(!manager.readEntriesFromFile(replacement), "Second mock initialization succeeded");
        require(first->filePath == path && first->bmiContents == content,
                "Rejected mock initialization invalidated an existing response");
        const auto unchanged = manager.findResponse("Module", FileType::MODULE);
        require(unchanged && unchanged->filePath == path && unchanged->bmiContents.data() == first->bmiContents.data(),
                "Rejected mock initialization changed the response cache");
    }
    {
        IPCManagerCompiler manager;
        require(!manager.readEntriesFromFile(missing), "Missing mock file accepted");
        require(!manager.readEntriesFromFile(mock), "Mock initialization retried after a failed first attempt");
    }
    encoded.pop_back();
    std::ofstream(mock, std::ios::binary) << encoded;
    {
        IPCManagerCompiler manager;
        require(!manager.readEntriesFromFile(mock), "Truncated mock file accepted");
        const auto retained = manager.findResponse("Module", FileType::MODULE);
        require(retained && retained->filePath == path && retained->bmiContents == content,
                "Complete entry before mock truncation was not retained");
        require(!manager.readEntriesFromFile(replacement), "Mock initialization retried after a truncated first file");
        require(retained->filePath == path && retained->bmiContents == content,
                "Repeated failed mock initialization invalidated an existing view");
    }
}

int main(int argc, char **argv)
{
    const std::string content(8192, 'x'); // Includes a page-aligned file size.
    if (argc == 3)
    {
        const std::string path = argv[2];
        const std::string_view mode = argv[1];
        if (mode == "--produce")
        {
            std::ofstream out(path, std::ios::binary);
            out << content;
            out.close();
            require(bool(out), "Could not publish BMI");
        }
        else if (mode == "--consume")
        {
            const auto contents = CompilerTest::read(path);
            require(bool(contents), "Consumer could not open BMI after producer exit");
            require(*contents == content, "Consumer read incorrect BMI bytes");
        }
        else if (mode == "--check-mappings")
            checkMappings(path, content);
        else
            require(false, "Unknown mapping helper mode");
        return 0;
    }

    const fs::path directory =
        fs::temp_directory_path() /
        ("ipc2978-mapping-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(directory);
    const std::string path = (directory / "module with spaces.pcm").string();
    const std::string executable = fs::absolute(argv[0]).string();
    auto command = [&](const char *mode, const std::string &argument) {
        return '"' + executable + "\" " + mode + " \"" + argument + '"';
    };

    // No process creates or retains a central mapping. The producer exits before any reader starts.
    ipc2978_test::TestProcess producer{fail};
    producer.startAsyncProcess(command("--produce", path).c_str());
    finish(producer);
    std::vector<std::unique_ptr<ipc2978_test::TestProcess>> consumers;
    for (uint64_t i = 0; i < 4; ++i)
    {
        consumers.emplace_back(std::make_unique<ipc2978_test::TestProcess>(fail));
        consumers.back()->startAsyncProcess(command("--consume", path).c_str());
    }
    for (const auto &consumer : consumers)
        finish(*consumer);

    // All views intentionally last until process exit. Keep the parent unmapped so Windows can remove the files.
    ipc2978_test::TestProcess checks{fail};
    checks.startAsyncProcess(command("--check-mappings", directory.string()).c_str());
    finish(checks);
    fs::remove_all(directory);
}
