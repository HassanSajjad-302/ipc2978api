
#ifndef MANAGER_HPP
#define MANAGER_HPP

#include "Messages.hpp"
#include "expected.hpp"

#include <string>
#include <vector>

namespace P2978
{

// A 32-byte marker terminates each pipe message. Compiler requests also put a
// uint32_t payload size immediately before it, separating requests from diagnostics.
inline const char *delimiter = "DELIMITER"
                               "\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5"
                               "DELIMITER";

enum class ErrorCategory : uint8_t
{
    NONE,

    PARSING_ERROR,
    READ_FILE_ZERO_BYTES_READ,
    UNKNOWN_CTB_TYPE,
};

// Describe the current errno on Unix or GetLastError() on Windows.
std::string getErrorString();
std::string getErrorString(uint64_t bytesRead_, uint64_t bytesProcessed_);
std::string getErrorString(ErrorCategory errorCategory_);

class Manager
{
  protected:
    // Send already-framed bytes through the endpoint's pipe.
    virtual tl::expected<void, std::string> writeInternal(std::string_view buffer) const = 0;

  public:
    virtual ~Manager() = default;
    // Complete partial writes; the caller owns the descriptor or handle.
#ifndef _WIN32
    static tl::expected<void, std::string> writeAll(const int fd, const char *buffer, const uint64_t count);
#else
    static tl::expected<void, std::string> writeAll(void *handle, std::string_view buffer);
#endif

    static std::string getBufferWithType(CTB type);
    // Wire lengths and counts use native-endian uint32_t values.
    static void writeUInt32(std::string &buffer, uint32_t value);
    static void writeString(std::string &buffer, const std::string_view &str);
    // Paths include a trailing NUL for OS calls; the encoded length excludes it.
    static void writePath(std::string &buffer, const std::string_view &str);
    static void writeModuleDep(std::string &buffer, const ModuleDep &dep);
    static void writeHuDep(std::string &buffer, const HuDep &dep);
    static void writeHeaderFile(std::string &buffer, const HeaderFile &dep);
    static void writeVectorOfStrings(std::string &buffer, const std::vector<std::string_view> &strs);
    static void writeVectorOfModuleDep(std::string &buffer, const std::vector<ModuleDep> &deps);
    static void writeVectorOfHuDeps(std::string &buffer, const std::vector<HuDep> &deps);
    static void writeVectorOfHeaderFiles(std::string &buffer, const std::vector<HeaderFile> &headerFiles);

    // Parsing offsets use uint64_t independently of the wire field widths.
    static tl::expected<bool, std::string> readBool(std::string_view message, uint64_t &bytesRead);
    static tl::expected<uint8_t, std::string> readUInt8(std::string_view message, uint64_t &bytesRead);
    static tl::expected<uint32_t, std::string> readUInt32(std::string_view message, uint64_t &bytesRead);
    static tl::expected<std::string_view, std::string> readString(std::string_view message, uint64_t &bytesRead);

    // Returned string/path views borrow message. A path's view excludes its NUL,
    // but the parser consumes and validates that byte before returning.
    static tl::expected<std::string_view, std::string> readPath(std::string_view message, uint64_t &bytesRead);
};

template <typename T, typename... Args> constexpr T *construct_at(T *p, Args &&...args)
{
    return ::new (static_cast<void *>(p)) T(std::forward<Args>(args)...);
}

template <typename T> T &getInitializedObjectFromBuffer(char (&buffer)[320])
{
    T &t = reinterpret_cast<T &>(buffer);
    construct_at(&t);
    return t;
}

} // namespace P2978
#endif // MANAGER_HPP
