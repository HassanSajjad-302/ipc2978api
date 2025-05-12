
#ifndef TESTING_HPP
#define TESTING_HPP

#include "Messages.hpp"
#include <print>
#include <random>

using namespace N2978;
using std::print;

inline std::random_device rd;
inline std::mt19937 generator(rd());

string getRandomString();
bool getRandomBool();
void printSendingOrReceiving(bool sent);
void printMessage(const CTBModule &ctbModule, bool sent);
void printMessage(const CTBNonModule &nonModule, bool sent);
void printMessage(const CTBLastMessage &lastMessage, bool sent);
void printMessage(const BTCModule &btcModule, bool sent);
void printMessage(const BTCNonModule &nonModule, bool sent);
void printMessage(const BTCLastMessage &lastMessage, bool sent);

#endif // TESTING_HPP
