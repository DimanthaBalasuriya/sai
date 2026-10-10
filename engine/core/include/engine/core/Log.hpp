#pragma once
#include <string_view>
#include <vector>
#include <string>
namespace Engine::Log {
// Process-wide logging is the allowed singleton exception; calls are thread-safe.
void Info(std::string_view message) noexcept;
void Error(std::string_view message) noexcept;
void Flush() noexcept;
std::vector<std::string> Recent(); // Owned snapshot for the editor console.
enum class Level { Info,Warning,Error,Success };
struct Message {std::string text;Level level;};
void Warning(std::string_view message) noexcept;
void Success(std::string_view message) noexcept;
std::vector<Message> RecentDetailed();
}
