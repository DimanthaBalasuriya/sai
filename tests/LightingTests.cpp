#include "engine/renderer/Exposure.hpp"
#include "engine/rhi/AdapterSelection.hpp"
#include "engine/scene/Commands.hpp"
#include "engine/scene/Serialization.hpp"
#include <array>
#include <iostream>
#include <limits>
using namespace Engine;
int main(){
    int failures=0;
    auto check=[&](bool value,const char* name){if(!value){std::cerr<<"FAIL: "<<name<<'\n';++failures;}};
    auto exposure=Renderer::ManualExposure(15);
    check(std::abs(exposure-1.f/39321.6f)<1e-10f,"sunny exposure");
    check(Renderer::ManualExposure(16)==exposure*.5f,"one EV halves linear brightness");
    check(Renderer::ManualExposure(14)==exposure*2,"minus one EV doubles linear brightness");
    check(Renderer::ManualExposure(16,1)==exposure,"compensation cancels EV");
    check(Renderer::ManualExposure(std::numeric_limits<float>::quiet_NaN())==0,"nonfinite exposure rejected");
    check(Renderer::ManualExposure(32,-16)==std::exp2(-48.f)/1.2f,"full reflected exposure range");
    using namespace Rhi;
    std::array devices{
        AdapterInfo{0,"integrated",AdapterClass::Integrated,16},
        AdapterInfo{1,"discrete",AdapterClass::Discrete,8},
        AdapterInfo{2,"large discrete",AdapterClass::Discrete,24},
        AdapterInfo{3,"llvmpipe",AdapterClass::Software,64},
        AdapterInfo{4,"unsupported",AdapterClass::Discrete,128,"descriptor indexing missing"}};
    std::size_t selected=0;
    check(bool(SelectAdapter(devices,"",selected))&&selected==2,"rank then VRAM");
    check(bool(SelectAdapter(devices,"0",selected))&&selected==0,"numeric override");
    check(bool(SelectAdapter(devices,"discrete",selected))&&selected==1,"name override");
    check(!SelectAdapter(devices,"3",selected),"reject software override");
    check(!SelectAdapter(devices,"unsupported",selected),"reject missing features");
    check(!SelectAdapter(devices,"99",selected),"reject unknown override");
    check(bool(SelectAdapter(std::span(devices).first(1),"",selected))&&selected==0,"integrated-only supported");
    check(!SelectAdapter(std::span(devices).subspan(3,1),"",selected),"software-only fails");
    check(!SelectAdapter({},"",selected),"empty enumeration fails");
    Scene::World world;Scene::CommandHistory history;Scene::EntityDefinition camera;camera.id={99,1};camera.camera=Scene::Camera{};
    check(bool(history.Execute(world,std::make_unique<Scene::CreateEntityCommand>(camera))),"create camera");
    Renderer::RenderWorld snapshot;check(bool(world.Extract(snapshot))&&snapshot.camera.ev100==15,"default camera exposure extracted");
    check(bool(history.Execute(world,std::make_unique<Scene::SetFieldCommand>(camera.id,Scene::ComponentType::Camera,"ev100",14.f))),"reflected EV edit");
    check(bool(world.Extract(snapshot))&&snapshot.camera.ev100==14,"incremental camera exposure");
    check(bool(history.Undo(world))&&bool(world.Extract(snapshot))&&snapshot.camera.ev100==15,"undo exposure");
    check(bool(history.Redo(world))&&bool(world.Extract(snapshot))&&snapshot.camera.ev100==14,"redo exposure");
    check(!history.Execute(world,std::make_unique<Scene::SetFieldCommand>(camera.id,Scene::ComponentType::Camera,"ev100",std::numeric_limits<float>::infinity())),"reject infinite edit");
    const auto path=std::filesystem::temp_directory_path()/"engine_l0_exposure_test.json";
    check(bool(Scene::SaveScene(world,path,{})),"save exposure");
    std::vector<Scene::EntityDefinition> loaded;std::vector<Guid> assets;
    check(bool(Scene::ReadScene(path,loaded,assets))&&loaded.size()==1&&loaded[0].camera->ev100==14,"serialized EV round trip");
    std::filesystem::remove(path);
    return failures?1:0;
}
