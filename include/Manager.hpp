
#ifndef MANAGER_HPP
#define MANAGER_HPP

#include "Messages.hpp"
#include "expected.hpp"

#include <string>
#include <vector>

namespace tl
{
template <typename T, typename U> class expected;
}

namespace P2978
{

// 32-byte delimiter
inline const char *delimiter = "DELIMITER"
                               "\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5"
                               "DELIMITER";

enum class ErrorCategory : uint8_t
{
    NONE,

    PARSING_ERROR,
    // error-category for API errors
    READ_FILE_ZERO_BYTES_READ,
    UNKNOWN_CTB_TYPE,
};

std::string getErrorString();
std::string getErrorString(uint64_t bytesRead_, uint64_t bytesProcessed_);
std::string getErrorString(ErrorCategory errorCategory_);
// to facilitate error propagation.
inline std::string getErrorString(std::string err)
{
    return err;
}

class Manager
{
  public:
    virtual tl::expected<void, std::string> writeInternal(std::string_view buffer) const = 0;
    virtual ~Manager() = default;
#ifndef _WIN32
    static tl::expected<void, std::string> writeAll(const int fd, const char *buffer, const uint64_t count);
#else
    static tl::expected<void, std::string> writeAll(void *handle, std::string_view buffer);
#endif

    static std::string getBufferWithType(CTB type);
    static void writeUInt32(std::string &buffer, uint32_t value);
    static void writeString(std::string &buffer, const std::string_view &str);
    // path is used in system calls. so it is followed by null character while the normal string is not.
    static void writePath(std::string &buffer, const std::string_view &str);
    static void writeBMIFile(std::string &buffer, const BMIFile &file);
    static void writeModuleDep(std::string &buffer, const ModuleDep &dep);
    static void writeHuDep(std::string &buffer, const HuDep &dep);
    static void writeHeaderFile(std::string &buffer, const HeaderFile &dep);
    static void writeVectorOfStrings(std::string &buffer, const std::vector<std::string_view> &strs);
    static void writeVectorOfModuleDep(std::string &buffer, const std::vector<ModuleDep> &deps);
    static void writeVectorOfHuDeps(std::string &buffer, const std::vector<HuDep> &deps);
    static void writeVectorOfHeaderFiles(std::string &buffer, const std::vector<HeaderFile> &headerFiles);

    static tl::expected<bool, std::string> readBool(std::string_view message, uint64_t &bytesRead);
    static tl::expected<uint8_t, std::string> readUInt8(std::string_view message, uint64_t &bytesRead);
    static tl::expected<uint32_t, std::string> readUInt32(std::string_view message, uint64_t &bytesRead);
    static tl::expected<std::string_view, std::string> readString(std::string_view message, uint64_t &bytesRead);

    // path is used in system calls. so it is followed by null character while the normal string is not.
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
