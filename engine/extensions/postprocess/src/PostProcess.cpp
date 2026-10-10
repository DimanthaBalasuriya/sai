#include "engine/extensions/PostProcess.hpp"
namespace Engine::Extensions {
namespace {
class Bloom final:public IRenderFeature {
public:
    Bloom(Rhi::Device& device,std::filesystem::path shaders):m_extract(device),m_horizontal(device),m_vertical(device),m_composite(device),m_shaders(std::move(shaders)){}
    const char* Name() const noexcept override { return "Bloom"; }int Order() const noexcept override { return 30; }
    Result Initialize() override {
        for (auto* pass:{&m_extract,&m_horizontal,&m_vertical,&m_composite}) { auto result=pass->Initialize(m_shaders,"bloom",Rhi::Format::Rgba16Float);if (!result) return result; }return Result::Success();
    }
    Result AddPasses(Renderer::RenderGraph& graph,FeatureFrame& frame) override {
        if (!frame.lit) return Result::Success();
        auto width=std::max(1u,frame.width/2),height=std::max(1u,frame.height/2);
        auto bright=HdrImage(graph,"Bloom bright",width,height),horizontal=HdrImage(graph,"Bloom horizontal",width,height),vertical=HdrImage(graph,"Bloom vertical",width,height),output=HdrImage(graph,"Bloom HDR",frame.width,frame.height);
        PassData data;data.parameters={m_threshold,m_intensity,1.0f/width,1.0f/height};data.flags.x=0;
        std::array inputs{frame.color};auto result=m_extract.Add(graph,"Bloom extract",inputs,bright,frame,data);if (!result) return result;
        data.flags.x=1;inputs[0]=bright;result=m_horizontal.Add(graph,"Bloom horizontal",inputs,horizontal,frame,data);if (!result) return result;
        data.flags.x=2;inputs[0]=horizontal;result=m_vertical.Add(graph,"Bloom vertical",inputs,vertical,frame,data);if (!result) return result;
        data.flags.x=3;std::array combine{frame.color,vertical};result=m_composite.Add(graph,"Bloom composite",combine,output,frame,data);if (result) frame.color=output;return result;
    }
    std::span<const FeatureParameter> Parameters() noexcept override { return m_parameters; }
private:
    FullscreenPass m_extract,m_horizontal,m_vertical,m_composite;std::filesystem::path m_shaders;
    float m_threshold=1,m_intensity=0.25f;std::array<FeatureParameter,2> m_parameters{{{"Threshold",&m_threshold,0,5},{"Intensity",&m_intensity,0,2}}};
};
class Tonemap final:public IRenderFeature {
public:
    Tonemap(Rhi::Device& device,std::filesystem::path shaders):m_pass(device),m_shaders(std::move(shaders)){}
    const char* Name() const noexcept override { return "Tonemap"; }int Order() const noexcept override { return 40; }
    Result Initialize() override { return m_pass.Initialize(m_shaders,"tonemap",Rhi::Format::Rgba16Float); }
    Result AddPasses(Renderer::RenderGraph& graph,FeatureFrame& frame) override {
        if (!frame.lit) return Result::Success();auto output=HdrImage(graph,"Tonemapped linear",frame.width,frame.height);PassData data;data.parameters.x=1;data.flags.x=static_cast<std::uint32_t>(m_tonemapper+.5f);data.flags.y=m_clipping>=.5f?1u:0u;
        std::array inputs{frame.color};auto result=m_pass.Add(graph,"Tonemap",inputs,output,frame,data);if (result) frame.color=output;return result;
    }
    std::span<const FeatureParameter> Parameters() noexcept override { return m_parameters; }
private:
    FullscreenPass m_pass;std::filesystem::path m_shaders;float m_tonemapper=0,m_clipping=0;std::array<FeatureParameter,2> m_parameters{{{"Tonemapper (0 ACES, 1 PBR Neutral)",&m_tonemapper,0,1},{"Clipping overlay",&m_clipping,0,1}}};
};
}
std::unique_ptr<IModule> MakeBloomModule(FeatureRegistry& registry,Rhi::Device& device,const std::filesystem::path& shaders) { return std::make_unique<FeatureModule>(registry,std::make_unique<Bloom>(device,shaders)); }
std::unique_ptr<IModule> MakeTonemapModule(FeatureRegistry& registry,Rhi::Device& device,const std::filesystem::path& shaders) { return std::make_unique<FeatureModule>(registry,std::make_unique<Tonemap>(device,shaders)); }
}
