
#ifndef TESTING_HPP
#define TESTING_HPP

#include "IPCManagerCompiler.hpp"
#include "Messages.hpp"
#include <map>
#include <random>
#include <string>

using std::string, std::string_view, std::vector, std::map;
using namespace P2978;

inline std::random_device rd;
inline int randomSeed = rd();
inline std::mt19937 generator(randomSeed);

[[noreturn]] void exitFailure(const string &str);
string fileToString(string_view file_name);
string getRandomString(uint64_t length = 0);
bool getRandomBool();
uint64_t getRandomNumber(uint64_t max);
BTCModule getBTCModule(const CTBModule &ctbModule);
BTCNonModule getBTCNonModule(const CTBNonModule &nonModule);
std::string_view fileTypeToString(FileType type);
void appendResponse(std::string &output, std::string_view path, std::string_view contents, FileType type,
                    bool isSystem);
void printSendingOrReceiving(bool sent);
void printMessage(const CTBModule &ctbModule, bool sent);
void printMessage(const CTBNonModule &nonModule, bool sent);
void printMessage(const BTCModule &btcModule, bool sent);
void printMessage(const BTCNonModule &nonModule, bool sent);

struct TestResponse
{
    string filePath;
    string fileContent;
    FileType type;
    bool isSystem;
    TestResponse(string filePath_, string fileContent_, FileType fileType_, bool isSystem_);
};

inline std::map<string, TestResponse> tempTestFiles;

#endif // TESTING_HPP
