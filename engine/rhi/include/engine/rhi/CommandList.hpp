#pragma once
#include <span>
#include <string_view>
#include "engine/core/Result.hpp"
#include "engine/rhi/Resources.hpp"
namespace Engine::Rhi {
// Borrowed encoder, valid between Device::BeginFrame and EndFrame. Main-thread only.
// Resources are device-owned; recording commands does not transfer ownership.
class CommandList {
public:
    virtual ~CommandList() = default;
    virtual Result BeginPass(std::string_view name) = 0;
    virtual Result EndPass() = 0;
    virtual Result Barriers(std::span<const ImageBarrier> images, std::span<const BufferBarrier> buffers) = 0;
    virtual Result BeginRendering(const RenderingDesc& desc) = 0;
    virtual Result EndRendering() = 0;
    virtual Result BindPipeline(PipelineHandle pipeline) = 0;
    virtual Result BindTextureTable(TextureTableHandle table) = 0;
    virtual Result BindMesh(BufferHandle vertices, BufferHandle indices) = 0;
    virtual Result PushConstants(std::span<const std::byte> bytes) = 0;
    virtual Result DrawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex = 0, std::uint32_t instanceCount = 1, std::uint32_t firstInstance = 0) = 0;
    virtual Result Dispatch(std::uint32_t,std::uint32_t,std::uint32_t){return Result::Failure("Compute dispatch is unsupported by this encoder");}
};
}
