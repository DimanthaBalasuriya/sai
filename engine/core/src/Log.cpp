#include "engine/core/Log.hpp"
#include <spdlog/spdlog.h>
#include <mutex>
namespace Engine::Log {
namespace { std::mutex mutex;std::vector<Message> messages;
void Remember(std::string_view message,Level level) { std::scoped_lock lock(mutex);if (messages.size()>=1000) messages.erase(messages.begin(),messages.begin()+100);messages.push_back({std::string(message),level}); }
}
void Info(std::string_view message) noexcept { try { Remember(message,Level::Info);spdlog::info("{}", message); } catch (...) {} }
void Error(std::string_view message) noexcept { try { Remember(message,Level::Error);spdlog::error("{}", message); } catch (...) {} }
void Warning(std::string_view message) noexcept { try { Remember(message,Level::Warning);spdlog::warn("{}", message); } catch (...) {} }
void Success(std::string_view message) noexcept { try { Remember(message,Level::Success);spdlog::info("{}", message); } catch (...) {} }
void Flush() noexcept { try { spdlog::default_logger()->flush(); } catch (...) {} }
std::vector<std::string> Recent() { std::scoped_lock lock(mutex);std::vector<std::string> output;for(const auto& m:messages) output.push_back(m.text);return output; }
std::vector<Message> RecentDetailed() { std::scoped_lock lock(mutex);return messages; }
}
