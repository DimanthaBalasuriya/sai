#pragma once
#include "engine/scene/Commands.hpp"
#include "engine/renderer/RenderAssets.hpp"
#include "engine/renderer/RenderWorld.hpp"
#include <glm/glm.hpp>
#include <map>
#include "engine/core/SpatialIndex.hpp"
namespace EditorTools {
struct GuidLess {bool operator()(Engine::Guid a,Engine::Guid b) const {return a.high<b.high||(a.high==b.high&&a.low<b.low);}};
class Selection {
public:
    std::vector<Engine::Guid> ids;
    Engine::Guid Primary() const {return ids.empty()?Engine::Guid{}:ids.back();}
    bool Contains(Engine::Guid id) const;
    void Click(Engine::Guid id,bool add,bool toggle);
    void Prune(const Engine::Scene::World&);
};
Engine::Result Capture(const Engine::Scene::World&,std::vector<Engine::Scene::EntityDefinition>&);
std::vector<Engine::Guid> Expand(const Engine::Scene::World&,std::span<const Engine::Guid>);
Engine::Result Copy(const Engine::Scene::World&,std::span<const Engine::Guid>,std::vector<Engine::Scene::EntityDefinition>&);
Engine::Result Paste(Engine::Scene::World&,Engine::Scene::CommandHistory&,std::span<const Engine::Scene::EntityDefinition>,std::vector<Engine::Guid>&);
Engine::Result Delete(Engine::Scene::World&,Engine::Scene::CommandHistory&,std::span<const Engine::Guid>);
struct Ray {glm::vec3 origin,direction;};
struct Hit {Engine::Guid id;glm::vec3 point{0};float distance=0;explicit operator bool() const {return bool(id);}};
struct Bounds {glm::vec3 minimum{0},maximum{0};bool valid=false;};
class Geometry {
public:
    Geometry();
    void Store(const Engine::Renderer::AssetBundle&);
    Hit Pick(const Engine::Renderer::RenderWorld&,const Ray&,std::span<const Engine::Guid> ignored={}) const;
    Bounds SelectionBounds(const Engine::Renderer::RenderWorld&,std::span<const Engine::Guid>) const;
    std::vector<Engine::Guid> Box(const Engine::Renderer::RenderWorld&,const glm::mat4& vp,glm::vec2 minimum,glm::vec2 maximum) const;
    void Cull(const Engine::Renderer::RenderWorld&,const glm::mat4& vp,std::vector<std::uint32_t>&) const;
private:
    void Sync(const Engine::Renderer::RenderWorld&) const;
    std::map<Engine::Guid,Engine::Renderer::MeshAsset,GuidLess> m_meshes;
    std::map<Engine::Guid,Bounds,GuidLess> m_bounds;
    mutable Engine::Spatial::SpatialIndex m_index;
    struct SpatialEntry {Engine::Spatial::SpatialIndex::Proxy proxy;std::uint64_t mark;};
    mutable std::map<Engine::Guid,SpatialEntry,GuidLess> m_spatialEntries;
    mutable std::vector<Engine::Spatial::SpatialIndex::Proxy> m_proxies;
    mutable std::vector<std::uint32_t> m_candidates;
    mutable Engine::Guid m_source;mutable std::uint64_t m_revision=UINT64_MAX,m_spatialGeneration=0;
};
Ray ScreenRay(const glm::mat4& inverseVp,glm::vec2 normalizedPosition);
class TransformDrag {
public:
    Engine::Result Begin(Engine::Scene::World&,std::span<const Engine::Guid>,const glm::mat4& pivot,bool individual);
    Engine::Result Update(const glm::mat4& pivot);
    void Preview(Engine::Renderer::RenderWorld&) const;
    glm::mat4 PreviewTransform(Engine::Guid,const Engine::Mat4&) const;
    Engine::Result Commit(Engine::Scene::World&,Engine::Scene::CommandHistory&);
    void Cancel();
    bool Active() const {return !m_before.empty();}
    bool Changed() const {return m_changed;}
    const glm::mat4& Pivot() const {return m_pivot;}
    std::span<const Engine::Guid> Affected() const {return m_affected;}
private:
    std::vector<Engine::Scene::EntityDefinition> m_before,m_after;
    std::map<Engine::Guid,glm::mat4,GuidLess> m_globals,m_deltas;
    std::vector<Engine::Guid> m_roots,m_affected;
    glm::mat4 m_start{1},m_pivot{1};bool m_individual=false,m_changed=false;
    Engine::Guid m_source;std::uint64_t m_revision=0;
};
class Visibility {
public:
    std::vector<Engine::Guid> hidden;std::uint64_t revision=0;
    bool Hidden(Engine::Guid id) const;
    Engine::Result Toggle(Engine::Scene::World&,Engine::Scene::CommandHistory&,std::span<const Engine::Guid>);
    void Filter(Engine::Scene::World&,Engine::Renderer::RenderWorld&,const TransformDrag* drag=nullptr) const;
};
}
