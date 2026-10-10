#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include "engine/core/Result.hpp"
#include "engine/rhi/AdapterSelection.hpp"
#include "engine/rhi/CommandList.hpp"
namespace Engine { class Window; }
namespace Engine::Rhi {
struct DeviceStatistics {
    std::uint64_t presentedFrames = 0;
    std::uint32_t swapchainGenerations = 0;
    std::uint32_t validationMessages = 0;
    double gpuFrameMilliseconds = 0.0;
    std::uint64_t memoryUsedBytes=0,memoryBudgetBytes=0;
    bool driverMemoryBudget=false;
};
// Owns GPU state; main-thread only. The borrowed window must outlive the device.
// Resources belong to this device and are released automatically at shutdown.
// Explicit Destroy waits for GPU idle; deferred destruction is a later extension.
class Device {
public:
    Device();
    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Result Initialize(Window& window,std::string_view gpuOverride={});
    const AdapterInfo& Adapter() const noexcept;
    Result CreateBuffer(const BufferDesc& desc, BufferHandle& output);
    Result CreateImage(const ImageDesc& desc, ImageHandle& output);
    Result CreateSampler(const SamplerDesc& desc, SamplerHandle& output);
    Result CreatePipeline(const PipelineDesc& desc, PipelineHandle& output);
    Result CreateComputePipeline(const ComputePipelineDesc&,PipelineHandle&);
    Result CreateTextureTable(std::string_view name, TextureTableHandle& output);
    Result SetTexture(TextureTableHandle table, std::uint32_t index, ImageHandle image, SamplerHandle sampler);
    // Like WriteBuffer: caller uses a table owned by the current fenced frame slot,
    // and updates it before its first binding in this frame.
    Result WriteTexture(TextureTableHandle table,std::uint32_t index,ImageHandle image,SamplerHandle sampler);
    Result WriteStorageImage(TextureTableHandle,std::uint32_t index,ImageHandle);
    Result UploadBuffer(BufferHandle buffer, std::span<const std::byte> bytes, ResourceState finalState);
    Result UploadImage(ImageHandle image, std::span<const std::byte> rgbaPixels);
    // Synchronous diagnostic capture, outside active frames only. Never used by
    // interactive rendering, exposure adaptation or lighting culling.
    Result ReadImage(ImageHandle image,std::vector<std::byte>& pixels);
    // Caller owns frame-slot scheduling: write only after that slot's BeginFrame fence wait.
    Result WriteBuffer(BufferHandle buffer, std::span<const std::byte> bytes,std::uint64_t offset=0);
    // Read host-visible data after the owning frame slot fence has completed.
    Result ReadBuffer(BufferHandle buffer,std::span<std::byte> bytes,std::uint64_t offset=0);
    Result Destroy(BufferHandle handle);
    Result Destroy(ImageHandle handle);
    Result Destroy(SamplerHandle handle);
    Result Destroy(PipelineHandle handle);
    Result Destroy(TextureTableHandle handle);
    ResourceState StateOf(BufferHandle handle) const noexcept;
    ResourceState StateOf(ImageHandle handle) const noexcept;
    std::uint64_t BufferAddress(BufferHandle handle) const noexcept;
    Result BeginFrame(FrameInfo& frame, bool& available);
    CommandList& Commands() noexcept;
    Result EndFrame();
    Result WaitIdle();
    // Optional UI bridge. UI context must exist until GuiShutdown; Vulkan stays private.
    Result GuiInitialize();
    Result GuiNewFrame();
    bool GuiUsesSrgb() const noexcept;
    Result GuiRebuildFonts(); // Outside a frame; retires the previous atlas after GPU idle.
    Result GuiTexture(ImageHandle image,std::uint64_t& output);
    Result GuiRemoveTexture(std::uint64_t texture);
    Result GuiDraw(void* drawData,ImageHandle color);
    void GuiShutdown() noexcept;
    std::span<const PassTiming> PassTimings() const noexcept;
    Result Shutdown() noexcept;
    DeviceStatistics Statistics() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
