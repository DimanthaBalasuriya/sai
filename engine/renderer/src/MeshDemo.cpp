#include "engine/renderer/MeshDemo.hpp"
#include "engine/core/Log.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <exception>
namespace Engine::Renderer {
namespace {
struct Vertex { float x,y,z,u,v; };
struct DrawData { glm::mat4 mvp; std::uint32_t textureIndex, samplerIndex; };
static_assert(sizeof(DrawData) == 72);
constexpr std::array<Vertex,24> kVertices{{
    {-1,-1,1,0,0},{1,-1,1,1,0},{1,1,1,1,1},{-1,1,1,0,1},
    {1,-1,-1,0,0},{-1,-1,-1,1,0},{-1,1,-1,1,1},{1,1,-1,0,1},
    {-1,-1,-1,0,0},{-1,-1,1,1,0},{-1,1,1,1,1},{-1,1,-1,0,1},
    {1,-1,1,0,0},{1,-1,-1,1,0},{1,1,-1,1,1},{1,1,1,0,1},
    {-1,1,1,0,0},{1,1,1,1,0},{1,1,-1,1,1},{-1,1,-1,0,1},
    {-1,-1,-1,0,0},{1,-1,-1,1,0},{1,-1,1,1,1},{-1,-1,1,0,1}
}};
constexpr std::array<std::uint32_t,36> kIndices{{
    0,1,2,2,3,0,4,5,6,6,7,4,8,9,10,10,11,8,
    12,13,14,14,15,12,16,17,18,18,19,16,20,21,22,22,23,20
}};
}
MeshDemo::MeshDemo(Rhi::Device& device) : m_device(device) {
    for (auto& cache : m_caches) cache = std::make_unique<TransientImageCache>(device);
}
MeshDemo::~MeshDemo() {
    const auto destroy = [&](auto handle) { if (handle) { auto result = m_device.Destroy(handle); if (!result) Log::Error(result.Error()); } };
    destroy(m_table); destroy(m_pipeline); destroy(m_vertices); destroy(m_indices);
    for (auto handle : m_textures) destroy(handle); destroy(m_sampler);
}
Result MeshDemo::Initialize(const std::filesystem::path& shaders) {
    try {
        if (m_vertices) return Result::Failure("Mesh demo already initialized");
        auto result = m_device.CreateBuffer({"Cube vertices",sizeof(kVertices), Rhi::BufferUsage::Vertex | Rhi::BufferUsage::TransferDestination | Rhi::BufferUsage::DeviceAddress,false},m_vertices);
        if (!result) return result;
        result = m_device.UploadBuffer(m_vertices,std::as_bytes(std::span{kVertices}),Rhi::ResourceState::VertexRead); if (!result) return result;
        result = m_device.CreateBuffer({"Cube indices",sizeof(kIndices),Rhi::BufferUsage::Index | Rhi::BufferUsage::TransferDestination,false},m_indices); if (!result) return result;
        result = m_device.UploadBuffer(m_indices,std::as_bytes(std::span{kIndices}),Rhi::ResourceState::IndexRead); if (!result) return result;
        if (m_device.BufferAddress(m_vertices) == 0) return Result::Failure("Vertex buffer device address unavailable");
        result = m_device.CreateSampler({"Checker sampler",false,true},m_sampler); if (!result) return result;
        result = m_device.CreateTextureTable("Demo indexed texture table",m_table); if (!result) return result;
        for (std::uint32_t index = 0; index < m_textures.size(); ++index) {
            result = m_device.CreateImage({"Checker texture " + std::to_string(index),64,64,Rhi::Format::Rgba8Srgb,Rhi::ImageUsage::Sampled | Rhi::ImageUsage::TransferDestination},m_textures[index]); if (!result) return result;
            std::array<std::uint8_t,64*64*4> pixels{};
            for (std::size_t y = 0; y < 64; ++y) for (std::size_t x = 0; x < 64; ++x) {
                const bool light = ((x/8 + y/8) % 2) == 0; auto offset = (y*64+x)*4;
                pixels[offset] = light ? 235 : (index == 0 ? 35 : 175);
                pixels[offset+1] = light ? 235 : (index == 0 ? 135 : 55);
                pixels[offset+2] = light ? 225 : (index == 0 ? 225 : 30);
                pixels[offset+3] = 255;
            }
            result = m_device.UploadImage(m_textures[index],std::as_bytes(std::span{pixels})); if (!result) return result;
            result = m_device.SetTexture(m_table,index,m_textures[index],m_sampler); if (!result) return result;
        }
        Rhi::PipelineDesc pipeline;
        pipeline.name = "Textured depth mesh"; pipeline.vertexShader = shaders / "mesh.vertex.spv"; pipeline.fragmentShader = shaders / "mesh.fragment.spv";
        pipeline.vertexStride = sizeof(Vertex); pipeline.pushConstantBytes = sizeof(DrawData); pipeline.depthTest = true;
        pipeline.attributes = {{0,0,Rhi::VertexFormat::Float3},{1,12,Rhi::VertexFormat::Float2}};
        return m_device.CreatePipeline(pipeline,m_pipeline);
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result MeshDemo::Render(double elapsedSeconds) {
    try {
        for (auto& cache : m_caches) { auto result = cache->FlushRetired(); if (!result) return result; }
        Rhi::FrameInfo frame; bool available = false;
        auto result = m_device.BeginFrame(frame,available); if (!result || !available) return result;
        RenderGraph graph;
        auto color = graph.ImportImage("Window color",frame.color,m_device.StateOf(frame.color));
        auto depth = graph.CreateImage({"Mesh depth",frame.width,frame.height,Rhi::Format::D32Float,Rhi::ImageUsage::DepthAttachment});
        auto vertices = graph.ImportBuffer("Cube vertices",m_vertices,m_device.StateOf(m_vertices));
        auto indices = graph.ImportBuffer("Cube indices",m_indices,m_device.StateOf(m_indices));
        auto texture0 = graph.ImportImage("Blue checker",m_textures[0],m_device.StateOf(m_textures[0]));
        auto texture1 = graph.ImportImage("Orange checker",m_textures[1],m_device.StateOf(m_textures[1]));
        graph.AddPass({"Textured meshes",{{color,Rhi::ResourceState::ColorWrite},{depth,Rhi::ResourceState::DepthWrite},
            {vertices,Rhi::ResourceState::VertexRead},{indices,Rhi::ResourceState::IndexRead},{texture0,Rhi::ResourceState::SampledRead},{texture1,Rhi::ResourceState::SampledRead}},
            [&](Rhi::CommandList& commands,const PassResources& resources) {
                auto recorded = commands.BeginRendering({resources.Image(color),resources.Image(depth)}); if (!recorded) return recorded;
                recorded = commands.BindPipeline(m_pipeline); if (!recorded) return recorded;
                recorded = commands.BindTextureTable(m_table); if (!recorded) return recorded;
                recorded = commands.BindMesh(resources.Buffer(vertices),resources.Buffer(indices)); if (!recorded) return recorded;
                auto projection = glm::perspective(glm::radians(48.0f),static_cast<float>(frame.width)/static_cast<float>(frame.height),0.1f,100.0f);
                projection[1][1] *= -1;
                auto view = glm::lookAt(glm::vec3(0,0,6),glm::vec3(0,0,0),glm::vec3(0,1,0));
                for (std::uint32_t i = 0; i < 2; ++i) {
                    auto model = glm::translate(glm::mat4(1),i == 0 ? glm::vec3(-0.5f,0,0) : glm::vec3(0.85f,0.35f,-1.6f));
                    model = glm::rotate(model,static_cast<float>(elapsedSeconds)*0.65f+0.45f,glm::normalize(glm::vec3(0.35f,1,0.15f)));
                    DrawData data{projection*view*model,i,i};
                    recorded = commands.PushConstants(std::as_bytes(std::span{&data,1})); if (!recorded) return recorded;
                    // Near mesh first, far mesh last: depth must preserve the overlap.
                    recorded = commands.DrawIndexed(static_cast<std::uint32_t>(kIndices.size())); if (!recorded) return recorded;
                }
                return commands.EndRendering();
            },{}});
        graph.AddPass({"Present",{{color,Rhi::ResourceState::Present}},{},{}});
        CompiledGraph plan; result = graph.Compile(plan); if (!result) return result;
        result = graph.Execute(plan,m_device.Commands(),*m_caches[frame.frameSlot]); if (!result) return result;
        return m_device.EndFrame();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
}
