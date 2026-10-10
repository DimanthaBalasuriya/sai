#include "engine/lighting/ShadowMath.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
namespace Engine::Lighting {
std::array<Cascade,4> BuildCascades(const Renderer::RenderCamera& camera,const Renderer::RenderLight& light,float aspect){
    std::array<Cascade,4> cascades;float near=camera.nearPlane,far=std::max(near+.001f,std::min(camera.farPlane,light.shadow.distance));
    auto transform=glm::make_mat4(camera.transform.values.data());glm::vec3 direction(light.direction.x,light.direction.y,light.direction.z);direction=glm::normalize(direction);
    auto basis=glm::lookAt(glm::vec3(0),direction,std::abs(direction.y)>.99f?glm::vec3(0,0,1):glm::vec3(0,1,0));float tangent=std::tan(camera.verticalFov*.5f);std::uint32_t count=std::clamp(light.shadow.cascades,1u,4u);float previous=near;
    for(std::uint32_t i=0;i<count;++i){float ratio=float(i+1)/count;float split=glm::mix(near+(far-near)*ratio,near*std::pow(far/near,ratio),light.shadow.split);
        // Analytic camera-space sphere: invariant under camera rotation.
        float a=tangent*tangent*(1+aspect*aspect);float center=(previous+split)*(1+a)*.5f;center=std::min(center,split);float radius=std::sqrt(a*split*split+(split-center)*(split-center));radius=std::ceil(radius*16)/16;
        glm::vec3 world=glm::vec3(transform*glm::vec4(0,0,-center,1));glm::vec3 snapped=glm::vec3(basis*glm::vec4(world,1));float texel=2*radius/std::max(128u,light.shadow.resolution);
        snapped.x=std::round(snapped.x/texel)*texel;snapped.y=std::round(snapped.y/texel)*texel;world=glm::vec3(glm::inverse(basis)*glm::vec4(snapped,1));
        auto view=glm::lookAt(world-direction*(radius+far),world,std::abs(direction.y)>.99f?glm::vec3(0,0,1):glm::vec3(0,1,0));auto projection=glm::orthoRH_ZO(-radius,radius,-radius,radius,2*(radius+far),.01f);projection[1][1]*=-1;
        cascades[i]={projection*view,split,radius,world};previous=split;
    }return cascades;
}
AtlasTile ShadowAtlasAllocator::Allocate(std::uint32_t resolution){
    std::uint32_t size=256;while(size<resolution&&size<2048)size*=2;std::uint32_t cells=size/256;
    for(std::uint32_t y=0;y+cells<=16;y+=cells)for(std::uint32_t x=0;x+cells<=16;x+=cells){bool free=true;for(std::uint32_t v=0;v<cells;++v)for(std::uint32_t u=0;u<cells;++u)free&=!m_cells[(y+v)*16+x+u];if(!free)continue;
        for(std::uint32_t v=0;v<cells;++v)for(std::uint32_t u=0;u<cells;++u)m_cells[(y+v)*16+x+u]=true;return {x*256,y*256,size};
    }return {};
}
}
