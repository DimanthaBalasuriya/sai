#include "engine/core/EngineContext.hpp"
#include "engine/core/ModuleRegistry.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace {
class TestModule final : public Engine::IModule {
public:
    TestModule(std::string name, std::vector<std::string>& events,
               std::vector<std::string_view> dependencies = {}, bool failStart = false, bool failRegister = false)
        : m_name(std::move(name)), m_events(events), m_dependencies(std::move(dependencies)),
          m_failStart(failStart), m_failRegister(failRegister) {}
    const char* Name() const noexcept override { return m_name.c_str(); }
    std::span<const std::string_view> Dependencies() const noexcept override { return m_dependencies; }
    Engine::Result OnRegister(Engine::EngineContext&) noexcept override {
        m_events.push_back("register " + m_name);
        return m_failRegister ? Engine::Result::Failure("registration failure") : Engine::Result::Success();
    }
    Engine::Result OnStartup(Engine::EngineContext&) noexcept override {
        m_events.push_back("start " + m_name);
        return m_failStart ? Engine::Result::Failure("startup failure") : Engine::Result::Success();
    }
    void OnShutdown(Engine::EngineContext&) noexcept override { m_events.push_back("stop " + m_name); }
private:
    std::string m_name;
    std::vector<std::string>& m_events;
    std::vector<std::string_view> m_dependencies;
    bool m_failStart, m_failRegister;
};
int failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
void Ordering() {
    Engine::EngineContext context;
    std::vector<std::string> events;
    Engine::ModuleRegistry registry(context);
    Check(bool(registry.Add(std::make_unique<TestModule>("Client", events, std::vector<std::string_view>{"Base"}))), "add client");
    Check(bool(registry.Add(std::make_unique<TestModule>("Base", events))), "add base");
    Check(bool(registry.Startup()), "out-of-order dependencies resolve");
    Check(!registry.Startup(), "duplicate startup rejected");
    Check(!registry.Add(std::make_unique<TestModule>("Late", events)), "late module rejected");
    registry.Shutdown();
    registry.Shutdown();
    Check(events == std::vector<std::string>{"register Base", "register Client", "start Base", "start Client", "stop Client", "stop Base"}, "ordered lifecycle and idempotent shutdown");
}
void InvalidGraph(bool cycle) {
    Engine::EngineContext context;
    std::vector<std::string> events;
    Engine::ModuleRegistry registry(context);
    Check(bool(registry.Add(std::make_unique<TestModule>("A", events, std::vector<std::string_view>{"B"}))), "add A");
    if (cycle) Check(bool(registry.Add(std::make_unique<TestModule>("B", events, std::vector<std::string_view>{"A"}))), "add B");
    auto result = registry.Startup();
    Check(!result, "invalid graph rejected");
    Check(!result.Error().empty(), "invalid graph reports reason");
    Check(events.empty(), "graph validated before callbacks");
}
void FailureCleanup(bool registration) {
    Engine::EngineContext context;
    std::vector<std::string> events;
    {
        Engine::ModuleRegistry registry(context);
        Check(bool(registry.Add(std::make_unique<TestModule>("Base", events))), "add failure dependency");
        Check(bool(registry.Add(std::make_unique<TestModule>("Failure", events, std::vector<std::string_view>{"Base"}, !registration, registration))), "add failing module");
        Check(!registry.Startup(), "failed callback returned");
    }
    if (registration) Check(events == std::vector<std::string>{"register Base", "register Failure"}, "registration failure never starts resources");
    else Check(events == std::vector<std::string>{"register Base", "register Failure", "start Base", "start Failure", "stop Failure", "stop Base"}, "partial startup unwinds once");
}
void OwnershipAndNames() {
    Engine::EngineContext context;
    std::vector<std::string> events;
    {
        Engine::ModuleRegistry registry(context);
        Check(!registry.Add(nullptr), "null rejected");
        Check(!registry.Add(std::make_unique<TestModule>("", events)), "empty name rejected");
        Check(bool(registry.Add(std::make_unique<TestModule>("Base", events))), "named module accepted");
        Check(!registry.Add(std::make_unique<TestModule>("Base", events)), "duplicate name rejected");
        Check(bool(registry.Startup()), "start for destructor test");
    }
    Check(events == std::vector<std::string>{"register Base", "start Base", "stop Base"}, "destructor shuts down resources");
}
}
int main() {
    Ordering(); InvalidGraph(false); InvalidGraph(true); FailureCleanup(false); FailureCleanup(true); OwnershipAndNames();
    return failures == 0 ? 0 : 1;
}
