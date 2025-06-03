
#ifndef MANAGER_HPP
#define MANAGER_HPP

#include "BufferSize.hpp"
#include "Messages.hpp"
#include <string>
#include <vector>
using std::string, std::vector;

namespace N2978
{

class Manager
{
  public:
    void *hPipe = nullptr;

    void read(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead) const;
    void write(const vector<char> &buffer) const;

    static vector<char> getBufferWithType(CTB type);
    static void writeUInt32(vector<char> &buffer, uint32_t value);
    static void writeString(vector<char> &buffer, const string &str);
    static void writeMemoryMappedBMIFile(vector<char> &buffer, const BMIFile &file);
    static void writeVectorOfStrings(vector<char> &buffer, const vector<string> &strs);
    static void writeVectorOfMemoryMappedBMIFiles(vector<char> &buffer, const vector<BMIFile> &files);

    bool readBoolFromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead, uint32_t &bytesProcessed) const;
    uint32_t readUInt32FromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead, uint32_t &bytesProcessed) const;
    string readStringFromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead, uint32_t &bytesProcessed) const;
    BMIFile readMemoryMappedBMIFileFromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead,
                                            uint32_t &bytesProcessed) const;
    vector<string> readVectorOfStringFromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead,
                                              uint32_t &bytesProcessed) const;
    vector<BMIFile> readVectorOfMemoryMappedBMIFilesFromPipe(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead,
                                                             uint32_t &bytesProcessed) const;
    void readNumberOfBytes(char *output, uint32_t size, char (&buffer)[BUFFERSIZE], uint32_t &bytesRead,
                           uint32_t &bytesProcessed) const;
};

template <typename T, typename... Args>
constexpr T* construct_at(T* p, Args&&... args) {
  return ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
}

template <typename T> T &getInitializedObjectFromBuffer(char (&buffer)[320])
{
    T &t = reinterpret_cast<T &>(buffer);
    construct_at(&t);
    return t;
}
} // namespace N2978
#endif // MANAGER_HPP
