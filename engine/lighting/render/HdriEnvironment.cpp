#include "engine/lighting/Providers.hpp"
#include "ConstantAmbient.hpp"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4505)
#endif
#include <stb_image.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <glm/gtc/packing.hpp>
namespace Engine::Lighting {
class HdriEnvironment final:public Extensions::IRenderFeature,public IEnvironmentLighting {
    struct Slot {Rhi::BufferHandle sh;std::array<Rhi::ImageHandle,5> specular;Rhi::ImageHandle brdf;std::array<Rhi::TextureTableHandle,7> tables;std::uint64_t generation=UINT64_MAX;Vec3 ambient{-1,-1,-1};};
    struct Push {std::uint64_t sh;std::uint32_t mode,size;float roughness;std::uint32_t hasTexture;std::array<std::uint32_t,2> padding{};glm::vec4 ambient;};
public:
    HdriEnvironment(Rhi::Device& d,std::filesystem::path p):m_device(d),m_path(std::move(p)){}
    ~HdriEnvironment(){m_device.WaitIdle();for(auto& s:m_slots){if(s.sh)m_device.Destroy(s.sh);if(s.brdf)m_device.Destroy(s.brdf);for(auto i:s.specular)if(i)m_device.Destroy(i);for(auto t:s.tables)if(t)m_device.Destroy(t);}if(m_source)m_device.Destroy(m_source);if(m_pipeline)m_device.Destroy(m_pipeline);if(m_sampler)m_device.Destroy(m_sampler);}
    const char* Name()const noexcept override{return "Environment Lighting";}int Order()const noexcept override{return 5;}
    Result Initialize()override{
        auto r=RegisterSettings();if(r)r=m_device.CreateComputePipeline({"SH projection / GGX prefilter / BRDF integration",m_path/"environment.compute.spv",sizeof(Push)},m_pipeline);if(r)r=m_device.CreateSampler({"HDRI sampler",true,true},m_sampler);
        for(auto& s:m_slots){if(r)r=m_device.CreateBuffer({"SH9 irradiance",9*16,Rhi::BufferUsage::Storage|Rhi::BufferUsage::DeviceAddress,true},s.sh);for(int i=0;i<5;++i)if(r)r=m_device.CreateImage({"GGX environment roughness "+std::to_string(i),64u>>i,64u>>i,Rhi::Format::Rgba16Float,Rhi::ImageUsage::Sampled|Rhi::ImageUsage::Storage,1,Rhi::ImageDimension::Cube},s.specular[i]);if(r)r=m_device.CreateImage({"Integrated BRDF LUT",64,64,Rhi::Format::Rgba16Float,Rhi::ImageUsage::Sampled|Rhi::ImageUsage::Storage},s.brdf);for(auto& table:s.tables)if(r)r=m_device.CreateTextureTable("Environment compute",table);}return r;
    }
    Result PrepareFrame()override{
        if(m_pending==m_loaded)return Result::Success();
        Rhi::ImageHandle next;if(!m_pending.empty()){int width,height,channels;float* pixels=stbi_loadf(m_pending.c_str(),&width,&height,&channels,4);if(!pixels)return Result::Failure("Cannot load HDRI: "+m_pending);std::vector<std::uint16_t> half(std::size_t(width)*height*4);for(std::size_t i=0;i<half.size();++i)half[i]=glm::packHalf1x16(std::clamp(pixels[i],0.f,65504.f));stbi_image_free(pixels);auto r=m_device.CreateImage({"HDRI source",std::uint32_t(width),std::uint32_t(height),Rhi::Format::Rgba16Float,Rhi::ImageUsage::Sampled|Rhi::ImageUsage::TransferDestination},next);if(r)r=m_device.UploadImage(next,std::as_bytes(std::span{half}));if(!r){if(next)m_device.Destroy(next);return r;}}
        auto r=m_device.WaitIdle();if(!r)return r;if(m_source)m_device.Destroy(m_source);m_source=next;m_loaded=m_pending;++m_generation;return Result::Success();
    }
    Result BeforeGeometry(Renderer::RenderGraph& g,Extensions::FeatureFrame& f,Renderer::RenderWorld& w)override{
        if(!f.lit)return Result::Success();if(w.settings.environment==0)return m_constant.Evaluate(g,f,w);if(w.settings.environment==2)return Result::Success();m_pending=w.settings.hdri;return Evaluate(g,f,w);
    }
    Result Evaluate(Renderer::RenderGraph& g,Extensions::FeatureFrame& f,Renderer::RenderWorld& w)override{
        auto& s=m_slots[f.slot];auto color=w.settings.ambient;bool dirty=s.generation!=m_generation||s.ambient.x!=color.x||s.ambient.y!=color.y||s.ambient.z!=color.z;
        if(dirty){auto sh=g.ReuseBuffer("Environment SH9",s.sh,m_device.StateOf(s.sh));Renderer::ResourceId source;if(m_source)source=g.ReuseImage("HDRI source",m_source,m_device.StateOf(m_source));
            for(std::uint32_t pass=0;pass<7;++pass){auto mode=pass==0?0u:pass==6?2u:1u,size=pass==6?64u:pass?64u>>(pass-1):16u;auto image=pass==6?s.brdf:pass?s.specular[pass-1]:Rhi::ImageHandle{};std::vector<Renderer::ResourceUse> uses;if(!pass)uses.push_back({sh,Rhi::ResourceState::StorageReadWrite});if(m_source&&mode!=2)uses.push_back({source,Rhi::ResourceState::SampledRead});Renderer::ResourceId output;if(image){output=g.ReuseImage("Environment prefilter",image,m_device.StateOf(image));uses.push_back({output,Rhi::ResourceState::StorageReadWrite});}
                Push push{m_device.BufferAddress(s.sh),mode,size,pass?float(pass-1)/4:0,m_source?1u:0u,{}, {color.x,color.y,color.z,0}};auto table=s.tables[pass];g.AddPass({pass==0?"Environment Lighting SH projection":pass==6?"Environment Lighting BRDF LUT":"Environment Lighting GGX prefilter "+std::to_string(pass),std::move(uses),[this,push,table,image](Rhi::CommandList& cmd,const Renderer::PassResources&){auto r=Result::Success();if(m_source&&push.mode!=2)r=m_device.WriteTexture(table,0,m_source,m_sampler);if(image&&r)r=m_device.WriteStorageImage(table,0,image);if(r)r=cmd.BindPipeline(m_pipeline);if(r)r=cmd.BindTextureTable(table);if(r)r=cmd.PushConstants(std::as_bytes(std::span{&push,1}));return r?cmd.Dispatch((push.size+7)/8,push.mode==0?1:(push.size+7)/8,push.mode==1?6:1):r;},{}});
            }s.generation=m_generation;s.ambient=color;
        }
        std::vector<Renderer::ResourceUse> ready{{g.ReuseBuffer("Environment SH9 ready",s.sh,m_device.StateOf(s.sh)),Rhi::ResourceState::StorageRead}};for(auto image:s.specular)ready.push_back({g.ReuseImage("Environment specular ready",image,m_device.StateOf(image)),Rhi::ResourceState::SampledRead});ready.push_back({g.ReuseImage("BRDF ready",s.brdf,m_device.StateOf(s.brdf)),Rhi::ResourceState::SampledRead});g.AddPass({"Environment Lighting ready",std::move(ready),{},{}});
        w.environment.diffuseCube={};w.environment.irradiance=s.sh;w.environment.irradianceAddress=m_device.BufferAddress(s.sh);w.environment.specular=s.specular;w.environment.brdf=s.brdf;w.environment.intensity=w.settings.environmentIntensity;return Result::Success();
    }
    Result AddPasses(Renderer::RenderGraph&,Extensions::FeatureFrame&)override{return Result::Success();}
private:Rhi::Device& m_device;std::filesystem::path m_path;std::array<Slot,Rhi::kFramesInFlight> m_slots;Rhi::ImageHandle m_source;Rhi::PipelineHandle m_pipeline;Rhi::SamplerHandle m_sampler;ConstantAmbient m_constant;std::string m_pending,m_loaded;std::uint64_t m_generation=0;
};
std::unique_ptr<IModule> MakeEnvironmentModule(Extensions::FeatureRegistry& r,Rhi::Device& d,const std::filesystem::path& p){return std::make_unique<Extensions::FeatureModule>(r,std::make_unique<HdriEnvironment>(d,p));}
}
