#pragma once
#include "engine/extensions/Features.hpp"
namespace Engine::Extensions {
// Application registration list; factories are linked only for enabled modules.
Result RegisterBuiltins(FeaturePipeline&,Rhi::Device&,const std::filesystem::path& shaders);
}
