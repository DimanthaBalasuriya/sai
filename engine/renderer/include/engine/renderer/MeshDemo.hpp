#pragma once
#include "engine/renderer/RenderGraph.hpp"
#include <array>
#include <memory>
namespace Engine::Renderer {
// M2 test client: owns its resources; the borrowed device must outlive it.
// Main-thread only. Scene extraction and render pipelines begin in M3.
class MeshDemo {
public:
    explicit MeshDemo(Rhi::Device& device);
    ~MeshDemo();
    MeshDemo(const MeshDemo&) = delete;
    MeshDemo& operator=(const MeshDemo&) = delete;
    Result Initialize(const std::filesystem::path& shaders);
    Result Render(double elapsedSeconds);
private:
    Rhi::Device& m_device;
    Rhi::BufferHandle m_vertices, m_indices;
    std::array<Rhi::ImageHandle,2> m_textures{};
    Rhi::SamplerHandle m_sampler;
    Rhi::PipelineHandle m_pipeline;
    Rhi::TextureTableHandle m_table;
    std::array<std::unique_ptr<TransientImageCache>,Rhi::kFramesInFlight> m_caches;
};
}
