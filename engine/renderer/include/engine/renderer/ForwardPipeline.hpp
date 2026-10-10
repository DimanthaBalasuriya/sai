#pragma once
#include "engine/renderer/RenderGraph.hpp"
#include "engine/renderer/RenderWorld.hpp"
#include "engine/renderer/RenderAssets.hpp"
#include <array>
#include <memory>
#include "engine/core/SpatialIndex.hpp"
#include <map>
namespace Engine::Renderer {
// Features own rendering policy; dependencies and GPU lifetime stay in the graph/RHI.
struct ForwardFrame { ResourceId color,depth; std::uint32_t width,height; GraphPass geometry; };
class IRenderFeature {
public:
    virtual ~IRenderFeature()=default;
    virtual Result AddPasses(RenderGraph& graph,const ForwardFrame& frame)=0;
};
// Owns GPU resources and feature list. Device outlives pipeline; main-thread only.
class ForwardPipeline {
public:
    explicit ForwardPipeline(Rhi::Device& device);
    ~ForwardPipeline();
    Result Initialize(const std::filesystem::path& shaders,Rhi::Format outputFormat=Rhi::Format::Swapchain);
    Result AddFeature(std::unique_ptr<IRenderFeature> feature);
    Result UploadAssets(const AssetBundle& assets);
    Result Render(const RenderWorld& world);
    Result PrepareFrame(); // Retire old targets before BeginFrame.
    Result PrepareShadows(const RenderWorld&,std::uint32_t slot);
    void ShadowCasters(const Mat4& viewProjection,std::vector<std::uint32_t>& output) const;
    Result AddShadowPass(RenderGraph&,const RenderWorld&,Rhi::ImageHandle,const Mat4&,std::span<const std::uint32_t>,std::uint32_t slot,std::uint32_t x,std::uint32_t y,std::uint32_t resolution,std::string name);
    Result RenderToTarget(const RenderWorld& world,const Rhi::FrameInfo& target,bool present=false);
    Rhi::ImageHandle DepthTarget(std::uint32_t slot) const { return m_depth[slot].image; }
    void SetDebugView(std::uint32_t view) noexcept { m_debugView=view; }
    std::size_t LastInstanceCount() const noexcept { return m_lastInstanceCount; }
    std::size_t LastDrawCallCount() const noexcept {return m_batches.size();}
    std::size_t LastVisibleInstanceCount() const noexcept {return m_visible.size();}
    std::span<const std::pair<std::string,double>> CpuPassTimes() const noexcept { return m_cpuPassTimes; }
private:
    static constexpr std::size_t kCapacity=131072;
    Rhi::Device& m_device;
    Rhi::BufferHandle m_vertices,m_indices;
    struct GpuMesh { Guid id; Rhi::BufferHandle vertices,indices; std::uint32_t indexCount;Spatial::Aabb bounds{{-1,-1,-1},{1,1,1}}; };
    struct GpuTexture { Guid id; Rhi::ImageHandle image; std::uint32_t index; };
    struct Batch { Guid mesh,material; std::uint32_t first,count; };
    std::vector<GpuMesh> m_meshes;
    std::vector<GpuTexture> m_importTextures;
    std::vector<MaterialAsset> m_materials;
    Rhi::BufferHandle m_materialBuffer;
    std::vector<Batch> m_batches;
    std::vector<std::byte> m_instanceBytes;
    Guid m_batchSource;
    std::uint64_t m_batchRevision=0;
    std::vector<std::uint32_t> m_mapping,m_visible,m_query;
    std::vector<std::pair<Guid,Guid>> m_keys;
    Spatial::SpatialIndex m_spatial;std::vector<Spatial::SpatialIndex::Proxy> m_proxies;
    struct GuidLess {bool operator()(Guid a,Guid b)const{return a.high<b.high||(a.high==b.high&&a.low<b.low);}};
    struct SpatialEntry {Spatial::SpatialIndex::Proxy proxy;std::uint64_t mark;};
    std::map<Guid,SpatialEntry,GuidLess> m_spatialEntries;
    std::uint64_t m_spatialGeneration=0;
    Mat4 m_cullMatrix;bool m_cullValid=false;
    std::array<std::vector<std::uint32_t>,Rhi::kFramesInFlight> m_pending;
    std::array<std::vector<std::uint8_t>,Rhi::kFramesInFlight> m_pendingFlags;
    std::uint32_t m_debugView=0;
    std::array<Rhi::ImageHandle,2> m_textures{};
    Rhi::ImageHandle m_neutralEnvironment;
    Rhi::SamplerHandle m_sampler,m_shadowSampler,m_iblSampler;
    Rhi::PipelineHandle m_pipeline;
    Rhi::PipelineHandle m_wirePipeline;
    Rhi::PipelineHandle m_shadowPipeline;
    std::array<Rhi::BufferHandle,Rhi::kFramesInFlight> m_shadowModels{};
    std::array<Rhi::TextureTableHandle,Rhi::kFramesInFlight> m_shadowTables{};
    Spatial::SpatialIndex m_shadowSpatial;std::vector<Spatial::SpatialIndex::Proxy> m_shadowProxies;
    Guid m_shadowSource;std::uint64_t m_shadowRevision=UINT64_MAX;
    std::array<Guid,Rhi::kFramesInFlight> m_shadowUploadedSource{};
    std::array<std::uint64_t,Rhi::kFramesInFlight> m_shadowUploadedRevision{};
    std::vector<Mat4> m_shadowCpuModels;
    std::array<Rhi::TextureTableHandle,Rhi::kFramesInFlight> m_tables{};
    struct Depth {Rhi::ImageHandle image;std::uint32_t width=0,height=0;};
    std::array<Depth,Rhi::kFramesInFlight> m_depth{};std::vector<Rhi::ImageHandle> m_retired;
    std::array<std::unique_ptr<TransientImageCache>,Rhi::kFramesInFlight> m_caches;
    std::array<Rhi::BufferHandle,Rhi::kFramesInFlight> m_instanceBuffers{},m_sceneBuffers{};
    std::array<std::uint64_t,Rhi::kFramesInFlight> m_revisions{};
    std::array<Guid,Rhi::kFramesInFlight> m_sources{};
    std::array<bool,Rhi::kFramesInFlight> m_uploaded{};
    std::vector<std::unique_ptr<IRenderFeature>> m_features;
    std::size_t m_lastInstanceCount=0;
    std::vector<std::pair<std::string,double>> m_cpuPassTimes;
};
}
