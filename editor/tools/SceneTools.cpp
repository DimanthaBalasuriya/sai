#include "SceneTools.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <algorithm>
#include <limits>
#include <random>
#include <set>
#include "engine/core/Profile.hpp"
namespace EditorTools {
namespace {
bool Has(std::span<const Engine::Guid> ids,Engine::Guid id) {return std::find(ids.begin(),ids.end(),id)!=ids.end();}
Engine::Result Decompose(const glm::mat4& m,Engine::Scene::Transform& output) {
    glm::vec3 scale,position,skew;glm::vec4 perspective;glm::quat rotation;
    if(!glm::decompose(m,scale,rotation,position,skew,perspective)||glm::length(skew)>.001f||glm::any(glm::lessThanEqual(scale,glm::vec3(.0001f)))) return Engine::Result::Failure("Transform would introduce shear or a non-positive scale");
    float z,y,x;glm::extractEulerAngleZYX(glm::mat4_cast(rotation),z,y,x);output={{position.x,position.y,position.z},{x,y,z},{scale.x,scale.y,scale.z}};return Engine::Result::Success();
}
Bounds LocalBounds(const Engine::Renderer::MeshAsset& mesh) {Bounds b;if(mesh.vertices.empty())return b;b.minimum=glm::vec3(std::numeric_limits<float>::max());b.maximum=-b.minimum;for(auto v:mesh.vertices) {glm::vec3 p(v.x,v.y,v.z);b.minimum=glm::min(b.minimum,p);b.maximum=glm::max(b.maximum,p);}b.valid=true;return b;}
glm::vec3 Corner(const Bounds& b,int i) {return {i&1?b.maximum.x:b.minimum.x,i&2?b.maximum.y:b.minimum.y,i&4?b.maximum.z:b.minimum.z};}
bool Triangle(const Ray& ray,glm::vec3 a,glm::vec3 b,glm::vec3 c,float& distance) {
    auto edge1=b-a,edge2=c-a;auto p=glm::cross(ray.direction,edge2);float det=glm::dot(edge1,p);if(std::abs(det)<1e-8f)return false;
    auto t=ray.origin-a;float u=glm::dot(t,p)/det;if(u<0||u>1)return false;auto q=glm::cross(t,edge1);float v=glm::dot(ray.direction,q)/det;if(v<0||u+v>1)return false;
    distance=glm::dot(edge2,q)/det;return distance>=0;
}
}
bool Selection::Contains(Engine::Guid id) const {return Has(ids,id);}
void Selection::Click(Engine::Guid id,bool add,bool toggle) {if(!add&&!toggle)ids.clear();if(!id)return;auto it=std::find(ids.begin(),ids.end(),id);if(toggle&&it!=ids.end())ids.erase(it);else if(it==ids.end())ids.push_back(id);}
void Selection::Prune(const Engine::Scene::World& world) {std::erase_if(ids,[&](auto id){return !world.Contains(id);});}
Engine::Result Capture(const Engine::Scene::World& world,std::vector<Engine::Scene::EntityDefinition>& output) {std::vector<Engine::Guid> ids;auto r=world.Entities(ids);if(!r)return r;std::vector<Engine::Scene::EntityDefinition> next;for(auto id:ids) {Engine::Scene::EntityDefinition entity;r=world.Definition(id,entity);if(!r)return r;next.push_back(std::move(entity));}output=std::move(next);return Engine::Result::Success();}
std::vector<Engine::Guid> Expand(const Engine::Scene::World& world,std::span<const Engine::Guid> selection) {
    std::vector<Engine::Guid> output;world.Descendants(selection,output);std::sort(output.begin(),output.end(),GuidLess{});return output;
}
Engine::Result Copy(const Engine::Scene::World& world,std::span<const Engine::Guid> selection,std::vector<Engine::Scene::EntityDefinition>& output) {
    std::vector<Engine::Scene::EntityDefinition> next;auto expanded=Expand(world,selection);
    for(auto id:expanded) {Engine::Scene::EntityDefinition e;auto r=world.Definition(id,e);if(!r)return r;if(!std::binary_search(expanded.begin(),expanded.end(),e.hierarchy.parent,GuidLess{})) {Engine::Mat4 global;r=const_cast<Engine::Scene::World&>(world).GlobalTransform(id,global);if(!r)return r;r=Decompose(glm::make_mat4(global.values.data()),e.transform);if(!r)return r;e.hierarchy.parent={};}next.push_back(std::move(e));}
    output=std::move(next);return Engine::Result::Success();
}
Engine::Result Paste(Engine::Scene::World& world,Engine::Scene::CommandHistory& history,std::span<const Engine::Scene::EntityDefinition> clipboard,std::vector<Engine::Guid>& selected) {
    std::vector<Engine::Scene::EntityDefinition> definitions(clipboard.begin(),clipboard.end());for(auto& d:definitions)if(!d.hierarchy.parent)d.name.value+=" copy";
    return Engine::Scene::InstantiatePrefab(world,history,definitions,selected);
}
Engine::Result Delete(Engine::Scene::World& world,Engine::Scene::CommandHistory& history,std::span<const Engine::Guid> selection){auto expanded=Expand(world,selection);if(expanded.empty())return Engine::Result::Success();return history.Execute(world,std::make_unique<Engine::Scene::EntityBatchCommand>(std::vector<Engine::Scene::EntityDefinition>{},std::move(expanded)));}

Geometry::Geometry() {
    Engine::Renderer::MeshAsset cube;cube.id=Engine::kBuiltinCube;
    for(int i=0;i<8;++i)cube.vertices.push_back({i&1?1.f:-1.f,i&2?1.f:-1.f,i&4?1.f:-1.f});
    cube.indices={0,1,3,0,3,2,4,6,7,4,7,5,0,4,5,0,5,1,2,3,7,2,7,6,0,2,6,0,6,4,1,5,7,1,7,3};m_bounds[cube.id]=LocalBounds(cube);m_meshes[cube.id]=std::move(cube);
}
void Geometry::Store(const Engine::Renderer::AssetBundle& assets) {for(const auto& mesh:assets.meshes){m_meshes[mesh.id]=mesh;m_bounds[mesh.id]=LocalBounds(mesh);}m_revision=UINT64_MAX;}
namespace {
std::array<Engine::Spatial::Plane,6> Frustum(const glm::mat4& vp){auto row=[&](int i){return glm::vec4(vp[0][i],vp[1][i],vp[2][i],vp[3][i]);};auto r0=row(0),r1=row(1),r2=row(2),r3=row(3);std::array<Engine::Spatial::Plane,6> output;std::array planes{r3+r0,r3-r0,r3+r1,r3-r1,r2,r3-r2};for(int i=0;i<6;++i)output[i]={{planes[i].x,planes[i].y,planes[i].z},planes[i].w};return output;}
}
void Geometry::Sync(const Engine::Renderer::RenderWorld& world)const{
    ENGINE_PROFILE_SCOPE("Editor.SpatialSync");
    if(world.source&&!world.modified&&world.source==m_source&&world.revision==m_revision)return;
    bool rebuild=world.modified||world.source!=m_source||m_proxies.size()!=world.instances.size()||world.fullUpdate||world.baseRevision!=m_revision;
    if(rebuild){++m_spatialGeneration;if(m_source!=world.source){m_index.Clear();m_spatialEntries.clear();}m_proxies.assign(world.instances.size(),-1);m_candidates.reserve(world.instances.size());}
    auto update=[&](std::uint32_t i){auto& instance=world.instances[i];auto found=m_bounds.find(instance.mesh);if(found==m_bounds.end()||!found->second.valid)return;auto& b=found->second;auto m=glm::make_mat4(instance.model.values.data());glm::vec3 minimum(std::numeric_limits<float>::max()),maximum(-std::numeric_limits<float>::max());for(int c=0;c<8;++c){auto p=glm::vec3(m*glm::vec4(Corner(b,c),1));minimum=glm::min(minimum,p);maximum=glm::max(maximum,p);}Engine::Spatial::Aabb bounds{{minimum.x,minimum.y,minimum.z},{maximum.x,maximum.y,maximum.z}};if(rebuild){auto foundProxy=m_spatialEntries.find(instance.entity);if(foundProxy!=m_spatialEntries.end()){m_proxies[i]=foundProxy->second.proxy;foundProxy->second.mark=m_spatialGeneration;m_index.SetPayload(m_proxies[i],i);}}if(m_proxies[i]<0){m_proxies[i]=m_index.Insert(bounds,i);m_spatialEntries[instance.entity]={m_proxies[i],m_spatialGeneration};}else m_index.Update(m_proxies[i],bounds);};
    if(rebuild)for(std::uint32_t i=0;i<world.instances.size();++i)update(i);else for(auto i:world.changedInstances)if(i<world.instances.size())update(i);
    if(rebuild)for(auto it=m_spatialEntries.begin();it!=m_spatialEntries.end();)if(it->second.mark!=m_spatialGeneration){m_index.Remove(it->second.proxy);it=m_spatialEntries.erase(it);}else ++it;
    m_source=world.source;m_revision=world.revision;
}
void Geometry::Cull(const Engine::Renderer::RenderWorld& world,const glm::mat4& vp,std::vector<std::uint32_t>& output)const{Sync(world);auto planes=Frustum(vp);m_index.Query(planes,output);}
Ray ScreenRay(const glm::mat4& inverse,glm::vec2 p) {auto a=inverse*glm::vec4(p.x*2-1,1-p.y*2,0,1),b=inverse*glm::vec4(p.x*2-1,1-p.y*2,1,1);glm::vec3 origin=glm::vec3(a)/a.w;return {origin,glm::normalize(glm::vec3(b)/b.w-origin)};}
Hit Geometry::Pick(const Engine::Renderer::RenderWorld& world,const Ray& ray,std::span<const Engine::Guid> ignored) const {
    Sync(world);m_index.Query(Engine::Spatial::SpatialRay{{ray.origin.x,ray.origin.y,ray.origin.z},{ray.direction.x,ray.direction.y,ray.direction.z}},m_candidates);
    Hit result;result.distance=std::numeric_limits<float>::max();
    for(auto index:m_candidates) {const auto& instance=world.instances[index];if(Has(ignored,instance.entity))continue;auto found=m_meshes.find(instance.mesh);if(found==m_meshes.end())continue;auto inverse=glm::inverse(glm::make_mat4(instance.model.values.data()));Ray local{glm::vec3(inverse*glm::vec4(ray.origin,1)),glm::vec3(inverse*glm::vec4(ray.direction,0))};const auto& mesh=found->second;
        for(std::size_t i=0;i+2<mesh.indices.size();i+=3) {glm::vec3 p[3];bool valid=true;for(int j=0;j<3;++j) {auto vertexIndex=mesh.indices[i+j];if(vertexIndex>=mesh.vertices.size()){valid=false;break;}auto v=mesh.vertices[vertexIndex];p[j]={v.x,v.y,v.z};}float t;if(valid&&Triangle(local,p[0],p[1],p[2],t)&&t<result.distance) {result={instance.entity,ray.origin+ray.direction*t,t};}}
    }return result;
}
Bounds Geometry::SelectionBounds(const Engine::Renderer::RenderWorld& world,std::span<const Engine::Guid> ids) const {
    Bounds result;result.minimum=glm::vec3(std::numeric_limits<float>::max());result.maximum=-result.minimum;
    for(const auto& instance:world.instances) if(Has(ids,instance.entity)) {auto it=m_meshes.find(instance.mesh);if(it==m_meshes.end())continue;auto b=m_bounds.at(instance.mesh);if(!b.valid)continue;auto m=glm::make_mat4(instance.model.values.data());for(int i=0;i<8;++i) {auto p=glm::vec3(m*glm::vec4(Corner(b,i),1));result.minimum=glm::min(result.minimum,p);result.maximum=glm::max(result.maximum,p);}result.valid=true;}return result;
}
std::vector<Engine::Guid> Geometry::Box(const Engine::Renderer::RenderWorld& world,const glm::mat4& vp,glm::vec2 lo,glm::vec2 hi) const {
    std::vector<Engine::Guid> result;Sync(world);if(hi.x<=lo.x||hi.y<=lo.y)return result;glm::mat4 crop(1);crop[0][0]=1/(hi.x-lo.x);crop[1][1]=1/(hi.y-lo.y);crop[3][0]=-(lo.x+hi.x-1)/(hi.x-lo.x);crop[3][1]=-(1-lo.y-hi.y)/(hi.y-lo.y);auto planes=Frustum(crop*vp);m_index.Query(planes,m_candidates);
    for(auto index:m_candidates) {const auto& instance=world.instances[index];auto it=m_meshes.find(instance.mesh);if(it==m_meshes.end())continue;auto b=m_bounds.at(instance.mesh);glm::vec2 minimum(std::numeric_limits<float>::max()),maximum(-std::numeric_limits<float>::max());int visible=0;
        for(int i=0;i<8;++i) {auto clip=vp*glm::make_mat4(instance.model.values.data())*glm::vec4(Corner(b,i),1);if(clip.w<=.0001f)continue;glm::vec2 p{(clip.x/clip.w+1)*.5f,(1-clip.y/clip.w)*.5f};minimum=glm::min(minimum,p);maximum=glm::max(maximum,p);++visible;}
        if(visible&&minimum.x<=hi.x&&minimum.y<=hi.y&&maximum.x>=lo.x&&maximum.y>=lo.y)result.push_back(instance.entity);
    }return result;
}
Engine::Result TransformDrag::Begin(Engine::Scene::World& world,std::span<const Engine::Guid> selected,const glm::mat4& pivot,bool individual) {
    Cancel();auto r=Capture(world,m_before);if(!r)return r;m_after=m_before;m_start=m_pivot=pivot;m_individual=individual;m_revision=world.Revision();Engine::Renderer::RenderWorld render;r=world.Extract(render);if(!r){Cancel();return r;}m_source=render.source;
    for(auto id:selected) {bool descendant=false;auto parent=id;for(std::size_t depth=0;depth<m_before.size();++depth) {auto it=std::find_if(m_before.begin(),m_before.end(),[&](const auto& e){return e.id==parent;});if(it==m_before.end()||!it->hierarchy.parent)break;parent=it->hierarchy.parent;if(Has(selected,parent)){descendant=true;break;}}if(!descendant&&world.Contains(id))m_roots.push_back(id);}
    if(m_roots.empty()){Cancel();return Engine::Result::Failure("Select an entity before dragging");}m_affected=Expand(world,m_roots);
    for(const auto& e:m_before) {Engine::Mat4 global;r=world.GlobalTransform(e.id,global);if(!r){Cancel();return r;}m_globals[e.id]=glm::make_mat4(global.values.data());}return Engine::Result::Success();
}
Engine::Result TransformDrag::Update(const glm::mat4& pivot) {
    if(!Active())return Engine::Result::Failure("No active transform drag");auto next=m_before;auto delta=pivot*glm::inverse(m_start);std::map<Engine::Guid,glm::mat4,GuidLess> deltas;
    for(auto root:m_roots) {auto rootDelta=delta;if(m_individual) {auto position=glm::vec3(m_globals.at(root)[3]);rootDelta=glm::translate(glm::mat4(1),glm::vec3(pivot[3]-m_start[3]))*glm::translate(glm::mat4(1),position)*glm::mat4(glm::mat3(delta))*glm::translate(glm::mat4(1),-position);}
        auto it=std::find_if(next.begin(),next.end(),[&](const auto& e){return e.id==root;});auto parent=it->hierarchy.parent?m_globals.at(it->hierarchy.parent):glm::mat4(1);auto r=Decompose(glm::inverse(parent)*rootDelta*m_globals.at(root),it->transform);if(!r)return r;
        for(auto id:m_affected) {auto cursor=id;for(std::size_t depth=0;depth<m_before.size();++depth) {if(cursor==root){deltas[id]=rootDelta;break;}auto e=std::find_if(m_before.begin(),m_before.end(),[&](const auto& v){return v.id==cursor;});if(e==m_before.end()||!e->hierarchy.parent)break;cursor=e->hierarchy.parent;}}
    }
    m_after=std::move(next);m_deltas=std::move(deltas);m_pivot=pivot;m_changed=false;for(int c=0;c<4;++c)for(int row=0;row<4;++row)m_changed|=std::abs(delta[c][row]-(c==row?1.f:0.f))>.00001f;return Engine::Result::Success();
}
void TransformDrag::Preview(Engine::Renderer::RenderWorld& world) const {if(!Active())return;world.modified=true;world.fullUpdate=true;for(auto& instance:world.instances) {auto it=m_deltas.find(instance.entity);if(it==m_deltas.end())continue;auto model=it->second*glm::make_mat4(instance.model.values.data());auto normal=glm::transpose(glm::inverse(model));std::copy_n(glm::value_ptr(model),16,instance.model.values.begin());std::copy_n(glm::value_ptr(normal),16,instance.normal.values.begin());}}
glm::mat4 TransformDrag::PreviewTransform(Engine::Guid id,const Engine::Mat4& original) const {auto model=glm::make_mat4(original.values.data());auto it=m_deltas.find(id);return it==m_deltas.end()?model:it->second*model;}
Engine::Result TransformDrag::Commit(Engine::Scene::World& world,Engine::Scene::CommandHistory& history) {
    if(!Active())return Engine::Result::Success();Engine::Renderer::RenderWorld current;auto r=world.Extract(current);if(!r)return r;if(world.Revision()!=m_revision||current.source!=m_source){Cancel();return Engine::Result::Failure("Scene changed during the transform drag");}
    if(m_changed)r=history.Execute(world,std::make_unique<Engine::Scene::ReplaceSceneCommand>(std::move(m_after)));Cancel();return r;
}
void TransformDrag::Cancel() {m_before.clear();m_after.clear();m_globals.clear();m_deltas.clear();m_roots.clear();m_affected.clear();m_changed=false;}
namespace {
class VisibilityCommand final:public Engine::Scene::ICommand {
public:VisibilityCommand(Visibility& owner,std::vector<Engine::Guid> after):m_owner(owner),m_before(owner.hidden),m_after(std::move(after)){}
    Engine::Result Do(Engine::Scene::World&) override {m_owner.hidden=m_after;++m_owner.revision;return Engine::Result::Success();}
    Engine::Result Undo(Engine::Scene::World&) override {m_owner.hidden=m_before;++m_owner.revision;return Engine::Result::Success();}
private:Visibility& m_owner;std::vector<Engine::Guid> m_before,m_after;
};
}
bool Visibility::Hidden(Engine::Guid id) const {return Has(hidden,id);}
Engine::Result Visibility::Toggle(Engine::Scene::World& world,Engine::Scene::CommandHistory& history,std::span<const Engine::Guid> ids) {auto all=Expand(world,ids);if(all.empty())return Engine::Result::Success();bool show=std::all_of(all.begin(),all.end(),[&](auto id){return Hidden(id);});auto after=hidden;for(auto id:all) {if(show)std::erase(after,id);else if(!Has(after,id))after.push_back(id);}return history.Execute(world,std::make_unique<VisibilityCommand>(*this,std::move(after)));}
void Visibility::Filter(Engine::Scene::World&,Engine::Renderer::RenderWorld& world,const TransformDrag* drag) const {
    if(!hidden.empty()){std::erase_if(world.instances,[&](const auto& instance){return Hidden(instance.entity);});world.modified=true;world.fullUpdate=true;world.revision+=revision;}
    if(!hidden.empty())std::erase_if(world.lights,[&](const auto& light){return Hidden(light.entity);});
    if(drag&&drag->Active())for(auto& light:world.lights){
        Engine::Mat4 identity;auto delta=drag->PreviewTransform(light.entity,identity);
        auto position=glm::vec3(delta*glm::vec4(light.position.x,light.position.y,light.position.z,1));
        auto direction=glm::normalize(glm::vec3(delta*glm::vec4(light.direction.x,light.direction.y,light.direction.z,0)));
        light.position={position.x,position.y,position.z};light.direction={direction.x,direction.y,direction.z};
        if(light.modulePacked){light.gpu.position=light.position;light.gpu.direction=light.direction;}
    }

}
}
