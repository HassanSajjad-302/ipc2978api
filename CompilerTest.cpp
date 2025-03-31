
#include "IPCManagerCompiler.hpp"

#include <chrono>
#include <print>
#include <random>
#include <string>
#include <thread>

string generate_random_string(const int length)
{
    const string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);

    string random_string;
    for (int i = 0; i < length; ++i)
    {
        random_string += characters[distribution(generator)];
    }

    return random_string;
}

int main()
{
    IPCManagerCompiler manager("test");
    manager.sendMessage(CTBModule{.moduleName = generate_random_string(50)});

}
