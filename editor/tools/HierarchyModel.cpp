#include "HierarchyModel.h"
#include "engine/core/Profile.hpp"
#include <algorithm>
namespace EditorTools {
Engine::Result HierarchyModel::Refresh(const Engine::Scene::World& world){
    if(m_version==world.HierarchyVersion())return Engine::Result::Success();
    ENGINE_PROFILE_SCOPE("Editor.HierarchyFlatten");
    auto result=world.HierarchyEntries(m_entries);if(!result)return result;
    std::sort(m_entries.begin(),m_entries.end(),[](auto& a,auto& b){return a.id.high<b.id.high||(a.id.high==b.id.high&&a.id.low<b.id.low);});
    m_indices.clear();m_indices.reserve(m_entries.size());m_roots.clear();m_first.assign(m_entries.size(),UINT32_MAX);m_next.assign(m_entries.size(),UINT32_MAX);
    for(std::uint32_t i=0;i<m_entries.size();++i)m_indices.emplace(m_entries[i].id,i);
    for(std::size_t i=m_entries.size();i-->0;){auto parent=m_indices.find(m_entries[i].parent);if(parent==m_indices.end())m_roots.push_back(static_cast<std::uint32_t>(i));else{m_next[i]=m_first[parent->second];m_first[parent->second]=static_cast<std::uint32_t>(i);}}
    std::reverse(m_roots.begin(),m_roots.end());m_rows.reserve(m_entries.size());m_stack.reserve(m_entries.size());m_version=world.HierarchyVersion();m_dirty=true;++m_rebuilds;return result;
}
void HierarchyModel::Filter(std::string_view value){if(value!=m_filter){m_filter=value;m_dirty=true;}}
void HierarchyModel::SetOpen(Engine::Guid id,bool value){if(value?m_open.insert(id).second:m_open.erase(id)!=0)m_dirty=true;}
void HierarchyModel::Reveal(Engine::Guid id){auto found=m_indices.find(id);while(found!=m_indices.end()){SetOpen(m_entries[found->second].id,true);found=m_indices.find(m_entries[found->second].parent);}}
std::span<const std::uint32_t> HierarchyModel::Rows(){if(!m_dirty)return m_rows;m_rows.clear();m_stack.clear();for(auto it=m_roots.rbegin();it!=m_roots.rend();++it)m_stack.push_back(*it);while(!m_stack.empty()){auto i=m_stack.back();m_stack.pop_back();auto& e=m_entries[i];if(m_filter.empty()||e.name.find(m_filter)!=std::string::npos)m_rows.push_back(i);if(!m_filter.empty()||Open(e.id)){auto start=m_stack.size();for(auto child=m_first[i];child!=UINT32_MAX;child=m_next[child])m_stack.push_back(child);std::reverse(m_stack.begin()+static_cast<std::ptrdiff_t>(start),m_stack.end());}}m_dirty=false;return m_rows;}
}
