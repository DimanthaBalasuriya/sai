#include "engine/scene/Serialization.hpp"
#include <iostream>
#include <fstream>
#include <cmath>
int main() {
    using namespace Engine;using namespace Engine::Scene;int failures=0;
    const auto check=[&](bool condition,const char* message){if (!condition) { ++failures;std::cerr<<"FAIL: "<<message<<'\n'; }};
    World world;CommandHistory history;EntityDefinition parent;parent.id={7,1};parent.name.value="Parent \"quoted\"";
    check(bool(history.Execute(world,std::make_unique<CreateEntityCommand>(parent))),"create parent");
    EntityDefinition child;child.id={7,2};child.hierarchy.parent=parent.id;child.transform.position={1,2,3};child.mesh=MeshRenderer{};child.camera=Camera{};child.light=Light{};
    check(bool(history.Execute(world,std::make_unique<CreateEntityCommand>(child))),"create reflected components");
    const Guid asset{20,30};check(bool(SaveScene(world,"scene-roundtrip.json",std::span{&asset,1})),"save all fields via reflection");
    std::vector<EntityDefinition> definitions;std::vector<Guid> assets;
    check(bool(ReadScene("scene-roundtrip.json",definitions,assets))&&definitions.size()==2&&assets.size()==1&&assets[0]==asset,"parse GUIDs and asset manifest");
    World loaded;CommandHistory loadedHistory;check(bool(LoadScene(loaded,loadedHistory,"scene-roundtrip.json",assets)),"load through command");
    FieldValue name;check(bool(loaded.ReadField(parent.id,ComponentType::Name,"value",name))&&std::get<std::string>(name)==parent.name.value,"quoted string round trip");
    EntityDefinition recovered;check(bool(loaded.Definition(child.id,recovered))&&recovered.mesh&&recovered.camera&&recovered.light&&recovered.transform.position==child.transform.position&&recovered.hierarchy.parent==parent.id,"optional components and hierarchy round trip");
    check(bool(loadedHistory.Undo(loaded))&&loaded.EntityCount()==0,"undo scene replacement");check(bool(loadedHistory.Redo(loaded))&&loaded.EntityCount()==2,"redo scene replacement");
    auto transform=child.transform;transform.position.x=9;
    check(bool(loadedHistory.Execute(loaded,std::make_unique<SetTransformCommand>(child.id,transform))),"gizmo-style transform command");
    check(bool(loadedHistory.Undo(loaded))&&bool(loaded.Definition(child.id,recovered))&&recovered.transform.position.x==1,"atomic transform undo");
    { std::ofstream stream("invalid-scene.json");stream<<R"({"version":99,"assets":[],"entities":[]})"; }
    auto count=loaded.EntityCount();check(!LoadScene(loaded,loadedHistory,"invalid-scene.json",assets)&&loaded.EntityCount()==count,"invalid version preserves World");
    std::vector<EntityDefinition> cycle{parent,child};cycle[0].hierarchy.parent=child.id;
    check(!loadedHistory.Execute(loaded,std::make_unique<ReplaceSceneCommand>(cycle))&&loaded.EntityCount()==count,"invalid replacement is transactional");
    auto singular=child.transform;singular.scale={0,1,1};check(!loadedHistory.Execute(loaded,std::make_unique<SetTransformCommand>(child.id,singular))&&bool(loaded.Definition(child.id,recovered))&&recovered.transform.position.x==1,"failed transform command preserves all fields");
    check(bool(SaveScene(loaded,"scene-roundtrip.json",assets)),"atomic replacement of existing scene file");
    if (!failures) std::cout<<"Reflected scene round trip and transaction tests passed\n";return failures?1:0;
}
