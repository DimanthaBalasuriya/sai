#pragma once
#include "engine/core/Math.hpp"
#include <vector>
#include <span>
#include <array>
#include <cstdint>
namespace Engine::Spatial {
struct Aabb {Vec3 minimum,maximum;};
struct SpatialRay {Vec3 origin,direction;};
struct Plane {Vec3 normal;float offset=0;};
// Main-thread dynamic fat-AABB tree. Proxies survive rotations; moves inside
// the fat bounds do not touch topology. Query scratch is retained after warmup.
class SpatialIndex {
public:
    using Proxy=std::int32_t;
    Proxy Insert(Aabb,std::uint32_t payload);
    bool Update(Proxy,Aabb);
    void Remove(Proxy);
    void SetPayload(Proxy proxy,std::uint32_t value){m_nodes[proxy].payload=value;}
    void Clear();
    void Query(Aabb,std::vector<std::uint32_t>&) const;
    void Query(SpatialRay,std::vector<std::uint32_t>&) const;
    void Query(std::span<const Plane>,std::vector<std::uint32_t>&) const;
    std::size_t Size() const noexcept{return m_size;}
    std::uint64_t Reinserts() const noexcept{return m_reinserts;}
private:
    struct Node {Aabb bounds;Proxy parent=-1,left=-1,right=-1;std::int32_t height=-1;std::uint32_t payload=0;};
    Proxy Allocate();void Free(Proxy);void Attach(Proxy);void Detach(Proxy);Proxy Balance(Proxy);void Fix(Proxy);
    std::vector<Node> m_nodes;mutable std::vector<Proxy> m_stack;
    Proxy m_root=-1,m_free=-1;std::size_t m_size=0;std::uint64_t m_reinserts=0;
};
}
