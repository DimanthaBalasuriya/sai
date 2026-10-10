#pragma once
#include "engine/lighting/Lighting.hpp"
#include "engine/extensions/Features.hpp"
namespace Engine::Lighting {
inline constexpr Scene::ComponentType kEnvironmentSettings=static_cast<Scene::ComponentType>(0x4c490010);
inline constexpr Scene::ComponentType kFogSettings=static_cast<Scene::ComponentType>(0x4c490011);
inline constexpr Scene::ComponentType kExposureSettings=static_cast<Scene::ComponentType>(0x4c490012);
struct IEnvironmentLighting {virtual ~IEnvironmentLighting()=default;virtual Result Evaluate(Renderer::RenderGraph&,Extensions::FeatureFrame&,Renderer::RenderWorld&)=0;};
struct ISceneFogProvider {virtual ~ISceneFogProvider()=default;virtual Result Composite(Renderer::RenderGraph&,Extensions::FeatureFrame&)=0;};
Result RegisterSettings();
std::unique_ptr<IModule> MakeEnvironmentModule(Extensions::FeatureRegistry&,Rhi::Device&,const std::filesystem::path&);
std::unique_ptr<IModule> MakeFogModule(Extensions::FeatureRegistry&,Rhi::Device&,const std::filesystem::path&);
std::unique_ptr<IModule> MakeExposureModule(Extensions::FeatureRegistry&,Rhi::Device&,const std::filesystem::path&);
}
