#pragma once
#include "engine/extensions/Features.hpp"
#include "engine/lighting/ShadowMath.hpp"
namespace Engine::Lighting {
struct ShadowRecord {std::array<glm::mat4,6> matrices{};std::array<glm::vec4,6> tiles{};glm::vec4 splits{},options{};glm::uvec4 meta{};};
static_assert(sizeof(ShadowRecord)==528);
class IShadowTechnique {
public:virtual ~IShadowTechnique()=default;virtual Result Declare(Renderer::RenderGraph&,Extensions::FeatureFrame&,Renderer::RenderWorld&)=0;
};
std::unique_ptr<IModule> MakeShadowModule(Extensions::FeatureRegistry&,Rhi::Device&,const std::filesystem::path&);
}
