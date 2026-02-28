
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

void exitFailure(const string &str);
string fileToString(string_view file_name);
string getRandomString(uint32_t length = 0);
bool getRandomBool();
uint32_t getRandomNumber(uint32_t max);
BTCModule getBTCModule(const CTBModule &ctbModule);
BTCNonModule getBTCNonModule(const CTBNonModule &nonModule);
void printSendingOrReceiving(bool sent);
void printMessage(const CTBModule &ctbModule, bool sent);
void printMessage(const CTBNonModule &nonModule, bool sent);
void printMessage(const CTBLastMessage &lastMessage, bool sent);
void printMessage(const BTCModule &btcModule, bool sent);
void printMessage(const BTCNonModule &nonModule, bool sent);
void printMessage(const BTCLastMessage &lastMessage, bool sent);

struct TestResponse
{
    string filePath;
    string fileContent;
    FileType type;
    bool isSystem;
    TestResponse(string filePath_, string fileContent_, FileType fileType_, bool isSystem_);
};

inline std::map<string_view, TestResponse> tempTestFiles;
inline vector<string *> buildTestallocations;

#endif // TESTING_HPP
