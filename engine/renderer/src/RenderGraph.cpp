#include "engine/renderer/RenderGraph.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/Profile.hpp"
#include <algorithm>
#include <chrono>
#include <exception>
namespace Engine::Renderer {
ResourceId RenderGraph::ReuseImage(std::string name,Rhi::ImageHandle handle,Rhi::ResourceState initial) noexcept {
    for(std::uint32_t i=0;i<m_resources.size();++i)if(m_resources[i].image&&!m_resources[i].transient&&m_resources[i].imageHandle==handle)return {i};
    return ImportImage(std::move(name),handle,initial);
}
ResourceId RenderGraph::ReuseBuffer(std::string name,Rhi::BufferHandle handle,Rhi::ResourceState initial) noexcept {
    for(std::uint32_t i=0;i<m_resources.size();++i)if(!m_resources[i].image&&m_resources[i].bufferHandle==handle)return {i};
    return ImportBuffer(std::move(name),handle,initial);
}
ResourceId RenderGraph::ImportImage(std::string name, Rhi::ImageHandle handle, Rhi::ResourceState initial) noexcept {
    try {
    const auto id = ResourceId{static_cast<std::uint32_t>(m_resources.size())};
    m_resources.push_back({std::move(name), true, false, {}, handle, {}, initial}); ++m_revision; return id;
    } catch (...) { m_declarationFailed = true; return {}; }
}
ResourceId RenderGraph::ImportBuffer(std::string name, Rhi::BufferHandle handle, Rhi::ResourceState initial) noexcept {
    try {
    const auto id = ResourceId{static_cast<std::uint32_t>(m_resources.size())};
    m_resources.push_back({std::move(name), false, false, {}, {}, handle, initial}); ++m_revision; return id;
    } catch (...) { m_declarationFailed = true; return {}; }
}
ResourceId RenderGraph::CreateImage(Rhi::ImageDesc desc) noexcept {
    try {
    const auto id = ResourceId{static_cast<std::uint32_t>(m_resources.size())};
    m_resources.push_back({desc.name, true, true, std::move(desc), {}, {}, Rhi::ResourceState::Undefined}); ++m_revision; return id;
    } catch (...) { m_declarationFailed = true; return {}; }
}
std::uint32_t RenderGraph::AddPass(GraphPass pass) noexcept {
    try { auto index = static_cast<std::uint32_t>(m_passes.size()); m_passes.push_back(std::move(pass)); ++m_revision; return index; }
    catch (...) { m_declarationFailed = true; return UINT32_MAX; }
}
Result RenderGraph::Compile(CompiledGraph& output) const {
    try {
        ENGINE_PROFILE_SCOPE("Compile render graph");
        if (m_declarationFailed) return Result::Failure("Render graph declaration allocation failed");
        const auto count = m_passes.size();
        if (count > Rhi::kMaxTimedPasses) return Result::Failure("Render graph exceeds timed pass capacity");
        std::vector<std::vector<bool>> edges(count, std::vector<bool>(count));
        for (std::size_t p = 0; p < count; ++p) {
            const auto& pass = m_passes[p];
            if (pass.name.empty()) return Result::Failure("Every pass needs a name");
            for (auto dependency : pass.dependencies) {
                if (dependency >= count || dependency == p) return Result::Failure("Invalid pass dependency");
                edges[dependency][p] = true;
            }
            std::vector<bool> seen(m_resources.size());
            for (const auto& use : pass.uses) {
                if (use.resource.value >= m_resources.size() || seen[use.resource.value] || use.state == Rhi::ResourceState::Undefined || use.state == Rhi::ResourceState::HostWrite)
                    return Result::Failure("Invalid or duplicate pass resource use");
                seen[use.resource.value] = true;
                const auto& resource = m_resources[use.resource.value];
                if (resource.image ? (use.state == Rhi::ResourceState::VertexRead || use.state == Rhi::ResourceState::IndexRead || use.state == Rhi::ResourceState::StorageRead || use.state == Rhi::ResourceState::HostWrite)
                    : (use.state == Rhi::ResourceState::ColorWrite || use.state == Rhi::ResourceState::DepthWrite || use.state == Rhi::ResourceState::SampledRead || use.state == Rhi::ResourceState::Present))
                    return Result::Failure("Resource state does not match resource kind");
            }
        }
        // Declaration order defines versions: read-after-write, write-after-read, and write-after-write.
        for (std::size_t r = 0; r < m_resources.size(); ++r) {
            const auto& resource = m_resources[r];
            if (resource.name.empty() || (!resource.transient && (resource.image ? !resource.imageHandle : !resource.bufferHandle)))
                return Result::Failure("Invalid imported resource");
            if (!resource.transient) for (std::size_t previous = 0; previous < r; ++previous) {
                const auto& other = m_resources[previous];
                if (!other.transient && other.image == resource.image && (resource.image ? other.imageHandle == resource.imageHandle : other.bufferHandle == resource.bufferHandle))
                    return Result::Failure("Import each physical resource only once");
            }
            if (resource.transient && (!resource.desc.width || !resource.desc.height || resource.desc.format == Rhi::Format::Swapchain))
                return Result::Failure("Invalid transient image");
            bool initialized = resource.initial != Rhi::ResourceState::Undefined;
            std::uint32_t writer = UINT32_MAX; std::vector<std::uint32_t> readers;
            for (std::uint32_t p = 0; p < count; ++p) for (const auto& use : m_passes[p].uses) {
                if (use.resource.value != r) continue;
                if (Rhi::IsWrite(use.state)) {
                    if (writer != UINT32_MAX) edges[writer][p] = true;
                    for (auto reader : readers) edges[reader][p] = true;
                    readers.clear(); writer = p; initialized = true;
                } else {
                    if (!initialized) return Result::Failure("Read before resource initialization: " + resource.name);
                    if (writer != UINT32_MAX) edges[writer][p] = true;
                    readers.push_back(p);
                }
            }
        }
        std::vector<std::uint32_t> indegrees(count);
        for (std::size_t a = 0; a < count; ++a) for (std::size_t b = 0; b < count; ++b) if (edges[a][b]) ++indegrees[b];
        std::vector<bool> emitted(count); CompiledGraph plan; plan.owner = this; plan.revision = m_revision;
        std::vector<Rhi::ResourceState> states; for (const auto& resource : m_resources) states.push_back(resource.initial);
        std::vector<std::uint32_t> first(m_resources.size(), UINT32_MAX), last(m_resources.size());
        for (std::uint32_t position = 0; position < count; ++position) {
            std::uint32_t p = 0; while (p < count && (emitted[p] || indegrees[p])) ++p;
            if (p == count) return Result::Failure("Render graph dependency cycle");
            emitted[p] = true; CompiledPass compiled{p,{}};
            for (const auto& use : m_passes[p].uses) {
                auto r = use.resource.value; bool isFirst = first[r] == UINT32_MAX;
                if (isFirst) first[r] = position;
                last[r] = position;
                if (states[r] != use.state || Rhi::IsWrite(states[r]) || (isFirst && m_resources[r].transient))
                    compiled.transitions.push_back({use.resource, states[r], use.state, isFirst && m_resources[r].transient});
                states[r] = use.state;
            }
            plan.passes.push_back(std::move(compiled));
            for (std::size_t b = 0; b < count; ++b) if (edges[p][b]) --indegrees[b];
        }
        plan.allocationSlots.assign(m_resources.size(), UINT32_MAX);
        struct Allocation { Rhi::ImageDesc desc; std::uint32_t last; };
        std::vector<Allocation> allocations;
        // Walk lifetime starts, allowing compatible, non-overlapping resources to share an image.
        for (std::uint32_t position = 0; position < count; ++position) for (std::size_t r = 0; r < m_resources.size(); ++r) {
            if (!m_resources[r].transient || first[r] != position) continue;
            std::uint32_t slot = 0;
            while (slot < allocations.size() && (allocations[slot].last >= position || !Rhi::CompatibleImages(allocations[slot].desc,m_resources[r].desc))) ++slot;
            if (slot == allocations.size()) allocations.push_back({m_resources[r].desc,last[r]});
            else allocations[slot].last = last[r];
            plan.allocationSlots[r] = slot;
        }
        plan.allocationCount = static_cast<std::uint32_t>(allocations.size()); output = std::move(plan); return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Rhi::ImageHandle PassResources::Image(ResourceId id) const noexcept {
    if (id.value >= m_resources.size() || !m_resources[id.value].image) return {};
    for (const auto& use : m_uses) if (use.resource == id) return m_images[id.value];
    return {};
}
Rhi::BufferHandle PassResources::Buffer(ResourceId id) const noexcept {
    if (id.value >= m_resources.size() || m_resources[id.value].image) return {};
    for (const auto& use : m_uses) if (use.resource == id) return m_resources[id.value].bufferHandle;
    return {};
}
Result RenderGraph::Execute(const CompiledGraph& plan, Rhi::CommandList& commands, TransientImageCache& cache) {
    try {
        ENGINE_PROFILE_SCOPE("Execute render graph");
        if (plan.owner != this || plan.revision != m_revision || plan.allocationSlots.size() != m_resources.size() || plan.passes.size() != m_passes.size())
            return Result::Failure("Compiled graph does not match declarations");
        std::vector<Rhi::ImageHandle> images(m_resources.size());
        for (std::size_t r = 0; r < m_resources.size(); ++r) {
            if (!m_resources[r].image) continue;
            images[r] = m_resources[r].imageHandle;
            if (m_resources[r].transient && plan.allocationSlots[r] != UINT32_MAX) {
                auto result = cache.Acquire(plan.allocationSlots[r], m_resources[r].desc, images[r]); if (!result) return result;
            }
        }
        m_cpuTimes.clear();
        for (const auto& compiled : plan.passes) {
            if (compiled.index >= m_passes.size()) return Result::Failure("Invalid compiled pass index");
            const auto& pass = m_passes[compiled.index]; auto started = std::chrono::steady_clock::now();
            auto result = commands.BeginPass(pass.name); if (!result) return result;
            std::vector<Rhi::ImageBarrier> imageBarriers; std::vector<Rhi::BufferBarrier> bufferBarriers;
            for (const auto& transition : compiled.transitions) {
                auto r = transition.resource.value;
                if (m_resources[r].image) imageBarriers.push_back({images[r],transition.after,transition.discard});
                else bufferBarriers.push_back({m_resources[r].bufferHandle,transition.after});
            }
            result = commands.Barriers(imageBarriers,bufferBarriers); if (!result) return result;
            PassResources resources; resources.m_resources = m_resources; resources.m_uses = pass.uses; resources.m_images = images;
            if (pass.record) { result = pass.record(commands, resources); if (!result) return result; }
            result = commands.EndPass(); if (!result) return result;
            m_cpuTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
        }
        return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
TransientImageCache::~TransientImageCache() {
    for (const auto& entry : m_entries) if (entry.handle) { auto result = m_device.Destroy(entry.handle); if (!result) Log::Error(result.Error()); }
    auto result = FlushRetired(); if (!result) Log::Error(result.Error());
}
Result TransientImageCache::FlushRetired() {
    for (auto handle : m_retired) { auto result = m_device.Destroy(handle); if (!result) return result; }
    m_retired.clear(); return Result::Success();
}
Result TransientImageCache::Acquire(std::uint32_t slot, const Rhi::ImageDesc& desc, Rhi::ImageHandle& output) {
    try {
        if (m_entries.size() <= slot) m_entries.resize(static_cast<std::size_t>(slot)+1);
        auto& entry = m_entries[slot];
        if (!entry.handle || !Rhi::CompatibleImages(entry.desc,desc)) {
            Rhi::ImageHandle replacement; auto result = m_device.CreateImage(desc,replacement); if (!result) return result;
            if (entry.handle) m_retired.push_back(entry.handle);
            entry = {desc,replacement};
        }
        output = entry.handle; return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
}
