#include "engine/scene/Commands.hpp"
#include "engine/core/SpatialIndex.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <atomic>
#include <malloc.h>
static std::atomic<std::size_t> allocations=0;
static thread_local bool counting=false;
void* operator new(std::size_t size){if(counting)++allocations;if(auto p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
void* operator new(std::size_t size,std::align_val_t alignment){if(counting)++allocations;if(auto p=_aligned_malloc(size?size:1,static_cast<std::size_t>(alignment)))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size,std::align_val_t alignment){return ::operator new(size,alignment);}
void operator delete(void* p,std::align_val_t)noexcept{_aligned_free(p);}
void operator delete[](void* p,std::align_val_t)noexcept{_aligned_free(p);}
void operator delete(void* p,std::size_t,std::align_val_t)noexcept{_aligned_free(p);}
void operator delete[](void* p,std::size_t,std::align_val_t)noexcept{_aligned_free(p);}
using namespace Engine;
using namespace Engine::Scene;
static void Check(Result r){if(!r){std::fprintf(stderr,"%s\n",r.Error().c_str());std::exit(1);}}
int main(){
 for(bool deep:{false,true}){
  World world; std::vector<EntityDefinition> defs(100000);
  for(std::size_t i=0;i<defs.size();++i){auto& d=defs[i];d.id={1,i+1};d.mesh=MeshRenderer{};d.transform.position={float(i%100),0,float(i/100)};if(deep&&i%10)d.hierarchy.parent={1,i};}
  ReplaceSceneCommand create(std::move(defs));Check(create.Do(world));Renderer::RenderWorld snapshot;Check(world.Extract(snapshot));Mat4 matrix;
  auto measure=[&](const char* name){allocations=0;counting=true;auto start=std::chrono::steady_clock::now();Check(world.GlobalTransform({1,1},matrix));auto update=std::chrono::steady_clock::now();Check(world.Extract(snapshot));auto end=std::chrono::steady_clock::now();counting=false;std::printf("%s,%s,update_ms=%.6f,extract_ms=%.6f,allocations=%zu,instances=%zu\n",deep?"depth10":"flat",name,std::chrono::duration<double,std::milli>(update-start).count(),std::chrono::duration<double,std::milli>(end-update).count(),allocations.load(),snapshot.instances.size());};
  measure("idle");
  for(int frame=0;frame<5;++frame){for(std::uint64_t i=0;i<1000;++i){Transform t;t.position={float(i),float(frame+1),0};SetTransformCommand edit({1,deep?i*10+10:i+2},t);Check(edit.Do(world));}measure("moving1pct");}
  auto start=std::chrono::steady_clock::now();for(std::uint64_t i=11;i<=100000;i+=10){ReparentCommand edit({1,i},{1,1});Check(edit.Do(world));}std::printf("%s,reparent_commands_ms=%.3f\n",deep?"depth10":"flat",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());measure("reparent10k");
 }
 Spatial::SpatialIndex index;std::vector<Spatial::Aabb> bounds(100000);std::vector<Spatial::SpatialIndex::Proxy> proxies;proxies.reserve(bounds.size());std::vector<std::uint32_t> hits;hits.reserve(bounds.size());
 for(std::uint32_t i=0;i<bounds.size();++i){float x=float(i%316)*3,z=float(i/316)*3;bounds[i]={{x-1,-1,z-1},{x+1,1,z+1}};proxies.push_back(index.Insert(bounds[i],i));}
 auto spatial=[&](const char* name,auto operation){allocations=0;counting=true;auto start=std::chrono::steady_clock::now();operation();auto end=std::chrono::steady_clock::now();counting=false;std::printf("spatial100k,%s,ms=%.6f,allocations=%zu,reinserts=%llu,hits=%zu\n",name,std::chrono::duration<double,std::milli>(end-start).count(),allocations.load(),static_cast<unsigned long long>(index.Reinserts()),hits.size());if(allocations.load())std::exit(1);};
 spatial("inside_fat1pct",[&]{for(std::size_t i=0;i<1000;++i){auto b=bounds[i];b.minimum.x+=.01f;b.maximum.x+=.01f;index.Update(proxies[i],b);}});
 spatial("escape_fat1pct",[&]{for(std::size_t i=0;i<1000;++i){auto b=bounds[i];b.minimum.x+=1;b.maximum.x+=1;index.Update(proxies[i],b);}});
 spatial("ray",[&]{index.Query(Spatial::SpatialRay{{300,10,300},{0,-1,0}},hits);});
 spatial("box",[&]{index.Query(Spatial::Aabb{{100,-2,100},{200,2,200}},hits);});
 const std::array<Spatial::Plane,6> planes{Spatial::Plane{{1,0,0},-100},Spatial::Plane{{-1,0,0},200},Spatial::Plane{{0,1,0},2},Spatial::Plane{{0,-1,0},2},Spatial::Plane{{0,0,1},-100},Spatial::Plane{{0,0,-1},200}};
 spatial("frustum",[&]{index.Query(planes,hits);});
}
