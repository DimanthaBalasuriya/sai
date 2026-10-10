#include "engine/extensions/DebugDraw.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
namespace Engine::Extensions {
void DebugGeometry::Line(Vec3 start,Vec3 end,Vec3 color) { auto first=static_cast<std::uint32_t>(vertices.size());vertices.push_back({start,color});vertices.push_back({end,color});indices.push_back(first);indices.push_back(first+1); }
void DebugGeometry::Box(const Mat4& transform,Vec3 color) {
    auto matrix=glm::make_mat4(transform.values.data());std::array<Vec3,8> corners;
    for (int i=0;i<8;++i) { auto p=matrix*glm::vec4(i&1?1.0f:-1.0f,i&2?1.0f:-1.0f,i&4?1.0f:-1.0f,1);corners[i]={p.x,p.y,p.z}; }
    for (int i=0;i<8;++i) for (int bit:{1,2,4}) if (!(i&bit)) Line(corners[i],corners[i|bit],color);
}
Result DebugGeometry::Sphere(Vec3 center,float radius,Vec3 color,std::uint32_t segments) {
    if (!std::isfinite(radius)||radius<=0||segments<3||segments>256) return Result::Failure("Invalid debug sphere");
    for (int plane=0;plane<3;++plane) for (std::uint32_t i=0;i<segments;++i) {
        auto point=[&](std::uint32_t step) { float angle=6.28318530718f*step/segments,a=std::cos(angle)*radius,b=std::sin(angle)*radius;return Vec3{center.x+(plane==0?0:a),center.y+(plane==1?0:plane==0?a:b),center.z+(plane==2?0:b)}; };
        Line(point(i),point(i+1),color);
    }return Result::Success();
}
namespace {
class DebugDraw final:public IRenderFeature {
public:
    DebugDraw(Rhi::Device& device,std::filesystem::path shaders):m_device(device),m_shaders(std::move(shaders)){}
    ~DebugDraw() { for (auto b:m_vertices) if(b)m_device.Destroy(b);for(auto b:m_indices)if(b)m_device.Destroy(b);if(m_table)m_device.Destroy(m_table);if(m_pipeline)m_device.Destroy(m_pipeline); }
    const char* Name() const noexcept override { return "Debug Draw"; }int Order() const noexcept override { return 20; }
    Result Initialize() override {
        Rhi::PipelineDesc desc;desc.name="Debug x-ray lines";desc.vertexShader=m_shaders/"debugdraw.vertex.spv";desc.fragmentShader=m_shaders/"debugdraw.fragment.spv";desc.colorFormat=Rhi::Format::Rgba16Float;desc.lines=true;desc.vertexStride=sizeof(DebugVertex);desc.attributes={{0,0,Rhi::VertexFormat::Float3},{1,12,Rhi::VertexFormat::Float3}};desc.pushConstantBytes=sizeof(glm::mat4);
        auto result=m_device.CreatePipeline(desc,m_pipeline);if(!result)return result;result=m_device.CreateTextureTable("Debug empty table",m_table);if(!result)return result;
        for(std::size_t i=0;i<Rhi::kFramesInFlight;++i) {
            result=m_device.CreateBuffer({"Debug vertices",8192*sizeof(DebugVertex),Rhi::BufferUsage::Vertex,true},m_vertices[i]);if(!result)return result;
            result=m_device.CreateBuffer({"Debug indices",8192*sizeof(std::uint32_t),Rhi::BufferUsage::Index,true},m_indices[i]);if(!result)return result;
        }return Result::Success();
    }
    Result AddPasses(Renderer::RenderGraph& graph,FeatureFrame& frame) override {
        DebugGeometry geometry;
        for(int i=-10;i<=10;++i) { float p=static_cast<float>(i);geometry.Line({p,0,-10},{p,0,10},{0.15f,0.25f,0.3f});geometry.Line({-10,0,p},{10,0,p},{0.15f,0.25f,0.3f}); }
        geometry.Line({0,0,0},{3,0,0},{2,0.1f,0.1f});geometry.Line({0,0,0},{0,3,0},{0.1f,2,0.1f});geometry.Line({0,0,0},{0,0,3},{0.1f,0.3f,2});
        for (auto& instance:frame.world->instances) if (instance.entity==frame.selected||(!frame.selected&& &instance==&frame.world->instances.front())) {
            geometry.Box(instance.model,{2,1,0.1f});const auto& m=instance.model.values;geometry.Sphere({m[12],m[13],m[14]},1,{0.1f,1,2});break;
        }
        auto result=m_device.WriteBuffer(m_vertices[frame.slot],std::as_bytes(std::span{geometry.vertices}));if(!result)return result;
        result=m_device.WriteBuffer(m_indices[frame.slot],std::as_bytes(std::span{geometry.indices}));if(!result)return result;
        auto vertices=graph.ImportBuffer("Debug vertices",m_vertices[frame.slot],m_device.StateOf(m_vertices[frame.slot]));auto indices=graph.ImportBuffer("Debug indices",m_indices[frame.slot],m_device.StateOf(m_indices[frame.slot]));
        auto color=frame.color;auto view=frame.viewProjection;auto count=static_cast<std::uint32_t>(geometry.indices.size());
        graph.AddPass({"Debug Draw",{{color,Rhi::ResourceState::ColorWrite},{vertices,Rhi::ResourceState::VertexRead},{indices,Rhi::ResourceState::IndexRead}},[this,color,vertices,indices,view,count](Rhi::CommandList& commands,const Renderer::PassResources& resources) {
            Rhi::RenderingDesc rendering{resources.Image(color),{}};rendering.loadColor=true;auto result=commands.BeginRendering(rendering);if(!result)return result;
            result=commands.BindPipeline(m_pipeline);if(!result)return result;result=commands.BindTextureTable(m_table);if(!result)return result;result=commands.BindMesh(resources.Buffer(vertices),resources.Buffer(indices));if(!result)return result;
            result=commands.PushConstants(std::as_bytes(std::span{&view,1}));if(!result)return result;result=commands.DrawIndexed(count);return result?commands.EndRendering():result;
        },{}});return Result::Success();
    }
private:
    Rhi::Device& m_device;std::filesystem::path m_shaders;Rhi::PipelineHandle m_pipeline;Rhi::TextureTableHandle m_table;
    std::array<Rhi::BufferHandle,Rhi::kFramesInFlight> m_vertices{},m_indices{};
};
}
std::unique_ptr<IModule> MakeDebugDrawModule(FeatureRegistry& registry,Rhi::Device& device,const std::filesystem::path& shaders) { return std::make_unique<FeatureModule>(registry,std::make_unique<DebugDraw>(device,shaders)); }
}
