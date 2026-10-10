#include "engine/extensions/Features.hpp"
#include "engine/core/Log.hpp"
#include "engine/renderer/Projection.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <exception>
namespace Engine::Extensions {
Result FeatureRegistry::Register(IRenderFeature& feature) {
    if (std::any_of(m_entries.begin(),m_entries.end(),[&](auto& entry){return std::string_view(entry.feature->Name())==feature.Name();})) return Result::Failure("Duplicate render feature name");
    m_entries.push_back({&feature,true});std::stable_sort(m_entries.begin(),m_entries.end(),[](auto& a,auto& b){return a.feature->Order()<b.feature->Order();});return Result::Success();
}
void FeatureRegistry::Remove(IRenderFeature& feature) noexcept { std::erase_if(m_entries,[&](auto& entry){return entry.feature==&feature;}); }
bool FeatureRegistry::SetEnabled(std::string_view name,bool enabled) noexcept { for (auto& entry:m_entries) if (entry.feature->Name()==name) { entry.enabled=enabled;return true; }return false; }
Result FeatureRegistry::Build(Renderer::RenderGraph& graph,FeatureFrame& frame) {
    m_postEntries=m_entries;std::stable_sort(m_postEntries.begin(),m_postEntries.end(),[](auto& a,auto& b){return a.feature->PostOrder()<b.feature->PostOrder();});
    for (auto& entry:m_postEntries) if (entry.enabled) { auto result=entry.feature->AddPasses(graph,frame);if (!result) return result; }return Result::Success();
}
Result FeatureRegistry::PrepareFrame(){for(auto& e:m_entries){auto r=e.feature->PrepareFrame();if(!r)return r;}return Result::Success();}
Result FeatureRegistry::BuildBeforeGeometry(Renderer::RenderGraph& g,FeatureFrame& f,Renderer::RenderWorld& w){for(auto& e:m_entries)if(e.enabled){auto r=e.feature->BeforeGeometry(g,f,w);if(!r)return r;}return Result::Success();}
Result FeatureModule::OnStartup(EngineContext&) noexcept { try { auto result=m_feature->Initialize();return result?m_registry.Register(*m_feature):result; }catch(const std::exception& error){return Result::Failure(error.what());} }
void FeatureModule::OnShutdown(EngineContext&) noexcept { m_registry.Remove(*m_feature); }
Renderer::ResourceId HdrImage(Renderer::RenderGraph& graph,std::string name,std::uint32_t width,std::uint32_t height) {
    return graph.CreateImage({std::move(name),std::max(1u,width),std::max(1u,height),Rhi::Format::Rgba16Float,Rhi::ImageUsage::ColorAttachment|Rhi::ImageUsage::Sampled});
}
FullscreenPass::~FullscreenPass() {
    for (auto table:m_tables) if (table) m_device.Destroy(table);
    if (m_pipeline) m_device.Destroy(m_pipeline);if (m_vertices) m_device.Destroy(m_vertices);if (m_indices) m_device.Destroy(m_indices);if (m_sampler) m_device.Destroy(m_sampler);
}
Result FullscreenPass::Initialize(const std::filesystem::path& shaders,std::string_view shader,Rhi::Format format) {
    Rhi::PipelineDesc desc;desc.name=std::string(shader);desc.vertexShader=shaders/"fullscreen.vertex.spv";desc.fragmentShader=shaders/(std::string(shader)+".fragment.spv");desc.colorFormat=format;desc.pushConstantBytes=sizeof(PassData);
    auto result=m_device.CreatePipeline(desc,m_pipeline);if (!result) return result;
    std::array<float,3> unused{};std::array<std::uint32_t,3> indices{0,1,2};
    result=m_device.CreateBuffer({"Fullscreen placeholder",sizeof(unused),Rhi::BufferUsage::Vertex|Rhi::BufferUsage::TransferDestination},m_vertices);if (!result) return result;
    result=m_device.UploadBuffer(m_vertices,std::as_bytes(std::span{unused}),Rhi::ResourceState::VertexRead);if (!result) return result;
    result=m_device.CreateBuffer({"Fullscreen triangle indices",sizeof(indices),Rhi::BufferUsage::Index|Rhi::BufferUsage::TransferDestination},m_indices);if (!result) return result;
    result=m_device.UploadBuffer(m_indices,std::as_bytes(std::span{indices}),Rhi::ResourceState::IndexRead);if (!result) return result;
    result=m_device.CreateSampler({"Postprocess clamp sampler",true,false},m_sampler);if (!result) return result;
    for (auto& table:m_tables) { result=m_device.CreateTextureTable(shader,table);if (!result) return result; }return Result::Success();
}
Result FullscreenPass::Add(Renderer::RenderGraph& graph,std::string name,std::span<const Renderer::ResourceId> sources,Renderer::ResourceId destination,const FeatureFrame& frame,PassData data,std::span<const Renderer::ResourceUse> extraUses) {
    using namespace Renderer;
    std::vector<ResourceId> inputs(sources.begin(),sources.end());std::vector<ResourceUse> uses;
    for (auto input:inputs) if(std::none_of(uses.begin(),uses.end(),[&](auto& u){return u.resource==input;}))uses.push_back({input,Rhi::ResourceState::SampledRead});uses.push_back({destination,Rhi::ResourceState::ColorWrite});uses.insert(uses.end(),extraUses.begin(),extraUses.end());
    auto vertices=graph.ImportBuffer("Fullscreen vertices",m_vertices,m_device.StateOf(m_vertices));auto indices=graph.ImportBuffer("Fullscreen indices",m_indices,m_device.StateOf(m_indices));
    uses.push_back({vertices,Rhi::ResourceState::VertexRead});uses.push_back({indices,Rhi::ResourceState::IndexRead});auto slot=frame.slot;
    auto pass=graph.AddPass({std::move(name),std::move(uses),[this,inputs,destination,vertices,indices,slot,data](Rhi::CommandList& commands,const PassResources& resources) {
        for (std::uint32_t i=0;i<inputs.size();++i) { auto result=m_device.WriteTexture(m_tables[slot],i,resources.Image(inputs[i]),m_sampler);if (!result) return result; }
        auto result=commands.BeginRendering({resources.Image(destination),{}});if (!result) return result;
        result=commands.BindPipeline(m_pipeline);if (!result) return result;result=commands.BindTextureTable(m_tables[slot]);if (!result) return result;
        result=commands.BindMesh(resources.Buffer(vertices),resources.Buffer(indices));if (!result) return result;result=commands.PushConstants(std::as_bytes(std::span{&data,1}));if (!result) return result;
        result=commands.DrawIndexed(3);return result?commands.EndRendering():result;
    },{}});return pass==UINT32_MAX?Result::Failure("Failed to add fullscreen pass"):Result::Success();
}
FeaturePipeline::FeaturePipeline(Rhi::Device& device):m_device(device),m_forward(device),m_modules(m_context),m_resolve(device) {
    for (auto& cache:m_caches) cache=std::make_unique<Renderer::TransientImageCache>(device);
}
FeaturePipeline::~FeaturePipeline() {
    m_device.WaitIdle();m_modules.Shutdown();
    for (auto& target:m_hdr) if (target.image) m_device.Destroy(target.image);
    for (auto image:m_retired) m_device.Destroy(image);
}
Result FeaturePipeline::Initialize(const std::filesystem::path& shaders,Rhi::Format format) {
    auto result=m_forward.Initialize(shaders/"hdr",Rhi::Format::Rgba16Float);if (!result) return result;
    result=m_resolve.Initialize(shaders/"extensions","resolve",format);if (!result) return result;
    return m_modules.Startup();
}
Result FeaturePipeline::PrepareFrame() {
    auto prepared=m_registry.PrepareFrame();if(!prepared)return prepared;
    for (auto image:m_retired) { auto result=m_device.Destroy(image);if (!result) return result; }m_retired.clear();
    for (auto& cache:m_caches) { auto result=cache->FlushRetired();if (!result) return result; }return m_forward.PrepareFrame();
}
Result FeaturePipeline::Render(const Renderer::RenderWorld& world) {
    auto result=PrepareFrame();if (!result) return result;Rhi::FrameInfo target;bool available=false;result=m_device.BeginFrame(target,available);if (!result||!available) return result;
    result=RenderToTarget(world,target,true);return result?m_device.EndFrame():result;
}
Result FeaturePipeline::RenderToTarget(const Renderer::RenderWorld& world,const Rhi::FrameInfo& target,bool present) {
    try {
        if (target.frameSlot>=Rhi::kFramesInFlight) return Result::Failure("Invalid feature frame slot");
        auto& hdr=m_hdr[target.frameSlot];
        if (!hdr.image||hdr.width!=target.width||hdr.height!=target.height) {
            Target replacement{{},target.width,target.height};auto result=m_device.CreateImage({"HDR geometry",target.width,target.height,Rhi::Format::Rgba16Float,Rhi::ImageUsage::ColorAttachment|Rhi::ImageUsage::Sampled|Rhi::ImageUsage::TransferSource},replacement.image);if (!result) return result;
            if (hdr.image) m_retired.push_back(hdr.image);hdr=replacement;
        }
        m_lastSlot=target.frameSlot;auto& snapshot=m_snapshot;
        if(world.modified||snapshot.source!=world.source||snapshot.revision!=world.revision){
            if(!world.modified&&snapshot.source==world.source&&snapshot.revision==world.baseRevision&&!world.fullUpdate&&snapshot.instances.size()==world.instances.size()){for(auto index:world.changedInstances)snapshot.instances[index]=world.instances[index];snapshot.changedInstances=world.changedInstances;for(auto index:world.changedLights)snapshot.lights[index]=world.lights[index];snapshot.changedLights=world.changedLights;snapshot.fullUpdate=false;snapshot.baseRevision=world.baseRevision;snapshot.instanceRevision=world.instanceRevision;snapshot.revision=world.revision;}else snapshot=world;
        }
        snapshot.camera=world.camera;snapshot.settings=world.settings;snapshot.environment=world.environment;snapshot.lighting={};snapshot.atmosphericTransmittance={};snapshot.atmosphere={};
        Renderer::RenderGraph graph;FeatureFrame frame;

        frame.width=target.width;frame.height=target.height;frame.slot=target.frameSlot;frame.world=&snapshot;frame.lit=m_debugView==0||m_debugView>=5;
        frame.selected=m_selected;
        frame.debugView=m_debugView;
        frame.geometry=&m_forward;
        if(frame.lit){auto r=m_forward.PrepareShadows(snapshot,target.frameSlot);if(!r)return r;}
        auto camera=glm::make_mat4(world.camera.transform.values.data());auto projection=Renderer::PerspectiveReverseZ(world.camera.verticalFov,static_cast<float>(target.width)/target.height,world.camera.nearPlane,world.camera.farPlane);
        frame.viewProjection=projection*glm::inverse(camera);frame.data.inverseViewProjection=glm::inverse(frame.viewProjection);frame.data.camera=glm::vec4(camera[3]);
        Renderer::RenderGraph before;auto result=m_registry.BuildBeforeGeometry(before,frame,snapshot);if(!result)return result;
        if(!before.Passes().empty()){Renderer::CompiledGraph pre;result=before.Compile(pre);if(result)result=before.Execute(pre,m_device.Commands(),*m_caches[target.frameSlot]);if(!result)return result;}
        result=m_forward.RenderToTarget(snapshot,{hdr.image,hdr.width,hdr.height,target.frameSlot});if(!result)return result;
        frame.color=graph.ImportImage("HDR geometry",hdr.image,m_device.StateOf(hdr.image));frame.output=graph.ImportImage("Output",target.color,m_device.StateOf(target.color));
        frame.depth=graph.ImportImage("Geometry depth",m_forward.DepthTarget(target.frameSlot),Rhi::ResourceState::SampledRead);
        result=m_registry.Build(graph,frame);if (!result) return result;
        std::array sources{frame.color};result=m_resolve.Add(graph,"HDR output resolve",sources,frame.output,frame,{});if (!result) return result;
        graph.AddPass({present?"Present":"Viewport ready",{{frame.output,present?Rhi::ResourceState::Present:Rhi::ResourceState::SampledRead}},{},{}});
        Renderer::CompiledGraph plan;result=graph.Compile(plan);if (!result) return result;result=graph.Execute(plan,m_device.Commands(),*m_caches[target.frameSlot]);if (!result) return result;
        m_cpuTimes.clear();for(std::size_t i=0;i<before.Passes().size();++i)m_cpuTimes.emplace_back(before.Passes()[i].name,before.CpuPassMilliseconds()[i]);auto times=m_forward.CpuPassTimes();m_cpuTimes.insert(m_cpuTimes.end(),times.begin(),times.end());for (std::size_t i=0;i<graph.Passes().size();++i) m_cpuTimes.emplace_back(graph.Passes()[i].name,graph.CpuPassMilliseconds()[i]);return Result::Success();
    }catch(const std::exception& error){return Result::Failure(error.what());}
}
}

