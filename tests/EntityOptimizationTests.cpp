#include "engine/scene/Commands.hpp"
#include "engine/scene/EntityFeatures.hpp"
#include "engine/scene/Serialization.hpp"
#include "engine/core/SpatialIndex.hpp"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <random>
using namespace Engine;using namespace Engine::Scene;
static void Check(bool value,const char* message){if(!value){std::fprintf(stderr,"%s\n",message);std::exit(1);}}
static void Check(Result result){if(!result){std::fprintf(stderr,"%s\n",result.Error().c_str());std::exit(1);}}
static const Renderer::RenderInstance& Instance(const Renderer::RenderWorld& world,Guid id){auto it=std::find_if(world.instances.begin(),world.instances.end(),[&](auto& i){return i.entity==id;});Check(it!=world.instances.end(),"Missing extracted instance");return *it;}
int main(){
 Check(RegisterEntityFeatures());World world;CommandHistory history;EntityDefinition parent,child,leaf;
 parent.id={1,1};parent.mesh=MeshRenderer{};parent.mesh->color={.2f,.3f,.4f};parent.light=Light{};parent.light->intensity=7;
 ModuleComponent flags;Check(DefaultComponent(kEntityFlags,flags));flags.fields[1]=std::uint32_t{1};parent.extensions.push_back(flags);
 child.id={1,2};child.hierarchy.parent=parent.id;child.mesh=MeshRenderer{};child.light=Light{};child.transform.position={2,0,0};
 ModuleComponent inherit;Check(DefaultComponent(kPropertyInheritance,inherit));for(auto& f:inherit.fields)f=std::uint32_t{1};child.extensions.push_back(inherit);
 leaf.id={1,3};leaf.mesh=MeshRenderer{};leaf.hierarchy.parent=child.id;leaf.transform.position={3,0,0};
 Check(history.Execute(world,std::make_unique<EntityBatchCommand>(std::vector{leaf,child,parent})));
 Renderer::RenderWorld snapshot;Check(world.Extract(snapshot));auto source=snapshot.source;
 Check(world.Statistics().staticEntities==3,"Static inheritance");Check(Instance(snapshot,child.id).color==parent.mesh->color,"Effective mesh color");
 Check(SaveScene(world,"entity-flags-roundtrip.json"));World loaded;CommandHistory loadHistory;std::vector<Guid> assets;Check(LoadScene(loaded,loadHistory,"entity-flags-roundtrip.json",assets));Renderer::RenderWorld loadedSnapshot;Check(loaded.Extract(loadedSnapshot));Check(loaded.Statistics().staticEntities==3&&Instance(loadedSnapshot,child.id).color==parent.mesh->color,"Flags/inheritance survive serialization");std::filesystem::remove("entity-flags-roundtrip.json");
 EntityDefinition authored;Check(world.Definition(child.id,authored));Check(authored.mesh->color==Vec3{1,1,1},"Inheritance must retain authored values");
 Check(snapshot.lights.size()==2&&snapshot.lights[0].intensity==7&&snapshot.lights[1].intensity==7,"Effective light intensity");
 Check(history.Execute(world,std::make_unique<SetFieldCommand>(child.id,ComponentType::Light,"range",44.f)));Check(world.Extract(snapshot));Check(snapshot.changedLights.size()==1&&!snapshot.fullUpdate,"Only changed lights transfer into the owned snapshot");
 Check(world.Update());Check(world.Statistics().updated==0,"Idle skips static subtrees");
 Check(history.Execute(world,std::make_unique<SetFieldCommand>(parent.id,ComponentType::Transform,"position",Vec3{4,0,0})));
 Check(world.Update());Check(world.Statistics().updated==3,"Dirty ancestor updates descendants once");Check(world.Extract(snapshot));Check(Instance(snapshot,leaf.id).model.values[12]==9,"Parent-first global transform");
 auto old=snapshot;Check(history.Execute(world,std::make_unique<SetFieldCommand>(child.id,ComponentType::Transform,"position",Vec3{5,0,0})));Check(world.Extract(snapshot));
 Check(!snapshot.fullUpdate&&snapshot.changedInstances.size()==2,"Extraction visits dirty instances only");Check(Instance(old,leaf.id).model.values[12]==9,"Owned snapshots stay independent");
 Check(history.Execute(world,std::make_unique<SetFieldCommand>(child.id,ComponentType::Transform,"position",Vec3{6,0,0})));Check(world.Extract(snapshot));Check(world.Extract(old));Check(Instance(old,leaf.id).model.values[12]==13,"Stale consumer catches up safely");
 snapshot.modified=true;snapshot.instances.clear();Check(world.Extract(snapshot));Check(snapshot.instances.size()==3&&!snapshot.modified,"Restore filtered/previewed snapshot");
 auto hierarchyVersion=world.HierarchyVersion();Check(history.Execute(world,std::make_unique<SetFieldCommand>(child.id,ComponentType::Name,"value",std::string("Rename"))));Check(world.HierarchyVersion()>hierarchyVersion,"Rename invalidates hierarchy labels");Check(world.Update());Check(world.Statistics().updated==0,"Names do not dirty transforms");
 Check(history.Execute(world,std::make_unique<SetFieldCommand>(parent.id,kEntityFlags,"visible",std::uint32_t{0})));Check(world.Extract(snapshot));Check(snapshot.instances.empty()&&snapshot.lights.empty(),"Hidden ancestors suppress descendants");Check(history.Undo(world));Check(world.Extract(snapshot));Check(snapshot.instances.size()==3,"Visibility undo");
 std::vector<Guid> descendants;Check(world.Descendants(std::array{parent.id,child.id},descendants));Check(descendants.size()==3,"Overlapping subtree selection is unique");
 auto revision=world.Revision();EntityBatchCommand invalid({},std::vector{parent.id});Check(!invalid.Do(world)&&world.Revision()==revision,"Invalid batch has no partial removal");
 history.Clear();Check(history.Execute(world,std::make_unique<EntityBatchCommand>(std::vector<EntityDefinition>{},descendants)));Check(world.EntityCount()==0,"Batch delete");Check(history.Undo(world));Check(world.Extract(snapshot));Check(snapshot.source==source&&snapshot.instances.size()==3&&!history.CanUndo(),"Batch undo preserves world identity");
 std::vector<Guid> moduleEntities;Check(world.EntitiesWithComponent(kEntityFlags,moduleEntities));Check(moduleEntities.size()==1&&moduleEntities[0]==parent.id,"Module index survives undo");
 child.extensions.push_back({kEntityFlags,flags.fields});child.extensions.back().fields[0]=std::uint32_t{1};child.hierarchy.parent={};child.id={8,1};leaf.id={8,2};leaf.hierarchy.parent=child.id;
 // An internal module GUID link is remapped; external asset GUIDs remain authored.
 const auto linkType=static_cast<ComponentType>(8194);const std::array fields{FieldInfo{"target",FieldType::Guid,0,0,0}};const std::array<FieldValue,1> defaults{Guid{}};Check(RegisterComponent(linkType,"PrefabLink",fields,defaults));child.extensions.push_back({linkType,{leaf.id}});
 std::vector<Guid> roots;history.Clear();Check(InstantiatePrefab(world,history,std::array{leaf,child},roots));Check(roots.size()==1,"Prefab root mapping");Check(world.Definition(roots[0],authored));auto link=std::get<Guid>(authored.extensions.back().fields[0]);Check(world.Contains(link)&&link!=leaf.id,"Internal module link remapping");Check(history.Undo(world));Check(!world.Contains(link)&&world.EntityCount()==3,"Prefab is one undo entry");
 World deep;std::vector<EntityDefinition> chain(1025);for(std::size_t i=0;i<chain.size();++i){chain[i].id={2,i+1};if(i)chain[i].hierarchy.parent=chain[i-1].id;}EntityBatchCommand makeChain(std::move(chain));Check(makeChain.Do(deep));ReparentCommand tooDeep(parent.id,{2,1025});Check(!tooDeep.Do(deep),"Missing child rejection");EntityDefinition overflow;overflow.id={2,1026};overflow.hierarchy.parent={2,1025};CreateEntityCommand overflowCommand(overflow);Check(!overflowCommand.Do(deep)&&deep.EntityCount()==1025,"Depth bound enforced before creation");ReparentCommand cycle({2,1},{2,1025});Check(!cycle.Do(deep),"Cycle rejection uses handles");
 EntityDefinition extraRoot;extraRoot.id={2,2000};CreateEntityCommand extra(extraRoot);Check(extra.Do(deep));ReparentCommand depthOverflow(extraRoot.id,{2,1025});auto deepRevision=deep.Revision();Check(!depthOverflow.Do(deep)&&deep.Revision()==deepRevision,"Reparent rejects excessive descendant depth before mutation");
 Spatial::SpatialIndex tree;std::mt19937 random(123);std::vector<Spatial::Aabb> boxes(5000);std::vector<Spatial::SpatialIndex::Proxy> proxies;std::vector<std::uint32_t> result;result.reserve(boxes.size());
 for(std::uint32_t i=0;i<boxes.size();++i){float x=float(random()%1000),y=float(random()%1000),z=float(random()%1000);boxes[i]={{x,y,z},{x+2,y+2,z+2}};proxies.push_back(tree.Insert(boxes[i],i));}
 for(int query=0;query<100;++query){float x=float(random()%900),y=float(random()%900),z=float(random()%900);Spatial::Aabb box{{x,y,z},{x+100,y+100,z+100}};tree.Query(box,result);for(std::uint32_t i=0;i<boxes.size();++i){auto b=boxes[i];bool overlap=b.minimum.x<=box.maximum.x&&b.minimum.y<=box.maximum.y&&b.minimum.z<=box.maximum.z&&b.maximum.x>=box.minimum.x&&b.maximum.y>=box.minimum.y&&b.maximum.z>=box.minimum.z;if(overlap)Check(std::find(result.begin(),result.end(),i)!=result.end(),"Balanced spatial tree has no false negatives");}}
 auto moved=boxes[0];moved.minimum.x+=.01f;moved.maximum.x+=.01f;Check(!tree.Update(proxies[0],moved)&&tree.Reinserts()==0,"Fat box absorbs small motion");moved={{-2,-2,-2},{2,2,2}};Check(tree.Update(proxies[0],moved)&&tree.Reinserts()==1,"Escaping bounds reinsert");tree.Query(Spatial::SpatialRay{{0,0,10},{0,0,-1}},result);Check(std::find(result.begin(),result.end(),0)!=result.end(),"Parallel-axis ray query");
 for(std::size_t i=0;i<proxies.size();i+=2)tree.Remove(proxies[i]);Check(tree.Size()==2500,"Batch spatial removal");tree.Query(Spatial::Aabb{{-10,-10,-10},{2000,2000,2000}},result);Check(result.size()==2500&&std::all_of(result.begin(),result.end(),[](auto i){return i%2==1;}),"Rotations/removal retain payloads");
 std::puts("Entity optimization, inheritance, batches and spatial tests passed");
}
