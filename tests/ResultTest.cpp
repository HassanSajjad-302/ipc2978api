#include "Result.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

using P2978::Error;
using P2978::Result;

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void checkValues()
{
    const Result<bool> falseValue = false;
    require(falseValue && !*falseValue, "A successful false value was treated as an error");

    const std::string text(128, 'a');
    const Result<std::string> value = text;
    require(value && *value == text && value->size() == text.size(), "Const success access failed");
    Result<std::string> copy = value;
    copy->push_back('b');
    require(*value == text && copy->size() == text.size() + 1, "Copying shared success storage");

    const Result<std::string> failure = Error{text};
    require(!failure && failure.error() == text, "String error was treated as a successful string");
    copy = failure;
    copy.error().push_back('c');
    require(!copy && failure.error() == text, "Copying shared error storage");
    Result<std::string> moved = std::move(copy);
    require(!moved && moved.error() == text + 'c', "Moving an error lost its message");
    moved = value;
    require(moved && *moved == text, "Copy assignment from error to success failed");
    std::string extracted = *std::move(moved);
    require(extracted == text, "Moving the success value out failed");

    const Result<std::string> emptyValue = std::string{};
    const Result<std::string> emptyError = Error{std::string{}};
    require(emptyValue && emptyValue->empty() && !emptyError && emptyError.error().empty(),
            "Empty strings do not distinguish success from failure");
}

void checkVoid()
{
    Result<void> success = {};
    require(static_cast<bool>(success), "Default void result was not successful");
    const Result<void> failure = Error{"failure"};
    success = failure;
    require(!success && success.error() == "failure", "Void copy assignment lost its error");
    success.error() += " detail";
    require(failure.error() == "failure", "Void results shared error storage after copying");
    Result<void> moved = std::move(success);
    require(!moved && moved.error() == "failure detail", "Moving a void failure lost its error");
    moved = Result<void>{};
    require(static_cast<bool>(moved), "Void assignment did not clear the error state");
    moved = Error{std::string{}};
    require(!moved && moved.error().empty(), "An empty void error was treated as success");
}

struct Lifetime
{
    int live = 0;
    int destroyed = 0;
};

struct Resource
{
    Lifetime &lifetime;
    explicit Resource(Lifetime &lifetime_) : lifetime(lifetime_)
    {
        ++lifetime.live;
    }
    ~Resource()
    {
        --lifetime.live;
        ++lifetime.destroyed;
    }
};

void checkOwnership()
{
    using Owner = std::unique_ptr<Resource>;
    static_assert(!std::is_copy_constructible_v<Result<Owner>>);
    static_assert(!std::is_copy_assignable_v<Result<Owner>>);
    static_assert(!std::is_constructible_v<Result<Owner>, const Owner &>);
    static_assert(std::is_constructible_v<Result<Owner>, Owner &&>);
    static_assert(std::is_move_constructible_v<Result<Owner>>);
    static_assert(std::is_move_assignable_v<Result<Owner>>);
    static_assert(std::is_same_v<decltype(*std::declval<const Result<Owner> &>()), const Owner &>);
    static_assert(std::is_same_v<decltype(std::declval<const Result<Owner> &>().operator->()), const Owner *>);
    static_assert(std::is_same_v<decltype(*std::declval<Result<Owner> &&>()), Owner &&>);
    static_assert(std::is_same_v<decltype(std::declval<const Result<void> &>().error()), const std::string &>);

    Lifetime lifetime;
    {
        Result<Owner> first = std::make_unique<Resource>(lifetime);
        const auto *resource = first->get();
        Result<Owner> second = std::move(first);
        require(second && second->get() == resource && lifetime.live == 1 && lifetime.destroyed == 0,
                "Move construction did not transfer unique ownership");
        second = Error{"released"};
        require(!second && lifetime.live == 0 && lifetime.destroyed == 1,
                "Changing success to error did not destroy the owned resource once");
        second = std::make_unique<Resource>(lifetime);
        require(second && lifetime.live == 1, "Changing error to success did not acquire ownership");
        first = std::make_unique<Resource>(lifetime);
        resource = first->get();
        second = std::move(first);
        require(second && second->get() == resource && lifetime.live == 1 && lifetime.destroyed == 2,
                "Move assignment did not release the previous resource and transfer ownership");
    }
    require(lifetime.live == 0 && lifetime.destroyed == 3, "Result destruction leaked or duplicated ownership");
}
} // namespace

int main()
{
    checkValues();
    checkVoid();
    checkOwnership();
}
