#include "engine/core/ModuleRegistry.hpp"
#include <functional>
#include <string>
#include <unordered_map>

namespace Engine {
ModuleRegistry::~ModuleRegistry() { Shutdown(); }
Result ModuleRegistry::Add(std::unique_ptr<IModule> module) {
    if (m_attempted) return Result::Failure("Cannot add modules after startup begins");
    if (!module || !module->Name() || std::string_view(module->Name()).empty())
        return Result::Failure("Module needs a non-empty name");
    for (const auto& existing : m_modules)
        if (std::string_view(existing->Name()) == module->Name())
            return Result::Failure("Duplicate module: " + std::string(module->Name()));
    m_modules.push_back(std::move(module));
    return Result::Success();
}
Result ModuleRegistry::Startup() {
    if (m_attempted) return Result::Failure("Registry startup can only be attempted once");
    m_attempted = true;
    std::unordered_map<std::string_view, std::size_t> indices;
    for (std::size_t i = 0; i < m_modules.size(); ++i) indices.emplace(m_modules[i]->Name(), i);
    std::vector<int> state(m_modules.size(), 0);
    std::vector<IModule*> order;
    std::function<Result(std::size_t)> visit = [&](std::size_t index) -> Result {
        if (state[index] == 1) return Result::Failure("Module dependency cycle at " + std::string(m_modules[index]->Name()));
        if (state[index] == 2) return Result::Success();
        state[index] = 1;
        for (auto dependency : m_modules[index]->Dependencies()) {
            auto found = indices.find(dependency);
            if (found == indices.end()) return Result::Failure("Missing dependency: " + std::string(dependency));
            auto result = visit(found->second);
            if (!result) return result;
        }
        state[index] = 2;
        order.push_back(m_modules[index].get());
        return Result::Success();
    };
    for (std::size_t i = 0; i < m_modules.size(); ++i) {
        auto result = visit(i);
        if (!result) return result;
    }
    for (auto* module : order) {
        auto result = module->OnRegister(m_context);
        if (!result) return Result::Failure(std::string(module->Name()) + " registration: " + result.Error());
    }
    for (auto* module : order) {
        auto result = module->OnStartup(m_context);
        if (!result) {
            module->OnShutdown(m_context); // clean partial startup before dependencies
            Shutdown();
            return Result::Failure(std::string(module->Name()) + " startup: " + result.Error());
        }
        m_started.push_back(module);
    }
    return Result::Success();
}
void ModuleRegistry::Shutdown() noexcept {
    for (auto it = m_started.rbegin(); it != m_started.rend(); ++it) (*it)->OnShutdown(m_context);
    m_started.clear();
}
}
