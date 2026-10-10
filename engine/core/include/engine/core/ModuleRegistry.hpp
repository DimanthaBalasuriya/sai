#pragma once
#include <memory>
#include <vector>
#include "engine/core/IModule.hpp"
namespace Engine {
// Owns modules; main-thread only. Context must outlive this registry.
class ModuleRegistry {
public:
    explicit ModuleRegistry(EngineContext& context) : m_context(context) {}
    ~ModuleRegistry();
    ModuleRegistry(const ModuleRegistry&) = delete;
    ModuleRegistry& operator=(const ModuleRegistry&) = delete;
    Result Add(std::unique_ptr<IModule> module);
    Result Startup();
    void Shutdown() noexcept;
private:
    EngineContext& m_context;
    std::vector<std::unique_ptr<IModule>> m_modules;
    std::vector<IModule*> m_started;
    bool m_attempted = false;
};
}
