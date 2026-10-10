#pragma once
#include "SceneBridge.hpp"
namespace Engine::Sky {
// Optional module owns all GPU images. Snapshot contains values, never ECS pointers.
class ProceduralSky final:public Extensions::IRenderFeature {
public:
    ProceduralSky(Rhi::Device&,std::filesystem::path);~ProceduralSky();
    const char* Name()const noexcept override{return "Procedural Sky";}
    int Order()const noexcept override{return -5;}
    int PostOrder()const noexcept override{return 10;}
    Result Initialize()override;Result PrepareFrame()override;
    Result BeforeGeometry(Renderer::RenderGraph&,Extensions::FeatureFrame&,Renderer::RenderWorld&)override;
    Result AddPasses(Renderer::RenderGraph&,Extensions::FeatureFrame&)override;
    void Snapshot(const Settings&,const CelestialState&,double elapsed);
    LayerStack& Layers();SkyContext& Context();
    void DebugView(std::uint32_t);std::uint32_t DebugView()const;
    void ForcedAltitude(float meters);float ForcedAltitude()const;float Altitude()const;float SpaceBlend()const;
    Vec3 ReferenceError()const;
private:struct Impl;std::unique_ptr<Impl> m;
};
std::unique_ptr<IModule> MakeSkyModule(Extensions::FeatureRegistry&,Rhi::Device&,const std::filesystem::path&);
ProceduralSky* FindSky(Extensions::FeatureRegistry&);
// A separate optional layer demonstrates extension without changes to other layers.
std::unique_ptr<ISkyLayer> MakeAuroraLayer(std::function<Result(Renderer::RenderGraph&,SkyContext&)>);
Result ApplyWeatherPreset(Scene::World&,Scene::CommandHistory&,Guid,const WeatherParameters&,float seconds);
}
