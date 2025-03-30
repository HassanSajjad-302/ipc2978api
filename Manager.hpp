
#ifndef MANAGER_HPP
#define MANAGER_HPP

#include "BufferSize.hpp"
#include "Messages.hpp"
#include <string>
#include <vector>
using std::string, std::vector;

class Manager
{
  public:
    void *hPipe = nullptr;

    void read(char (&buffer)[BUFFERSIZE], uint32_t &bytesRead) const;
    void write(const vector<char> &buffer) const;

    static vector<char> getBufferWithType(CTB type);
    static vector<char> getBufferWithType(BTC type);
    static void writeString(vector<char> &buffer, const string &str);
    static void writeVectorOfStrings(vector<char> &buffer, const vector<string> &strs);

    bool readBoolFromPipe(char (&buffer)[4096], uint32_t &bytesRead, uint32_t &bytesProcessed) const;
    string readStringFromPipe(char (&buffer)[4096], uint32_t &bytesRead, uint32_t &bytesProcessed) const;
    vector<string> readVectorOfStringFromPipe(char (&buffer)[4096], uint32_t &bytesRead, uint32_t &bytesProcessed) const;
    vector<string> readVectorOfMaybeMappedFileFromPipe(char (&buffer)[4096], uint32_t &bytesRead,
                                                       uint32_t &bytesProcessed) const;
    void readNumberOfBytes(char *output, uint32_t size, char (&buffer)[4096], uint32_t &bytesRead,
                           uint32_t &bytesProcessed) const;
};

template <typename T> T &getInitializedObjectFromBuffer(char (&buffer)[320])
{
    T &t = reinterpret_cast<T &>(buffer);
    std::construct_at(&t);
    return t;
}

#endif // MANAGER_HPP
