#include "LightBufferCache.hpp"
#include <algorithm>
#include <cmath>
namespace Engine::Lighting {
namespace {
Renderer::GpuLight Packed(const Renderer::RenderLight& light){
    if(light.modulePacked)return light.gpu;
    Renderer::GpuLight gpu;gpu.position=light.position;gpu.direction=light.direction;gpu.type=light.type;gpu.invRangeSq=light.type?1/(light.range*light.range):0;
    // Legacy local authoring uses the same 1 -> 300 cd artist baseline.
    float intensity=light.intensity*(light.type?300.f:1.f);gpu.color={light.color.x*intensity,light.color.y*intensity,light.color.z*intensity};if(light.type==2){gpu.spotScale=1/std::max(1e-6f,std::cos(light.spotAngle*.8f)-std::cos(light.spotAngle));gpu.spotOffset=-std::cos(light.spotAngle)*gpu.spotScale;}gpu.flags=Renderer::kLightEnabled|(light.type?Renderer::kLightLegacyLocal:0);return gpu;
}
}
void LightBufferCache::Dirty(std::uint32_t index){for(std::size_t slot=0;slot<Rhi::kFramesInFlight;++slot)if(!m_flags[slot][index]){m_flags[slot][index]=1;m_pending[slot].push_back(index);}}
Result LightBufferCache::Update(const Renderer::RenderWorld& world){
    if(m_valid&&!world.modified&&m_source==world.source&&m_revision==world.revision)return Result::Success();
    bool full=!m_valid||world.modified||m_source!=world.source||world.fullUpdate||m_revision!=world.baseRevision||m_mapping.size()!=world.lights.size();
    if(!full)for(auto index:world.changedLights){if(index>=world.lights.size())return Result::Failure("Invalid dirty light index");if(world.lights[index].type!=m_types[index]){full=true;break;}}
    if(full){
        std::uint32_t directional=0,local=0;for(const auto& light:world.lights){if(light.type==0)++directional;else ++local;}
        if(directional>kDirectionalCapacity||local>kLocalCapacity)return Result::Failure("Lighting capacity exceeded: maximum 4 directional / 4096 local lights");
        m_directional=directional;m_local=local;m_lights.assign(kDirectionalCapacity+local,{});m_mapping.resize(world.lights.size());m_types.resize(world.lights.size());
        for(std::size_t slot=0;slot<Rhi::kFramesInFlight;++slot){m_pending[slot].clear();m_pending[slot].reserve(kCapacity);m_flags[slot].assign(m_lights.size(),0);}
        directional=0;local=kDirectionalCapacity;
        for(std::size_t i=0;i<world.lights.size();++i){auto target=world.lights[i].type==0?directional++:local++;m_mapping[i]=target;m_types[i]=world.lights[i].type;m_lights[target]=Packed(world.lights[i]);++m_packCount;}
        for(std::uint32_t i=0;i<m_lights.size();++i)Dirty(i);
        ++m_rebuilds;
    }else for(auto index:world.changedLights){auto target=m_mapping[index];m_lights[target]=Packed(world.lights[index]);Dirty(target);++m_packCount;}
    m_valid=true;m_source=world.source;m_revision=world.revision;return Result::Success();
}
void LightBufferCache::Complete(std::size_t slot) noexcept {for(auto index:m_pending[slot])m_flags[slot][index]=0;m_pending[slot].clear();}
}
