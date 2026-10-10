#pragma once
#include "engine/core/IModule.hpp"
#include "engine/assets/Registry.hpp"
namespace Engine::Assets {
// Registry owns the module; the module owns its catalog. Main-thread only.
class AssetsModule final : public IModule {
public:
    explicit AssetsModule(std::filesystem::path root):m_registry(std::move(root)){}
    const char* Name() const noexcept override { return "Assets"; }
    Result OnStartup(EngineContext&) noexcept override { return m_registry.Open(); }
    Registry& GetRegistry() noexcept { return m_registry; }
private: Registry m_registry;
};
}
