#include "IPCManagerBS.hpp"

#include <chrono>
#include <print>
#include <random>
#include <string>
#include <thread>

using std::print;

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
    IPCManagerBS manager("test");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    char buffer[320];
    CTB type;
    manager.receiveMessage(buffer, type);

    switch (type)
    {
    case CTB::MODULE: {
        const auto &[moduleName] = reinterpret_cast<CTBModule &>(buffer);
        print("Received a module request with logical-name {}\n", moduleName);
    }
    }
}
