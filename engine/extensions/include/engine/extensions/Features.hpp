#pragma once
#include "engine/renderer/ForwardPipeline.hpp"
#include "engine/core/ModuleRegistry.hpp"
#include "engine/core/EngineContext.hpp"
#include <glm/glm.hpp>
namespace Engine::Extensions {
// Main-thread services. Registries borrow module-owned features; graph callbacks
// borrow the frame snapshot only until Execute returns.
struct PassData { glm::mat4 inverseViewProjection{1};glm::vec4 parameters{};glm::uvec4 flags{};glm::vec4 camera{}; };
struct FeatureFrame {
    Renderer::ResourceId color,output,depth;
    std::uint32_t width=0,height=0,slot=0;
    const Renderer::RenderWorld* world=nullptr;
    glm::mat4 viewProjection{1};PassData data;
    bool lit=true;
    Guid selected;
    std::uint32_t debugView=0;
    Renderer::ForwardPipeline* geometry=nullptr;
};
struct FeatureParameter { const char* name;float* value;float minimum,maximum; };
class IRenderFeature {
public:
    virtual ~IRenderFeature()=default;
    virtual const char* Name() const noexcept=0;
    virtual int Order() const noexcept=0;
    virtual int PostOrder() const noexcept{return Order();}
    virtual Result Initialize()=0;
    virtual Result PrepareFrame(){return Result::Success();}
    virtual Result BeforeGeometry(Renderer::RenderGraph&,FeatureFrame&,Renderer::RenderWorld&){return Result::Success();}
    virtual Result AddPasses(Renderer::RenderGraph&,FeatureFrame&)=0;
    virtual std::span<const FeatureParameter> Parameters() noexcept { return {}; }
};
class FeatureRegistry {
public:
    struct Entry { IRenderFeature* feature;bool enabled=true; };
    Result Register(IRenderFeature& feature);
    void Remove(IRenderFeature& feature) noexcept;
    std::span<Entry> Entries() noexcept { return m_entries; }
    bool SetEnabled(std::string_view name,bool enabled) noexcept;
    Result Build(Renderer::RenderGraph& graph,FeatureFrame& frame);
    Result PrepareFrame();
    Result BuildBeforeGeometry(Renderer::RenderGraph&,FeatureFrame&,Renderer::RenderWorld&);
private:
    std::vector<Entry> m_entries;
    std::vector<Entry> m_postEntries;
};
class FeatureModule final : public IModule {
public:
    FeatureModule(FeatureRegistry& registry,std::unique_ptr<IRenderFeature> feature):m_registry(registry),m_feature(std::move(feature)){}
    const char* Name() const noexcept override { return m_feature->Name(); }
    Result OnStartup(EngineContext&) noexcept override;
    void OnShutdown(EngineContext&) noexcept override;
private:
    FeatureRegistry& m_registry;std::unique_ptr<IRenderFeature> m_feature;
};
// Owns a fullscreen pipeline and one descriptor table per fenced frame slot.
// Device outlives it. Images remain graph-owned; no Vulkan types cross this API.
class FullscreenPass {
public:
    explicit FullscreenPass(Rhi::Device& device):m_device(device){}
    ~FullscreenPass();
    Result Initialize(const std::filesystem::path& shaders,std::string_view shader,Rhi::Format format);
    Result Add(Renderer::RenderGraph& graph,std::string name,std::span<const Renderer::ResourceId> sources,Renderer::ResourceId destination,const FeatureFrame& frame,PassData data,std::span<const Renderer::ResourceUse> extraUses={});
private:
    Rhi::Device& m_device;Rhi::PipelineHandle m_pipeline;Rhi::BufferHandle m_vertices,m_indices;
    Rhi::SamplerHandle m_sampler;std::array<Rhi::TextureTableHandle,Rhi::kFramesInFlight> m_tables{};
};
Renderer::ResourceId HdrImage(Renderer::RenderGraph& graph,std::string name,std::uint32_t width,std::uint32_t height);
// Composes the unmodified forward renderer with removable feature modules.
class FeaturePipeline {
public:
    explicit FeaturePipeline(Rhi::Device& device);
    ~FeaturePipeline();
    const Renderer::RenderLighting& LightingStatistics() const noexcept {return m_snapshot.lighting;}
    Rhi::ImageHandle LastGeometryImage()const noexcept{return m_hdr[m_lastSlot].image;}
    FeatureRegistry& Features() noexcept { return m_registry; }
    Result AddModule(std::unique_ptr<IModule> module) { return m_modules.Add(std::move(module)); }
    Result Initialize(const std::filesystem::path& shaders,Rhi::Format outputFormat=Rhi::Format::Swapchain);
    Result UploadAssets(const Renderer::AssetBundle& assets) { return m_forward.UploadAssets(assets); }
    Result PrepareFrame();
    Result Render(const Renderer::RenderWorld& world);
    Result RenderToTarget(const Renderer::RenderWorld& world,const Rhi::FrameInfo& target,bool present=false);
    void SetDebugView(std::uint32_t view) noexcept { m_debugView=view;m_forward.SetDebugView(view); }
    void SetSelection(Guid selected) noexcept { m_selected=selected; }
    std::size_t LastInstanceCount() const noexcept { return m_forward.LastInstanceCount(); }
    std::size_t LastDrawCallCount() const noexcept {return m_forward.LastDrawCallCount();}
    std::size_t LastVisibleInstanceCount() const noexcept {return m_forward.LastVisibleInstanceCount();}
    std::span<const std::pair<std::string,double>> CpuPassTimes() const noexcept { return m_cpuTimes; }
private:
    Rhi::Device& m_device;Renderer::ForwardPipeline m_forward;
    EngineContext m_context;FeatureRegistry m_registry;ModuleRegistry m_modules;
    FullscreenPass m_resolve;
    std::array<std::unique_ptr<Renderer::TransientImageCache>,Rhi::kFramesInFlight> m_caches;
    struct Target { Rhi::ImageHandle image;std::uint32_t width=0,height=0; };
    std::array<Target,Rhi::kFramesInFlight> m_hdr{};std::vector<Rhi::ImageHandle> m_retired;
    std::vector<std::pair<std::string,double>> m_cpuTimes;std::uint32_t m_debugView=0;
    Guid m_selected;
    std::uint32_t m_lastSlot=0;
    Renderer::RenderWorld m_snapshot;
};
}
