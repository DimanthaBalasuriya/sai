#include "engine/lighting/Lighting.hpp"
#include "../engine/lighting/render/LightBufferCache.hpp"
#include "engine/scene/Serialization.hpp"
#include "engine/scene/EntityFeatures.hpp"
#ifdef ENGINE_TEST_LIGHT_PREVIEW
#include "SceneTools.h"
#include <glm/gtc/matrix_transform.hpp>
#endif
#ifdef ENGINE_TEST_SKY_LINK
#include "engine/sky/SceneBridge.hpp"
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>

namespace {thread_local bool trackAllocations=false;thread_local std::size_t allocations=0;}
void* operator new(std::size_t size){if(trackAllocations)++allocations;if(auto p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
namespace {
using namespace Engine;
int failures=0;
void Check(bool value,const char* name){if(!value){std::cerr<<"FAIL: "<<name<<'\n';++failures;}}
bool Near(float a,float b){return std::abs(a-b)<=std::max(1e-5f,std::abs(b)*1e-5f);}
template<class T> Scene::EntityDefinition Entity(Guid id,Scene::ComponentType type,const T& light){Scene::EntityDefinition entity;entity.id=id;Scene::ModuleComponent encoded;Check(bool(Lighting::Encode(type,&light,encoded)),"encode component");entity.extensions.push_back(std::move(encoded));return entity;}
Renderer::RenderLight& Find(Renderer::RenderWorld& world,Guid id){return *std::find_if(world.lights.begin(),world.lights.end(),[&](auto& light){return light.entity==id;});}
Scene::FieldValue Read(Scene::World& world,Guid id,Scene::ComponentType type,const char* name){Scene::FieldValue value;Check(bool(world.ReadField(id,type,name,value)),"read reflected field");return value;}
Result Edit(Scene::World& world,Scene::CommandHistory& history,Guid id,Scene::ComponentType type,const char* name,Scene::FieldValue value){return history.Execute(world,std::make_unique<Scene::SetFieldCommand>(id,type,name,std::move(value)));}
void Conversions(){
    Check(Near(Lighting::PointLumensToCandela(4*std::numbers::pi_v<float>),1),"isotropic lumen/candela conversion");
    Check(Near(Lighting::SpotSolidAngle(std::numbers::pi_v<float>*.5f),2*std::numbers::pi_v<float>),"hemisphere cone solid angle");
    Check(Near(Lighting::SpotLumensToCandela(1000,.5235987756f),1187.9486f),"spot flux distributed over outer cone");
    Check(std::isfinite(Lighting::SpotLumensToCandela(1,.001f)),"narrow cone conversion finite");
    auto white=Lighting::TemperatureTint(6500),warm=Lighting::TemperatureTint(2400);Check(white==Vec3{1,1,1}&&warm.x>warm.z,"temperature tint baseline and warm shift");
    Check(Lighting::PhysicalIntensity(Lighting::kDirectionalLight,1,2)==100000,"artist directional baseline");
    Check(Lighting::PhysicalIntensity(Lighting::kPointLight,1,2)==300,"artist local baseline");
}
void ExtractionAndCache(){
    Scene::World world;Scene::CommandHistory history;
    Scene::EntityDefinition parent;parent.id={7,1};parent.transform.rotation={0,.5f,0};Check(bool(history.Execute(world,std::make_unique<Scene::CreateEntityCommand>(parent))),"create parent");
    auto sun=Entity({7,2},Lighting::kDirectionalLight,Lighting::DirectionalLight{});sun.hierarchy.parent=parent.id;sun.transform.scale={.5f,2,3};
    auto point=Entity({7,3},Lighting::kPointLight,Lighting::PointLight{});
    Check(bool(history.Execute(world,std::make_unique<Scene::CreateEntityCommand>(sun))),"create directional");Check(bool(history.Execute(world,std::make_unique<Scene::CreateEntityCommand>(point))),"create point definition");
    Renderer::RenderWorld snapshot;Check(bool(world.Extract(snapshot))&&snapshot.lights.size()==2,"registered lights extracted");
    auto& light=Find(snapshot,sun.id);Check(light.modulePacked&&light.gpu.color==Vec3{100000,100000,100000}&&light.gpu.shadowIndex==UINT32_MAX,"64 byte physical pack");
    Check(Near(light.direction.x,-std::sin(.5f))&&Near(light.direction.z,-std::cos(.5f)),"direction from parent world transform");
    Lighting::LightBufferCache cache;Check(bool(cache.Update(snapshot))&&cache.DirectionalCount()==1&&cache.LocalCount()==1,"separate directional and local ranges");
    Check(cache.Pending(0).size()==5&&cache.Pending(1).size()==5,"initial writes seed both frames");cache.Complete(0);cache.Complete(1);
    auto packCount=cache.PackCount();Check(bool(cache.Update(snapshot))&&cache.PackCount()==packCount&&cache.Pending(0).empty(),"unchanged extraction packs and uploads nothing");
    const auto old=light.gpu;
    Check(bool(Edit(world,history,sun.id,Lighting::kDirectionalLight,"illuminance",200000.f)),"edit illuminance");Check(bool(world.Extract(snapshot))&&!snapshot.fullUpdate&&snapshot.changedLights.size()==1,"dirty module light metadata");
    Check(bool(cache.Update(snapshot))&&cache.Pending(0).size()==1&&cache.Pending(0)[0]==0&&cache.Pending(1).size()==1,"dirty GPU index stable across frame rings");
    Check(old.color.x==100000&&cache.Lights()[0].color.x==200000,"owned snapshot packing");cache.Complete(0);
    Check(bool(Edit(world,history,sun.id,Lighting::kDirectionalLight,"illuminance",240000.f))&&bool(world.Extract(snapshot))&&bool(cache.Update(snapshot)),"second edit before other frame consumes upload");
    Check(cache.Pending(1).size()==1&&cache.Lights()[0].color.x==240000,"pending writes coalesce latest value");cache.Complete(0);cache.Complete(1);
    Scene::EntityDefinition camera;camera.id={7,4};camera.camera=Scene::Camera{};Check(bool(history.Execute(world,std::make_unique<Scene::CreateEntityCommand>(camera))),"create camera");Check(bool(world.Extract(snapshot))&&bool(cache.Update(snapshot)),"topology rebuild");cache.Complete(0);cache.Complete(1);
    packCount=cache.PackCount();Check(bool(Edit(world,history,camera.id,Scene::ComponentType::Camera,"ev100",14.f))&&bool(world.Extract(snapshot))&&bool(cache.Update(snapshot)),"camera-only change");Check(cache.PackCount()==packCount&&cache.Pending(0).empty(),"exposure does not repack lights");
    Check(bool(Edit(world,history,sun.id,Lighting::kDirectionalLight,"enabled",std::uint32_t{0}))&&bool(world.Extract(snapshot))&&bool(cache.Update(snapshot)),"disable light");Check(!(cache.Lights()[0].flags&Renderer::kLightEnabled)&&cache.DirectionalCount()==1,"disabled light retains stable slot");cache.Complete(0);cache.Complete(1);
    Check(bool(history.Undo(world))&&bool(world.Extract(snapshot))&&bool(cache.Update(snapshot))&&(cache.Lights()[0].flags&Renderer::kLightEnabled),"undo light mute");cache.Complete(0);cache.Complete(1);
    Check(bool(Edit(world,history,sun.id,Lighting::kDirectionalLight,"illuminance",250000.f)),"prepare steady-state edit");
    allocations=0;trackAllocations=true;auto extracted=world.Extract(snapshot);auto updated=cache.Update(snapshot);cache.Complete(0);cache.Complete(1);trackAllocations=false;
    Check(bool(extracted)&&bool(updated),"steady-state dirty extraction/upload succeeds");
#if !defined(_ITERATOR_DEBUG_LEVEL) || _ITERATOR_DEBUG_LEVEL == 0
    Check(allocations==0,"steady-state dirty extraction/upload bookkeeping allocates nothing");
#endif
    auto saved=std::filesystem::temp_directory_path()/"engine_l1_lights.scene.json";Check(bool(Scene::SaveScene(world,saved)),"serialize light settings");std::vector<Scene::EntityDefinition> definitions;std::vector<Guid> assets;
    Check(bool(Scene::ReadScene(saved,definitions,assets)),"read serialized light components");Scene::World loaded;Scene::CommandHistory loadHistory;Check(bool(loadHistory.Execute(loaded,std::make_unique<Scene::ReplaceSceneCommand>(definitions))),"reload scene");Renderer::RenderWorld restored;Check(bool(loaded.Extract(restored))&&Find(restored,sun.id).gpu.color==Find(snapshot,sun.id).gpu.color,"physical light roundtrip");std::filesystem::remove(saved);
    auto rebuilds=cache.Rebuilds();Check(bool(cache.Update(restored))&&cache.Rebuilds()==rebuilds+1&&cache.Pending(0).size()==5,"new scene source reseeds frame buffers");
    auto oversized=restored;oversized.modified=true;oversized.lights.assign(5,Find(restored,sun.id));
    Check(!cache.Update(oversized)&&cache.DirectionalCount()==1&&cache.Rebuilds()==rebuilds+1,"directional capacity rejection preserves cache");
    auto flags=Scene::ModuleComponent{};Check(bool(Scene::DefaultComponent(Scene::kEntityFlags,flags)),"visibility fixture");flags.fields[0]=std::uint32_t{0};parent.extensions.push_back(flags);Check(bool(history.Execute(world,std::make_unique<Scene::ReplaceSceneCommand>(std::vector{parent,sun,point,camera})))&&bool(world.Extract(snapshot))&&snapshot.lights.size()==1,"hidden parent suppresses module light");
    auto duplicate=sun;duplicate.id={9,1};duplicate.hierarchy.parent={};duplicate.light=Scene::Light{};Check(!history.Execute(world,std::make_unique<Scene::CreateEntityCommand>(duplicate)),"conflicting light components rejected");
}
void ValidationUnitsAndPresets(){
    Scene::World world;Scene::CommandHistory history;auto sun=Entity({1,3},Lighting::kDirectionalLight,Lighting::DirectionalLight{});auto point=Entity({1,4},Lighting::kPointLight,Lighting::PointLight{});auto spot=Entity({1,5},Lighting::kSpotLight,Lighting::SpotLight{});
    Scene::EntityDefinition camera;camera.id={1,2};camera.camera=Scene::Camera{};
    for(auto& entity:std::array{sun,point,spot,camera})Check(bool(history.Execute(world,std::make_unique<Scene::CreateEntityCommand>(entity))),"create authoring fixture");
    Check(!Edit(world,history,sun.id,Lighting::kDirectionalLight,"unit",std::uint32_t{1}),"directional lumen unit rejected");
    auto revision=world.Revision();Check(!Edit(world,history,spot.id,Lighting::kSpotLight,"outerCone",.1f)&&world.Revision()==revision,"spot cone validation transactional");
    Check(!Edit(world,history,sun.id,Lighting::kDirectionalLight,"illuminance",std::numeric_limits<float>::quiet_NaN()),"nonfinite light rejected");
    Renderer::RenderWorld before,after;world.Extract(before);Check(bool(Lighting::ChangeIntensityUnit(world,history,point.id,Lighting::kPointLight,1))&&bool(world.Extract(after))&&Near(Find(after,point.id).gpu.color.x,Find(before,point.id).gpu.color.x),"unit switch preserves brightness");
    Check(std::get<std::uint32_t>(Read(world,point.id,Lighting::kPointLight,"unit"))==1,"lumen unit authored");Check(bool(history.Undo(world))&&std::get<std::uint32_t>(Read(world,point.id,Lighting::kPointLight,"unit"))==0,"unit switch is one undo entry");
    Check(bool(Edit(world,history,point.id,Lighting::kPointLight,"intensity",100000000.f)),"high intensity within range");revision=world.Revision();Check(!Lighting::ChangeIntensityUnit(world,history,point.id,Lighting::kPointLight,1)&&world.Revision()==revision,"overflowing unit switch leaves scene untouched");
    history.Clear();Check(bool(Lighting::ApplyPreset(world,history,2)),"indoor preset");Check(std::get<float>(Read(world,sun.id,Lighting::kDirectionalLight,"illuminance"))==50&&std::get<float>(Read(world,camera.id,Scene::ComponentType::Camera,"ev100"))==5,"preset couples lux and EV");
    Check(bool(history.Undo(world))&&!history.CanUndo()&&std::get<float>(Read(world,sun.id,Lighting::kDirectionalLight,"illuminance"))==100000&&std::get<float>(Read(world,camera.id,Scene::ComponentType::Camera,"ev100"))==15,"preset undo restores pair atomically");Check(bool(history.Redo(world)),"redo preset");
    Check(!Lighting::ApplyPreset(world,history,99),"invalid preset rejected");
    auto inherited=Entity({10,2},Lighting::kDirectionalLight,Lighting::DirectionalLight{});inherited.hierarchy.parent=sun.id;Scene::ModuleComponent inheritance;Scene::DefaultComponent(Scene::kPropertyInheritance,inheritance);inheritance.fields[1]=std::uint32_t{1};inheritance.fields[2]=std::uint32_t{1};inherited.extensions.push_back(inheritance);
    Check(bool(history.Execute(world,std::make_unique<Scene::CreateEntityCommand>(inherited)))&&bool(world.Extract(after))&&Find(after,inherited.id).intensity==50,"registered lights share generic inheritance");
}
#ifdef ENGINE_TEST_LIGHT_PREVIEW
void EditorPreview(){
    Scene::World world;Scene::CommandHistory history;Scene::EntityDefinition parent;parent.id={30,1};
    auto sun=Entity({30,2},Lighting::kDirectionalLight,Lighting::DirectionalLight{});sun.hierarchy.parent=parent.id;
    Check(bool(history.Execute(world,std::make_unique<Scene::CreateEntityCommand>(parent)))&&bool(history.Execute(world,std::make_unique<Scene::CreateEntityCommand>(sun))),"editor light fixture");
    Renderer::RenderWorld snapshot;world.Extract(snapshot);auto revision=world.Revision();EditorTools::TransformDrag drag;EditorTools::Visibility visibility;std::array selection{parent.id};
    Check(bool(drag.Begin(world,selection,glm::mat4(1),false))&&bool(drag.Update(glm::rotate(glm::mat4(1),.5f,glm::vec3(0,1,0)))),"parent light gizmo rotation");
    drag.Preview(snapshot);visibility.Filter(world,snapshot,&drag);auto& preview=Find(snapshot,sun.id);
    Check(snapshot.modified&&world.Revision()==revision&&Near(preview.gpu.direction.x,-std::sin(.5f))&&preview.direction==preview.gpu.direction,"gizmo previews GPU direction without authoring scene");
    drag.Cancel();world.Extract(snapshot);Check(Find(snapshot,sun.id).direction==Vec3{0,0,-1},"cancel restores light snapshot");
    Check(bool(visibility.Toggle(world,history,selection)),"hide parent in editor");visibility.Filter(world,snapshot);Check(snapshot.lights.empty()&&snapshot.modified,"editor hide filters registered descendant light");
    Check(bool(history.Undo(world))&&bool(world.Extract(snapshot))&&snapshot.lights.size()==1,"editor visibility undo restores registered light");
}
#endif
#ifdef ENGINE_TEST_SKY_LINK
void SkyLink(){
    Check(bool(Sky::RegisterComponents()),"sky registers lighting dependencies");Scene::World world;Scene::CommandHistory history;Check(bool(Sky::EnsureSettings(world,history)),"new sky fixture");Sky::Settings settings;Guid config,sun,moon;Check(bool(Sky::ReadSettings(world,settings,config,sun,moon))&&settings.sun.directionSource==2,"default sky follows authored light");
    Check(bool(Edit(world,history,sun,Scene::ComponentType::Transform,"rotation",Vec3{-.7f,.3f,0})),"rotate authored sun");Check(bool(Edit(world,history,sun,Lighting::kDirectionalLight,"illuminance",20000.f)),"edit sun lux");
    Sky::SceneSystems systems;Sky::CelestialState celestial;Check(bool(systems.Update(world,0,settings,celestial)),"update linked sky");Renderer::RenderWorld rendered;world.Extract(rendered);auto& light=Find(rendered,sun);
    Check(Near(celestial.sun.x,-light.direction.x)&&Near(celestial.sun.y,-light.direction.y)&&Near(celestial.sun.z,-light.direction.z)&&settings.sun.intensity==.2f,"sky direction/intensity agree with authored sun");
    Check(std::get<Vec3>(Read(world,sun,Scene::ComponentType::Transform,"rotation"))==Vec3{-.7f,.3f,0},"sky does not overwrite inspector rotation");
    Check(bool(Edit(world,history,sun,Lighting::kDirectionalLight,"angularDiameter",.001f))&&bool(Edit(world,history,sun,Lighting::kDirectionalLight,"unit",std::uint32_t{2}))&&bool(Edit(world,history,sun,Lighting::kDirectionalLight,"illuminance",100000000.f)),"extreme authored sky link fixture");
    Check(bool(Sky::ReadSettings(world,settings,config,sun,moon))&&settings.sun.angularSize==.001f&&std::isfinite(settings.sun.intensity),"sky accepts the full reflected light authoring range");
}
#endif
}
int main(){Check(bool(Engine::Scene::RegisterEntityFeatures()),"entity feature registration");Check(bool(Engine::Lighting::RegisterComponents())&&bool(Engine::Lighting::RegisterComponents()),"idempotent component registration");Conversions();ExtractionAndCache();ValidationUnitsAndPresets();
#ifdef ENGINE_TEST_LIGHT_PREVIEW
EditorPreview();
#endif
#ifdef ENGINE_TEST_SKY_LINK
SkyLink();
#endif
return failures?1:0;}
