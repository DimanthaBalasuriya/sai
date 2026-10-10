#pragma once
#include "engine/rhi/Device.hpp"
#include <functional>
#include <limits>
#include <vector>
namespace Engine::Renderer {
class RenderGraph;
// Graphs own declarations, not imported GPU resources. Main-thread only.
struct ResourceId { std::uint32_t value = UINT32_MAX; bool operator==(const ResourceId&) const = default; };
struct ResourceUse { ResourceId resource; Rhi::ResourceState state; }; // HostWrite is an imported state, never a graph operation.
struct GraphResource {
    std::string name;
    bool image = true, transient = false;
    Rhi::ImageDesc desc;
    Rhi::ImageHandle imageHandle;
    Rhi::BufferHandle bufferHandle;
    Rhi::ResourceState initial = Rhi::ResourceState::Undefined;
};
struct GraphTransition { ResourceId resource; Rhi::ResourceState before, after; bool discard = false; };
struct CompiledPass { std::uint32_t index; std::vector<GraphTransition> transitions; };
struct CompiledGraph {
    std::vector<CompiledPass> passes;
    std::vector<std::uint32_t> allocationSlots;
    std::uint32_t allocationCount = 0;
    const RenderGraph* owner = nullptr;
    std::uint64_t revision = 0;
};
// Borrowed handles are restricted to resources declared by the current pass.
class PassResources {
public:
    Rhi::ImageHandle Image(ResourceId id) const noexcept;
    Rhi::BufferHandle Buffer(ResourceId id) const noexcept;
private:
    friend class RenderGraph;
    std::span<const GraphResource> m_resources;
    std::span<const ResourceUse> m_uses;
    std::span<const Rhi::ImageHandle> m_images;
};
using PassCallback = std::function<Result(Rhi::CommandList&, const PassResources&)>;
struct GraphPass {
    std::string name;
    std::vector<ResourceUse> uses;
    PassCallback record;
    std::vector<std::uint32_t> dependencies;
};
// Owns cached images for ONE frame-in-flight slot; device must outlive it.
class TransientImageCache {
public:
    explicit TransientImageCache(Rhi::Device& device) : m_device(device) {}
    ~TransientImageCache();
    TransientImageCache(const TransientImageCache&) = delete;
    TransientImageCache& operator=(const TransientImageCache&) = delete;
    Result Acquire(std::uint32_t slot, const Rhi::ImageDesc& desc, Rhi::ImageHandle& output);
    Result FlushRetired(); // Call outside an active frame.
private:
    struct Entry { Rhi::ImageDesc desc; Rhi::ImageHandle handle; };
    Rhi::Device& m_device;
    std::vector<Entry> m_entries;
    std::vector<Rhi::ImageHandle> m_retired;
};
class RenderGraph {
public:
    ResourceId ImportImage(std::string name, Rhi::ImageHandle handle, Rhi::ResourceState initial) noexcept;
    ResourceId ImportBuffer(std::string name, Rhi::BufferHandle handle, Rhi::ResourceState initial) noexcept;
    ResourceId ReuseImage(std::string name, Rhi::ImageHandle handle, Rhi::ResourceState initial) noexcept;
    ResourceId ReuseBuffer(std::string name, Rhi::BufferHandle handle, Rhi::ResourceState initial) noexcept;
    ResourceId CreateImage(Rhi::ImageDesc desc) noexcept;
    std::uint32_t AddPass(GraphPass pass) noexcept;
    Result Compile(CompiledGraph& output) const;
    Result Execute(const CompiledGraph& plan, Rhi::CommandList& commands, TransientImageCache& cache);
    std::span<const GraphResource> Resources() const noexcept { return m_resources; }
    std::span<const GraphPass> Passes() const noexcept { return m_passes; }
    std::span<const double> CpuPassMilliseconds() const noexcept { return m_cpuTimes; }
private:
    std::vector<GraphResource> m_resources;
    std::vector<GraphPass> m_passes;
    std::vector<double> m_cpuTimes;
    std::uint64_t m_revision = 0;
    bool m_declarationFailed = false;
};
}
