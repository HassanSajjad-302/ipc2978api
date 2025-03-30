
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

    void read(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead) const;
    void write(const vector<char> &buffer) const;

    static vector<char> getBufferWithType(BTC type);
    static void writeString(vector<char> &buffer, const string &str);

    bool readBoolFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed) const;
    string readStringFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed) const;
    vector<string> readVectorOfStringFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead,
                                              uint64_t &bytesProcessed) const;
    vector<string> readVectorOfMaybeMappedFileFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead,
                                                       uint64_t &bytesProcessed) const;
    void readNumberOfBytes(char *output, uint64_t size, char (&buffer)[BUFFERSIZE], uint64_t &bytesRead,
                           uint64_t &bytesProcessed) const;
};

#endif // MANAGER_HPP
