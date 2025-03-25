
#ifndef MANAGER_HPP
#define MANAGER_HPP


#include <string>

using std::string;

#define BUFFERSIZE 4096
#define PIPE_TIMEOUT 5000

class Manager
{
  public:
    void *hPipe = nullptr;
    void read(char (&buffer)[BUFFERSIZE], uint64_t &bytesRead);
    void readStringFromPipe(string &output, char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed);
    void readNumberOfBytes(char *output, uint64_t size, char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed);
};

#endif // MANAGER_HPP
