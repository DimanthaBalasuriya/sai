#pragma once
#include "engine/extensions/Features.hpp"
namespace Engine::Extensions {
// Module owns its sky feature; registry and device must outlive the module.
std::unique_ptr<IModule> MakeSkyboxModule(FeatureRegistry&,Rhi::Device&,const std::filesystem::path& shaders);
}
