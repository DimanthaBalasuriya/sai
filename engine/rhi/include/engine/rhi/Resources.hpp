#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>
namespace Engine::Rhi {
// Value handles borrow device-owned resources. Generation checks reject stale handles.
// Descriptors own their CPU data; all resource operations are main-thread only.
template<class Tag> struct Handle {
    std::uint32_t slot = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t generation = 0;
    explicit operator bool() const noexcept { return generation != 0; }
    bool operator==(const Handle&) const = default;
};
using BufferHandle = Handle<struct BufferTag>;
using ImageHandle = Handle<struct ImageTag>;
using SamplerHandle = Handle<struct SamplerTag>;
using PipelineHandle = Handle<struct PipelineTag>;
using TextureTableHandle = Handle<struct TextureTableTag>;
enum class Format { Rgba8Srgb, Bgra8Srgb, Rgba8Unorm, Bgra8Unorm, Rgba16Float, D32Float, Swapchain };
enum class ResourceState { Undefined, TransferSource, TransferDestination, VertexRead, IndexRead, SampledRead, ColorWrite, DepthWrite, Present, HostWrite, StorageRead, StorageReadWrite };
constexpr bool IsWrite(ResourceState state) { return state == ResourceState::HostWrite || state == ResourceState::TransferDestination || state == ResourceState::ColorWrite || state == ResourceState::DepthWrite||state==ResourceState::StorageReadWrite; }
enum class BufferUsage : std::uint32_t { Vertex = 1, Index = 2, Storage = 4, TransferSource = 8, TransferDestination = 16, DeviceAddress = 32, Uniform = 64 };
enum class ImageUsage : std::uint32_t { Sampled = 1, ColorAttachment = 2, DepthAttachment = 4, TransferSource = 8, TransferDestination = 16, Storage=32 };
enum class ImageDimension { Texture2D,Texture3D,Cube };
constexpr BufferUsage operator|(BufferUsage a, BufferUsage b) { return static_cast<BufferUsage>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b)); }
constexpr ImageUsage operator|(ImageUsage a, ImageUsage b) { return static_cast<ImageUsage>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b)); }
template<class Flags> constexpr bool HasFlag(Flags flags, Flags flag) { return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(flag)) != 0; }
struct BufferDesc { std::string name; std::size_t bytes = 0; BufferUsage usage = BufferUsage::Vertex; bool hostVisible = false; };
struct ImageDesc {
    std::string name;
    std::uint32_t width = 0, height = 0;
    Format format = Format::Rgba8Srgb;
    ImageUsage usage = ImageUsage::Sampled;
    std::uint32_t depth=1;ImageDimension dimension=ImageDimension::Texture2D;
};
inline bool CompatibleImages(const ImageDesc& a, const ImageDesc& b) {
    return a.width == b.width && a.height == b.height && a.format == b.format && a.usage == b.usage&&a.depth==b.depth&&a.dimension==b.dimension;
}
struct SamplerDesc { std::string name; bool linear = true; bool repeat = true; };
enum class VertexFormat { Float2, Float3, Float4 };
struct VertexAttribute { std::uint32_t location = 0, offset = 0; VertexFormat format = VertexFormat::Float3; };
struct PipelineDesc {
    std::string name;
    std::filesystem::path vertexShader, fragmentShader;
    std::uint32_t vertexStride = 0, pushConstantBytes = 0;
    std::vector<VertexAttribute> attributes;
    Format colorFormat = Format::Swapchain;
    bool depthTest = false;
    bool wireframe = false;
    bool lines = false;
    bool reverseZ=false;
    bool depthOnly=false;
    float depthBias=0,slopeBias=0;
};
struct ComputePipelineDesc {std::string name;std::filesystem::path shader;std::uint32_t pushConstantBytes=0;};
struct ImageBarrier { ImageHandle image; ResourceState after; bool discard = false; };
struct BufferBarrier { BufferHandle buffer; ResourceState after; };
struct RenderingDesc {
    ImageHandle color, depth;
    std::array<float, 4> clearColor{0.015f, 0.02f, 0.035f, 0.0f};
    float clearDepth = 1.0f;
    bool loadColor = false;
    std::uint32_t x=0,y=0,width=0,height=0;
    bool loadDepth=false;
};
struct FrameInfo { ImageHandle color; std::uint32_t width = 0, height = 0, frameSlot = 0; };
struct PassTiming { std::string name; double gpuMilliseconds = 0.0; };
constexpr std::uint32_t kTextureTableCapacity = 256;
constexpr std::uint32_t kFramesInFlight = 2;
constexpr std::uint32_t kMaxTimedPasses = 256;
}
