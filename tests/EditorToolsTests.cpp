#include "SceneTools.h"
#include "HierarchyModel.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>
using namespace Engine;using namespace Engine::Scene;using namespace EditorTools;
namespace {
int failures=0;void Check(bool value,const char* message){if(!value){++failures;std::cerr<<message<<'\n';}}
float X(World& world,Guid id){Mat4 matrix;world.GlobalTransform(id,matrix);return matrix.values[12];}
std::size_t Count(World& world){std::vector<Guid> ids;world.Entities(ids);return ids.size();}
}
int main(){
    World world;CommandHistory history;Guid parent{9,1},child{9,2},light{9,3};EntityDefinition root;root.id=parent;root.mesh=MeshRenderer{};root.transform.position={2,0,0};
    EntityDefinition leaf;leaf.id=child;leaf.hierarchy.parent=parent;leaf.mesh=MeshRenderer{};leaf.transform.position={3,0,0};
    EntityDefinition lamp;lamp.id=light;lamp.hierarchy.parent=parent;lamp.light=Light{};
    Check(bool(history.Execute(world,std::make_unique<ReplaceSceneCommand>(std::vector{root,leaf,lamp}))),"build hierarchy");history.Clear();
    Selection selection;selection.Click(parent,false,false);selection.Click(child,true,false);selection.Click(parent,false,true);Check(selection.ids.size()==1&&selection.Primary()==child,"add and toggle selection");
    std::vector<Guid> selected{parent,child};TransformDrag drag;auto pivot=glm::translate(glm::mat4(1),glm::vec3(2,0,0));
    Check(bool(drag.Begin(world,selected,pivot,false))&&bool(drag.Update(glm::translate(pivot,glm::vec3(4,0,0)))),"begin pending transform");
    Renderer::RenderWorld preview;world.Extract(preview);drag.Preview(preview);auto moved=std::find_if(preview.instances.begin(),preview.instances.end(),[&](auto& i){return i.entity==child;});
    Check(moved!=preview.instances.end()&&std::abs(moved->model.values[12]-9)<.001f&&X(world,child)==5&&!history.CanUndo(),"preview moves descendant once without mutating world or history");
    Visibility visibility;visibility.Filter(world,preview,&drag);Check(preview.lights.size()==1&&std::abs(preview.lights[0].position.x-6)<.001f,"light previews follow parent drag");
    drag.Cancel();Check(X(world,child)==5&&!history.CanUndo(),"cancel produces no undo entry");
    drag.Begin(world,selected,pivot,false);drag.Update(glm::translate(pivot,glm::vec3(4,0,0)));Check(bool(drag.Commit(world,history))&&X(world,child)==9,"commit drag");
    Check(bool(history.Undo(world))&&X(world,child)==5&&!history.CanUndo(),"whole group drag is one undo");Check(bool(history.Redo(world))&&X(world,child)==9,"drag redo");history.Undo(world);history.Clear();
    drag.Begin(world,selected,pivot,false);drag.Update(glm::rotate(pivot,glm::radians(90.f),glm::vec3(0,0,1)));Check(bool(drag.Commit(world,history)),"rotation drag commit");Mat4 rotated;world.GlobalTransform(child,rotated);Check(std::abs(rotated.values[12]-2)<.001f&&std::abs(rotated.values[13]-3)<.001f,"group rotation preserves parent-child relationship");history.Undo(world);history.Clear();
    drag.Begin(world,selected,pivot,false);drag.Update(glm::scale(pivot,glm::vec3(2)));Check(bool(drag.Commit(world,history))&&std::abs(X(world,child)-8)<.001f,"group scaling moves descendant once");history.Undo(world);history.Clear();
    drag.Begin(world,selected,pivot,false);drag.Update(glm::translate(pivot,glm::vec3(1,0,0)));history.Execute(world,std::make_unique<SetFieldCommand>(parent,ComponentType::Name,"value",std::string("Changed")));
    Check(!drag.Commit(world,history)&&X(world,child)==5&&!drag.Active(),"stale drag refuses to overwrite concurrent command");history.Clear();
    std::vector<EntityDefinition> clipboard;std::vector<Guid> roots;Check(bool(Copy(world,std::vector<Guid>{child},clipboard))&&clipboard.size()==1&&!clipboard[0].hierarchy.parent&&clipboard[0].transform.position.x==5,"copy child preserves world placement");
    Check(bool(Copy(world,std::vector<Guid>{parent},clipboard))&&clipboard.size()==3,"copy includes complete subtree");Check(bool(Paste(world,history,clipboard,roots))&&Count(world)==6&&roots.size()==1&&roots[0]!=parent,"paste remaps identities");
    auto branch=Expand(world,roots);Check(branch.size()==3,"pasted hierarchy stays connected");Check(bool(history.Undo(world))&&Count(world)==3&&!history.CanUndo(),"paste one undo");history.Redo(world);history.Clear();
    Check(bool(Delete(world,history,roots))&&Count(world)==3,"delete complete branch");Check(bool(history.Undo(world))&&Count(world)==6,"undo restores branch identities");history.Clear();
    Check(bool(visibility.Toggle(world,history,std::vector<Guid>{parent})),"hide branch");Renderer::RenderWorld hidden;world.Extract(hidden);visibility.Filter(world,hidden);Check(hidden.instances.size()==2&&hidden.lights.size()==1,"hidden branch removes meshes and its light");
    Check(bool(history.Undo(world))&&!visibility.Hidden(child),"hide undo restores inherited visibility");
    Geometry geometry;Renderer::RenderWorld render;world.Extract(render);std::erase_if(render.instances,[&](auto& i){return i.entity!=parent&&i.entity!=child;});auto hit=geometry.Pick(render,{{2,0,10},{0,0,-1}});Check(hit&&hit.id==parent&&std::abs(hit.distance-9)<.001f,"nearest built-in triangle raycast");
    auto bounds=geometry.SelectionBounds(render,std::vector<Guid>{parent,child});Check(bounds.valid&&bounds.minimum.x==1&&bounds.maximum.x==6,"world bounds combine selection");
    auto box=geometry.Box(render,glm::mat4(1),{0,0},{.9f,1});Check(box.empty(),"box excludes projected objects outside rectangle");
    box=geometry.Box(render,glm::translate(glm::mat4(1),glm::vec3(-2,0,0)),{0,0},{1,1});Check(std::find(box.begin(),box.end(),parent)!=box.end(),"box includes overlapping projected bounds");
    Check(!geometry.Pick(render,{{2,0,10},{0,0,-1}},std::vector<Guid>{parent}),"surface snap ray excludes moving entities");
    auto ray=ScreenRay(glm::mat4(1),{.5f,.5f});Check(glm::length(ray.origin)<.001f&&ray.direction.z>0,"screen ray zero-to-one depth");
    selection.ids.push_back({99,99});selection.Prune(world);Check(!selection.Contains({99,99}),"selection prunes deleted identities");
    World independent;CommandHistory separate;EntityDefinition a,b;a.id={20,1};b.id={20,2};a.transform.position={-2,0,0};b.transform.position={2,0,0};separate.Execute(independent,std::make_unique<ReplaceSceneCommand>(std::vector{a,b}));separate.Clear();
    std::vector<Guid> pair{a.id,b.id};drag.Begin(independent,pair,glm::mat4(1),true);drag.Update(glm::rotate(glm::mat4(1),glm::radians(90.f),glm::vec3(0,0,1)));Check(bool(drag.Commit(independent,separate))&&std::abs(X(independent,a.id)+2)<.001f&&std::abs(X(independent,b.id)-2)<.001f,"individual rotation keeps separate origins");EntityDefinition definition;independent.Definition(a.id,definition);Check(std::abs(definition.transform.rotation.z-glm::radians(90.f))<.001f,"individual rotation changes orientation");
    World large;std::vector<EntityDefinition> many(100000);for(std::size_t i=0;i<many.size();++i)many[i].id={30,i+1};EntityBatchCommand createLarge(std::move(many));Check(bool(createLarge.Do(large)),"create 100k hierarchy fixture");
    HierarchyModel model;Check(bool(model.Refresh(large))&&model.Rows().size()==100000,"flat cached hierarchy has 100k rows");auto rebuilds=model.Rebuilds();SetTransformCommand move({30,1},Transform{{1,0,0}});Check(bool(move.Do(large))&&bool(model.Refresh(large))&&model.Rebuilds()==rebuilds,"transform edits do not rebuild hierarchy");
    ReparentCommand branchParent({30,2},{30,1});Check(bool(branchParent.Do(large))&&bool(model.Refresh(large))&&model.Rows().size()==99999,"collapsed branch omits descendants lazily");model.SetOpen({30,1},true);Check(model.Rows().size()==100000,"expansion rebuilds cached visible rows");
    SetFieldCommand rename({30,2},ComponentType::Name,"value",std::string("Needle"));Check(bool(rename.Do(large))&&bool(model.Refresh(large)),"rename invalidates hierarchy cache");model.Filter("Needle");Check(model.Rows().size()==1&&model.Entry(model.Rows()[0]).id==Guid{30,2},"cached hierarchy search");model.Filter("");model.SetOpen({30,1},false);model.Reveal({30,2});Check(model.Rows().size()==100000,"reveal opens ancestors");
    return failures?1:0;
}
