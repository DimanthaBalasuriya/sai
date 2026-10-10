#pragma once
#include "engine/core/IModule.hpp"
#include "engine/scene/World.hpp"
#include "engine/scene/EntityFeatures.hpp"
#include <exception>
namespace Engine::Scene {
// Registry-owned scene service; clients borrow World and finish before shutdown.
class SceneModule final : public IModule {
public:
    const char* Name() const noexcept override { return "Scene"; }
    World& GetWorld() noexcept { return m_world; }
    Result OnStartup(EngineContext&) noexcept override {
        try{return RegisterEntityFeatures();}catch(const std::exception& error){return Result::Failure(error.what());}
    }
private: World m_world;
};
}
