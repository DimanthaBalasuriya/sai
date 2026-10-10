#pragma once
#include "engine/scene/World.hpp"
#include <unordered_map>
#include <unordered_set>
namespace EditorTools {
struct GuidHash {std::size_t operator()(Engine::Guid id)const noexcept{return std::hash<std::uint64_t>{}(id.high)^(std::hash<std::uint64_t>{}(id.low)<<1);}};
class HierarchyModel {
public:
    Engine::Result Refresh(const Engine::Scene::World&);
    void Filter(std::string_view);
    void SetOpen(Engine::Guid,bool);
    bool Open(Engine::Guid id)const{return m_open.contains(id);}
    void Reveal(Engine::Guid);
    std::span<const std::uint32_t> Rows();
    const Engine::Scene::HierarchyEntry& Entry(std::uint32_t index)const{return m_entries[index];}
    std::size_t Rebuilds()const{return m_rebuilds;}
private:
    std::vector<Engine::Scene::HierarchyEntry> m_entries;
    std::vector<std::uint32_t> m_first,m_next,m_roots,m_rows,m_stack;
    std::unordered_map<Engine::Guid,std::uint32_t,GuidHash> m_indices;
    std::unordered_set<Engine::Guid,GuidHash> m_open;
    std::uint64_t m_version=0;std::size_t m_rebuilds=0;
    std::string m_filter;bool m_dirty=true;
};
}
