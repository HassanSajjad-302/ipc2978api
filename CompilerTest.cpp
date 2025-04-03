
#include "IPCManagerCompiler.hpp"

#include <chrono>
#include <print>
#include <random>
#include <string>
#include <thread>

string generateRandomString(const int length)
{
    const string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);

    string randomString(length, 'P');
    /*
    for (int i = 0; i < length; ++i)
    {
        randomString[i] = 'P';// characters[distribution(generator)];
    }
    */

    return randomString;
}

int main()
{
    IPCManagerCompiler manager("test");
    manager.sendMessage(CTBModule{.moduleName = generateRandomString(60)});
    BTCModule requestedFile = manager.receiveMessage<BTCModule>();
}
