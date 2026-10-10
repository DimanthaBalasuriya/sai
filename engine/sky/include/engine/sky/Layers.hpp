#pragma once
#include "Systems.hpp"
#include "engine/extensions/Features.hpp"
namespace Engine::Sky {
struct DebugViewRegistry {
    struct Entry {std::string name;Renderer::ResourceId resource;};std::vector<Entry> views;
    void Add(std::string name,Renderer::ResourceId resource){views.push_back({std::move(name),resource});}
};
struct SkyContext {
    Settings settings;CelestialState celestial;double elapsed=0;
    Extensions::FeatureFrame* frame=nullptr;
    Renderer::ResourceId transmittance,multiScattering,skyView,aerialPerspective,clouds,weatherMap,cloudShadow;
    DebugViewRegistry debug;
};
struct ISkyLayer {
    virtual ~ISkyLayer()=default;
    virtual const char* Name() const noexcept=0;
    virtual int Priority() const noexcept=0;
    virtual Result Initialize(){return Result::Success();}
    virtual Result Declare(Renderer::RenderGraph&,SkyContext&)=0;
    virtual void DebugViews(DebugViewRegistry&){}
};
class LayerStack {
public:
    struct Entry {std::unique_ptr<ISkyLayer> layer;bool enabled=true;int priority=0;};
    Result Add(std::unique_ptr<ISkyLayer>);
    Result Remove(std::string_view);Result SetPriority(std::string_view,int);
    bool SetEnabled(std::string_view,bool);
    Result Initialize();Result Declare(Renderer::RenderGraph&,SkyContext&);
    std::span<const Entry> Entries() const{return m_entries;}
private:std::vector<Entry> m_entries;bool m_started=false;
};
class SkyRenderFeature final:public Extensions::IRenderFeature {
public:
    const char* Name() const noexcept override{return "Procedural Sky";}
    int Order() const noexcept override{return 10;}
    Result Initialize() override{return m_layers.Initialize();}
    Result AddPasses(Renderer::RenderGraph&,Extensions::FeatureFrame&) override;
    LayerStack& Layers(){return m_layers;}SkyContext& Context(){return m_context;}
private:LayerStack m_layers;SkyContext m_context;
};
}
