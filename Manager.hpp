
#ifndef MANAGER_HPP
#define MANAGER_HPP

#include <string>
#include <vector>

using std::string, std::vector;

#define BUFFERSIZE 4096
#define PIPE_TIMEOUT 5000

class Manager
{
  public:
    void *hPipe = nullptr;
    void read(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead) const;
    void write(vector<char> &buffer) const;
    bool readBoolFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed);
    string readStringFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed);
    vector<string> readVectorOfStringFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead,
                                              uint64_t &bytesProcessed);
    vector<string> readVectorOfMaybeMappedFileFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead,
                                                                uint64_t &bytesProcessed);
    void readNumberOfBytes(char *output, uint64_t size, char (&buffer)[BUFFERSIZE], uint64_t &bytesRead,
                           uint64_t &bytesProcessed) const;
};

#endif // MANAGER_HPP
