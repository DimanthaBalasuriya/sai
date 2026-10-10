#pragma once
#include "engine/extensions/Features.hpp"
namespace Engine::Extensions {
// Factories return module ownership. Bloom precedes tonemap in the registry.
std::unique_ptr<IModule> MakeBloomModule(FeatureRegistry&,Rhi::Device&,const std::filesystem::path& shaders);
std::unique_ptr<IModule> MakeTonemapModule(FeatureRegistry&,Rhi::Device&,const std::filesystem::path& shaders);
}
