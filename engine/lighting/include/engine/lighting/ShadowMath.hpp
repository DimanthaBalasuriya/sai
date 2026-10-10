#pragma once
#include "engine/renderer/RenderWorld.hpp"
#include <glm/glm.hpp>
#include <array>
namespace Engine::Lighting {
struct Cascade {glm::mat4 matrix{1};float split=0,radius=0;glm::vec3 center{};};
std::array<Cascade,4> BuildCascades(const Renderer::RenderCamera&,const Renderer::RenderLight&,float aspect);
struct AtlasTile {std::uint32_t x=0,y=0,size=0;explicit operator bool()const{return size!=0;}};
class ShadowAtlasAllocator {
public:
    void Clear()noexcept{m_cells.fill(false);}
    AtlasTile Allocate(std::uint32_t resolution);
private:std::array<bool,256> m_cells{};
};
}
