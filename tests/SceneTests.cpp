#include "engine/scene/Commands.hpp"
#include <iostream>
#include <cmath>
#include <limits>
namespace {
using namespace Engine;
using namespace Engine::Scene;
int failures=0;
void Check(bool condition,const char* name) { if (!condition) { ++failures; std::cerr<<"FAIL: "<<name<<'\n'; } }
bool Near(float a,float b) { return std::abs(a-b)<0.001f; }
Result Create(World& world,CommandHistory& history,Guid id,Guid parent={},bool mesh=false) {
    EntityDefinition definition; definition.id=id; definition.hierarchy.parent=parent;
    if (mesh) definition.mesh=MeshRenderer{};
    return history.Execute(world,std::make_unique<CreateEntityCommand>(definition));
}
Result Edit(World& world,CommandHistory& history,Guid id,ComponentType component,std::string name,FieldValue value,bool merge=false) {
    return history.Execute(world,std::make_unique<SetFieldCommand>(id,component,std::move(name),std::move(value)),merge);
}
void HierarchyAndCommands() {
    World world; CommandHistory history; Guid parent{1,1},child{1,2},grandchild{1,3};
    Check(bool(Create(world,history,parent)),"create parent"); Check(bool(Create(world,history,child,parent,true)),"create child"); Check(bool(Create(world,history,grandchild,child)),"create grandchild");
    Check(bool(Edit(world,history,parent,ComponentType::Transform,"position",Vec3{2,0,0})),"edit parent through reflection");
    Check(bool(Edit(world,history,child,ComponentType::Transform,"position",Vec3{3,0,0})),"edit child");
    Mat4 matrix; Check(bool(world.GlobalTransform(grandchild,matrix))&&Near(matrix.values[12],5),"parent chain propagation");
    Renderer::RenderWorld before; Check(bool(world.Extract(before))&&before.instances.size()==1,"extract mesh snapshot");
    Check(bool(Edit(world,history,parent,ComponentType::Transform,"position",Vec3{7,0,0},true)),"second parent edit");
    Check(bool(world.GlobalTransform(grandchild,matrix))&&Near(matrix.values[12],10),"cached descendants update after parent edit");
    Renderer::RenderWorld after; Check(bool(world.Extract(after))&&Near(after.instances[0].model.values[12],10),"changed extraction updates model");
    Check(Near(before.instances[0].model.values[12],5),"snapshot does not alias live World");
    Check(!history.Execute(world,std::make_unique<ReparentCommand>(parent,grandchild)),"cycle rejected");
    Check(bool(history.Undo(world))&&bool(world.GlobalTransform(grandchild,matrix))&&Near(matrix.values[12],5),"undo preserves previous parent transform");
    Check(bool(history.Redo(world))&&bool(world.GlobalTransform(grandchild,matrix))&&Near(matrix.values[12],10),"redo parent transform");
    Check(bool(history.Execute(world,std::make_unique<ReparentCommand>(child,Guid{})))&&bool(world.GlobalTransform(grandchild,matrix))&&Near(matrix.values[12],3),"reparent updates descendants");
    Check(bool(history.Undo(world)),"undo reparent");
    Check(!Edit(world,history,parent,ComponentType::Transform,"scale",Vec3{0,1,1}),"singular transform rejected");
    Check(!Edit(world,history,parent,ComponentType::Transform,"position",std::string("wrong")),"reflected type mismatch rejected");
    Check(!Edit(world,history,parent,ComponentType::Transform,"position",Vec3{std::numeric_limits<float>::quiet_NaN(),0,0}),"nonfinite edit rejected");
    Check(!Create(world,history,parent),"duplicate GUID rejected");
}
void MergeAndIdentity() {
    World world; CommandHistory history; Guid id{5,1}; Check(bool(Create(world,history,id)),"create merge entity");
    Check(bool(Edit(world,history,id,ComponentType::Name,"value",std::string("A"),true)),"first name edit");
    Check(bool(Edit(world,history,id,ComponentType::Name,"value",std::string("B"),true)),"merge name edit");
    FieldValue value; Check(bool(history.Undo(world))&&bool(world.ReadField(id,ComponentType::Name,"value",value))&&std::get<std::string>(value)=="Entity","merged undo restores first value");
    Check(bool(history.Redo(world))&&bool(world.ReadField(id,ComponentType::Name,"value",value))&&std::get<std::string>(value)=="B","merged redo uses latest value");
    Check(bool(history.Undo(world))&&bool(history.Undo(world))&&!world.Contains(id),"undo create removes identity");
    Check(bool(history.Redo(world))&&world.Contains(id),"redo create preserves GUID");
    Check(bool(Edit(world,history,id,ComponentType::Name,"value",std::string("Branch")))&&!history.Redo(world),"new edit truncates redo branch");
    Check(world.Reflection().size()==6,"all core components reflected");
}
void CameraLightsAndScale() {
    World world; CommandHistory history; EntityDefinition camera; camera.id={1,1}; camera.camera=Camera{}; camera.transform.position={1,2,3};
    Check(bool(history.Execute(world,std::make_unique<CreateEntityCommand>(camera))),"create camera");
    Check(!Edit(world,history,camera.id,ComponentType::Camera,"nearPlane",2000.0f),"invalid clip interval rejected");
    EntityDefinition light; light.id={1,2}; light.light=Light{}; light.light->type=1; light.transform.position={4,5,6};
    Check(bool(history.Execute(world,std::make_unique<CreateEntityCommand>(light))),"create point light");
    auto invalidLight=light; invalidLight.id={1,9}; invalidLight.light->range=0;
    Check(!history.Execute(world,std::make_unique<CreateEntityCommand>(invalidLight)),"invalid initial light rejected without creating entity");
    EntityDefinition mesh; mesh.id={1,3}; mesh.mesh=MeshRenderer{}; mesh.transform.scale={2,3,4};
    Check(bool(history.Execute(world,std::make_unique<CreateEntityCommand>(mesh))),"create nonuniform mesh");
    Renderer::RenderWorld snapshot; Check(bool(world.Extract(snapshot)),"extract camera/light/mesh");
    Check(snapshot.camera.valid&&Near(snapshot.camera.transform.values[14],3),"camera transform extracted");
    Check(snapshot.lights.size()==1&&snapshot.lights[0].type==1&&Near(snapshot.lights[0].position.y,5),"point light extracted");
    Check(Near(snapshot.instances[0].normal.values[0],0.5f)&&Near(snapshot.instances[0].normal.values[5],1.0f/3),"inverse-transpose normal matrix");
    auto revision=snapshot.revision; Check(bool(world.Extract(snapshot))&&snapshot.revision==revision,"unchanged extraction retains revision");
    World another; Renderer::RenderWorld other; Check(bool(another.Extract(other))&&other.source!=snapshot.source,"independent Worlds have independent upload identities");
}
void LargeScene() {
    World world; CommandHistory history;
    for (std::uint64_t i=1;i<=10000;++i) { if (!Create(world,history,{10,i},{},true)) { Check(false,"create 10k entities"); return; } }
    Renderer::RenderWorld snapshot; Check(bool(world.Extract(snapshot))&&snapshot.instances.size()==10000,"extract 10k meshes");
    Check(bool(Create(world,history,{10,10001},{},true))&&bool(world.Extract(snapshot))&&snapshot.instances.size()==10001,"new entity appears in next extraction");
}
}
int main() { HierarchyAndCommands(); MergeAndIdentity(); CameraLightsAndScale(); LargeScene();
    if (!failures) std::cout<<"Scene, reflection, command and extraction tests passed\n"; return failures?1:0;
}
