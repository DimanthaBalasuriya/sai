#pragma once
#include <string>
#include <utility>

namespace Engine {
// Value-owned error result; safe to transfer between threads by value.
class Result {
public:
    static Result Success() { return Result(true, {}); }
    static Result Failure(std::string message) { return Result(false, std::move(message)); }
    explicit operator bool() const noexcept { return m_success; }
    const std::string& Error() const noexcept { return m_error; }
private:
    Result(bool success, std::string message) : m_success(success), m_error(std::move(message)) {}
    bool m_success;
    std::string m_error;
};
}
