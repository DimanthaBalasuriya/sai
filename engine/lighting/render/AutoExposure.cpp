#include "engine/lighting/Providers.hpp"
#include "engine/renderer/Exposure.hpp"
#include <chrono>
namespace Engine::Lighting {
class AutoExposure final:public Extensions::IRenderFeature {
    struct State {std::array<std::uint32_t,256> histogram{};float ev=15;std::uint32_t valid=0;float target=15,pad=0;};
    struct Slot {Rhi::BufferHandle state;std::array<Rhi::TextureTableHandle,3> tables{};bool used=false;};
    struct Push {std::uint64_t state;std::uint32_t mode,width,height;float exposure,oldEv,minEv,maxEv,speedUp,speedDown,dt;};
public:
    AutoExposure(Rhi::Device& d,std::filesystem::path p):m_device(d),m_path(std::move(p)){}
    ~AutoExposure(){m_device.WaitIdle();for(auto& s:m_slots){if(s.state)m_device.Destroy(s.state);for(auto t:s.tables)if(t)m_device.Destroy(t);}if(m_pipeline)m_device.Destroy(m_pipeline);if(m_sampler)m_device.Destroy(m_sampler);}
    const char* Name()const noexcept override{return "Auto Exposure";}int Order()const noexcept override{return -10;}int PostOrder()const noexcept override{return 25;}
    Result Initialize()override{auto r=RegisterSettings();if(r)r=m_device.CreateComputePipeline({"Exposure histogram/adaptation",m_path/"exposure.compute.spv",sizeof(Push)},m_pipeline);if(r)r=m_device.CreateSampler({"Exposure sample",false,false},m_sampler);for(auto& s:m_slots){if(r)r=m_device.CreateBuffer({"Auto Exposure histogram",sizeof(State),Rhi::BufferUsage::Storage|Rhi::BufferUsage::DeviceAddress,true},s.state);for(auto& table:s.tables)if(r)r=m_device.CreateTextureTable("Exposure table",table);}return r;}
    Result BeforeGeometry(Renderer::RenderGraph&,Extensions::FeatureFrame& f,Renderer::RenderWorld& w)override{
        auto now=std::chrono::steady_clock::now();m_dt=std::clamp(std::chrono::duration<float>(now-m_previous).count(),.001f,.1f);m_previous=now;
        if(f.debugView!=0&&w.settings.autoExposure){if(m_active)w.camera.ev100=m_ev;return Result::Success();}if(!f.lit||!w.settings.autoExposure){m_active=false;return Result::Success();}auto& s=m_slots[f.slot];
        if(!m_active||m_source!=w.source){m_ev=w.camera.ev100;m_source=w.source;for(auto& slot:m_slots)slot.used=false;m_active=true;}
        if(s.used){State state;auto r=m_device.ReadBuffer(s.state,std::as_writable_bytes(std::span{&state,1}));if(!r)return r;if(state.valid){m_ev=state.ev;w.lighting.exposureTarget=state.target;for(int i=0;i<256;++i)w.lighting.histogram[i]=float(state.histogram[i]);}}
        m_ev=std::clamp(m_ev,w.settings.minEv,w.settings.maxEv);w.camera.ev100=m_ev;return Result::Success();
    }
    Result AddPasses(Renderer::RenderGraph& g,Extensions::FeatureFrame& f)override{
        if(!f.lit||f.debugView!=0||!f.world->settings.autoExposure)return Result::Success();auto& s=m_slots[f.slot];auto& settings=f.world->settings;
        if(!s.used){State state;state.ev=m_ev;auto r=m_device.WriteBuffer(s.state,std::as_bytes(std::span{&state,1}));if(!r)return r;}
        auto buffer=g.ReuseBuffer("Auto Exposure histogram",s.state,m_device.StateOf(s.state));
        for(std::uint32_t mode=0;mode<3;++mode){std::vector<Renderer::ResourceUse> uses{{buffer,Rhi::ResourceState::StorageReadWrite}};if(mode==1)uses.push_back({f.color,Rhi::ResourceState::SampledRead});
            Push push{m_device.BufferAddress(s.state),mode,f.width,f.height,Renderer::ManualExposure(f.world->camera.ev100,f.world->camera.exposureCompensation),m_ev,settings.minEv,settings.maxEv,settings.speedUp,settings.speedDown,m_dt};auto image=f.color;auto table=s.tables[mode];
            g.AddPass({mode==0?"Auto Exposure histogram clear":mode==1?"Auto Exposure histogram":"Auto Exposure adaptation",std::move(uses),[this,push,image,table](Rhi::CommandList& cmd,const Renderer::PassResources& resources){auto r=Result::Success();if(push.mode==1)r=m_device.WriteTexture(table,0,resources.Image(image),m_sampler);if(r)r=cmd.BindPipeline(m_pipeline);if(r)r=cmd.BindTextureTable(table);if(r)r=cmd.PushConstants(std::as_bytes(std::span{&push,1}));return r?cmd.Dispatch(push.mode==1?(push.width+15)/16:push.mode==0?16:1,push.mode==1?(push.height+15)/16:1,1):r;},{}});
        }s.used=true;return Result::Success();
    }
private:Rhi::Device& m_device;std::filesystem::path m_path;std::array<Slot,Rhi::kFramesInFlight> m_slots;Rhi::PipelineHandle m_pipeline;Rhi::SamplerHandle m_sampler;Guid m_source;bool m_active=false;float m_ev=15,m_dt=1.f/60;std::chrono::steady_clock::time_point m_previous=std::chrono::steady_clock::now();
};
std::unique_ptr<IModule> MakeExposureModule(Extensions::FeatureRegistry& r,Rhi::Device& d,const std::filesystem::path& p){return std::make_unique<Extensions::FeatureModule>(r,std::make_unique<AutoExposure>(d,p));}
}
