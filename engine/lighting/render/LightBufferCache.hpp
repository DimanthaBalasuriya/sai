#pragma once
#include "engine/renderer/RenderWorld.hpp"
#include <array>
#include <span>
#include "engine/core/Result.hpp"
namespace Engine::Lighting {
// CPU upload bookkeeping only. Retains capacity and tracks each fenced frame slot.
class LightBufferCache {
public:
    static constexpr std::uint32_t kDirectionalCapacity=4,kLocalCapacity=4096,kCapacity=4100;
    Result Update(const Renderer::RenderWorld&);
    std::span<const Renderer::GpuLight> Lights() const noexcept {return m_lights;}
    std::span<const std::uint32_t> Pending(std::size_t slot) const noexcept{return m_pending[slot];}
    void Complete(std::size_t slot) noexcept;
    std::uint32_t DirectionalCount() const noexcept{return m_directional;}
    std::uint32_t LocalCount() const noexcept{return m_local;}
    std::uint64_t Rebuilds() const noexcept{return m_rebuilds;}
    std::uint64_t PackCount() const noexcept{return m_packCount;}
private:
    void Dirty(std::uint32_t);
    std::vector<Renderer::GpuLight> m_lights;
    std::vector<std::uint32_t> m_mapping,m_types;
    std::array<std::vector<std::uint32_t>,Rhi::kFramesInFlight> m_pending;
    std::array<std::vector<std::uint8_t>,Rhi::kFramesInFlight> m_flags;
    bool m_valid=false;Guid m_source;std::uint64_t m_revision=0,m_rebuilds=0,m_packCount=0;
    std::uint32_t m_directional=0,m_local=0;
};
}
