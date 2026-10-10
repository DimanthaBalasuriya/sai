#include "engine/extensions/Features.hpp"
#ifdef ENGINE_TEST_DEBUG_DRAW
#include "engine/extensions/DebugDraw.hpp"
#endif
#include <iostream>
#include <limits>
#include <cmath>
using namespace Engine;
using namespace Engine::Extensions;
namespace {
int failures=0;
void Check(bool value,const char* message) { if(!value){++failures;std::cerr<<message<<'\n';} }
class Probe final:public IRenderFeature {
public:
    Probe(const char* name,int order,bool fail=false):m_name(name),m_order(order),m_fail(fail){}
    const char* Name() const noexcept override{return m_name;}int Order() const noexcept override{return m_order;}
    Result Initialize() override{return Result::Success();}
    Result AddPasses(Renderer::RenderGraph& graph,FeatureFrame& frame) override {
        if(m_fail)return Result::Failure("Probe failure");auto next=HdrImage(graph,m_name,frame.width,frame.height);
        graph.AddPass({m_name,{{frame.color,Rhi::ResourceState::SampledRead},{next,Rhi::ResourceState::ColorWrite}},{},{}});frame.color=next;return Result::Success();
    }
private:const char* m_name;int m_order;bool m_fail;
};
void RegistryAndGraph() {
    FeatureRegistry registry;Probe bloom("Bloom",30),sky("Sky",10),tone("Tone",40),duplicate("Sky",50),bad("Bad",20,true);
    Check(bool(registry.Register(bloom))&&bool(registry.Register(tone))&&bool(registry.Register(sky)),"Register modules");
    Check(!registry.Register(duplicate),"Reject duplicate feature names");
    Renderer::RenderGraph graph;FeatureFrame frame;frame.width=63;frame.height=31;frame.color=graph.ImportImage("HDR",{0,1},Rhi::ResourceState::SampledRead);
    Check(bool(registry.Build(graph,frame)),"Build chained feature graph");Renderer::CompiledGraph plan;Check(bool(graph.Compile(plan)),"Compile feature hazards");
    Check(graph.Passes().size()==3&&graph.Passes()[0].name=="Sky"&&graph.Passes()[1].name=="Bloom"&&graph.Passes()[2].name=="Tone","Order modules independently of registration");
    Check(graph.Passes()[1].uses[0].resource==graph.Passes()[0].uses[1].resource,"Chain consumes previous HDR output");
    Check(graph.Resources()[frame.color.value].desc.format==Rhi::Format::Rgba16Float,"Retain HDR format before output resolve");
    Check(registry.SetEnabled("Bloom",false)&&!registry.SetEnabled("Missing",true),"Toggle only registered features");
    Renderer::RenderGraph toggled;frame.color=toggled.ImportImage("HDR",{0,1},Rhi::ResourceState::SampledRead);Check(bool(registry.Build(toggled,frame))&&toggled.Passes().size()==2,"Disabled feature contributes no pass");
    registry.Remove(sky);Check(registry.Entries().size()==2,"Remove module registration");
    registry.Register(bad);Renderer::RenderGraph failed;frame.color=failed.ImportImage("HDR",{0,1},Rhi::ResourceState::SampledRead);Check(!registry.Build(failed,frame)&&failed.Passes().empty(),"Propagate feature failure and stop recording");
    Renderer::RenderGraph tiny;auto id=HdrImage(tiny,"Tiny",0,0);Check(tiny.Resources()[id.value].desc.width==1&&tiny.Resources()[id.value].desc.height==1,"Clamp tiny bloom extent");
}
void ModuleLifecycle() {
    FeatureRegistry features;EngineContext context;ModuleRegistry modules(context);
    modules.Add(std::make_unique<FeatureModule>(features,std::make_unique<Probe>("Sky",10)));
    Check(features.Entries().empty(),"No registration before module startup");Check(bool(modules.Startup())&&features.Entries().size()==1,"Startup registers owned feature");
    modules.Shutdown();Check(features.Entries().empty(),"Shutdown removes borrowed feature");
}
#ifdef ENGINE_TEST_DEBUG_DRAW
void DebugShapes() {
    DebugGeometry geometry;Mat4 matrix;matrix.values[12]=3;matrix.values[13]=4;matrix.values[14]=5;geometry.Box(matrix,{1,0,0});
    Check(geometry.indices.size()==24&&geometry.vertices.size()==24,"Box has twelve edges");
    Check(geometry.vertices[0].position.x==2&&geometry.vertices[0].position.y==3&&geometry.vertices[0].position.z==4,"Transform box into world space");
    auto previous=geometry.vertices.size();Check(!geometry.Sphere({},-1,{},32)&&!geometry.Sphere({},1,{},2)&&!geometry.Sphere({},std::numeric_limits<float>::quiet_NaN(),{}),"Reject invalid spheres");
    Check(geometry.vertices.size()==previous,"Invalid sphere preserves geometry");DebugGeometry sphere;Check(bool(sphere.Sphere({2,3,4},2,{0,1,0},16))&&sphere.indices.size()==96,"Sphere has three closed rings");
    for(auto& vertex:sphere.vertices){auto p=vertex.position;float distance=std::sqrt((p.x-2)*(p.x-2)+(p.y-3)*(p.y-3)+(p.z-4)*(p.z-4));Check(std::abs(distance-2)<0.0001f,"Sphere points lie on radius");}
    for(auto index:sphere.indices)Check(index<sphere.vertices.size(),"Valid debug indices");
}
#endif
}
int main(){RegistryAndGraph();ModuleLifecycle();
#ifdef ENGINE_TEST_DEBUG_DRAW
DebugShapes();
#endif
return failures?1:0;}
