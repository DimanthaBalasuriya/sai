#pragma once
#include "engine/core/IModule.hpp"
namespace Engine {
// Owns GLFW initialization; main-thread only. Destroy windows before shutdown.
class PlatformModule final : public IModule {
public:
    const char* Name() const noexcept override { return "Platform"; }
    Result OnStartup(EngineContext&) noexcept override;
    void OnShutdown(EngineContext&) noexcept override;
private:
    bool m_initialized = false;
};
}
