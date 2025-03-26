
#ifndef MANAGER_HPP
#define MANAGER_HPP

#include <string>
#include <vector>

using std::string, std::vector;

#define BUFFERSIZE 4096
#define PIPE_TIMEOUT 5000

struct MaybeMappedFile;

class Manager
{
  public:
    void *hPipe = nullptr;
    void read(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead) const;
    bool readBoolFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed);
    string readStringFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed);
    MaybeMappedFile readMaybeMappedFileFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed);
    vector<string> readVectorOfStringFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead,
                                              uint64_t &bytesProcessed);
    vector<MaybeMappedFile> readVectorOfMaybeMappedFileFromPipe(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead,
                                                                uint64_t &bytesProcessed);
    void readNumberOfBytes(char *output, uint64_t size, char (&buffer)[BUFFERSIZE], uint64_t &bytesRead,
                           uint64_t &bytesProcessed);
};

#endif // MANAGER_HPP
