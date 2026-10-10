#include "engine/extensions/Skybox.hpp"
#include "engine/renderer/Exposure.hpp"
namespace Engine::Extensions {
namespace {
class Skybox final:public IRenderFeature {
public:
    Skybox(Rhi::Device& device,std::filesystem::path shaders):m_pass(device),m_shaders(std::move(shaders)){}
    const char* Name() const noexcept override { return "Skybox"; }
    int Order() const noexcept override { return 10; }
    Result Initialize() override { return m_pass.Initialize(m_shaders,"skybox",Rhi::Format::Rgba16Float); }
    Result AddPasses(Renderer::RenderGraph& graph,FeatureFrame& frame) override {
        if (!frame.lit) return Result::Success();
        auto output=HdrImage(graph,"Sky HDR",frame.width,frame.height);auto data=frame.data;data.parameters.x=m_intensity*5000.f*Renderer::ManualExposure(frame.world->camera.ev100,frame.world->camera.exposureCompensation);
        std::array inputs{frame.color};auto result=m_pass.Add(graph,"Skybox",inputs,output,frame,data);if (result) frame.color=output;return result;
    }
    std::span<const FeatureParameter> Parameters() noexcept override { return m_parameters; }
private:
    FullscreenPass m_pass;std::filesystem::path m_shaders;float m_intensity=1;
    std::array<FeatureParameter,1> m_parameters{{{"Intensity",&m_intensity,0,4}}};
};
}
std::unique_ptr<IModule> MakeSkyboxModule(FeatureRegistry& registry,Rhi::Device& device,const std::filesystem::path& shaders) { return std::make_unique<FeatureModule>(registry,std::make_unique<Skybox>(device,shaders)); }
}
