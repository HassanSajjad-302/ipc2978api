
#include "IPCManagerCompiler.hpp"
#include "Testing.hpp"

#include <chrono>
#include <print>
#include <random>
#include <string>
#include <thread>

int main()
{
    std::uniform_int_distribution<> distribution(0, 50);
    uint64_t randomNumber = distribution(generator);

    IPCManagerCompiler manager("test");
    manager.sendMessage(CTBModule{.moduleName = generateRandomString()});
    BTCModule requestedFile = manager.receiveMessage<BTCModule>();
}
