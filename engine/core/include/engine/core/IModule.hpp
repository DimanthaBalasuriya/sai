#pragma once
#include <span>
#include <string_view>
#include "engine/core/Result.hpp"
namespace Engine {
struct EngineContext;
// Registry owns modules. Lifecycle callbacks run on the main thread and must not throw.
class IModule {
public:
    virtual ~IModule() = default;
    virtual const char* Name() const noexcept = 0;
    virtual std::span<const std::string_view> Dependencies() const noexcept { return {}; }
    virtual Result OnRegister(EngineContext&) noexcept { return Result::Success(); }
    virtual Result OnStartup(EngineContext&) noexcept { return Result::Success(); }
    virtual void OnShutdown(EngineContext&) noexcept {}
};
}
