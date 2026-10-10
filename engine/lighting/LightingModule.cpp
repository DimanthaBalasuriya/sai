#include "engine/lighting/Lighting.hpp"
#include "engine/extensions/Features.hpp"
#include "render/LightBufferCache.hpp"
#include "render/GpuLighting.hpp"
#include "render/LtcTables.hpp"
#include "engine/lighting/MemoryPolicy.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
namespace Engine::Lighting {
namespace {
class LightUpload final:public Extensions::IRenderFeature {
    struct Slot {Rhi::BufferHandle frame,bounds,cells,indices,counters,areas;Rhi::TextureTableHandle table;std::uint32_t capacity=0,indexCapacity=0;bool used=false;std::array<float,6> projection{};};
public:
    LightUpload(Rhi::Device& device,std::filesystem::path shaders):m_device(device),m_shaders(std::move(shaders)){}
    ~LightUpload(){m_device.WaitIdle();for(auto buffer:m_buffers)if(buffer)m_device.Destroy(buffer);for(auto& s:m_slots)for(auto b:{s.frame,s.bounds,s.cells,s.indices,s.counters,s.areas})if(b)m_device.Destroy(b);for(auto& s:m_slots)if(s.table)m_device.Destroy(s.table);for(auto b:m_retired)m_device.Destroy(b);if(m_cluster)m_device.Destroy(m_cluster);if(m_ltc)m_device.Destroy(m_ltc);if(m_atmosphereSampler)m_device.Destroy(m_atmosphereSampler);}
    const char* Name()const noexcept override{return "Lighting";}
    int Order()const noexcept override{return 1;}
    std::span<const Extensions::FeatureParameter> Parameters()noexcept override{return m_parameters;}
    Result PrepareFrame()override{for(auto b:m_retired){auto r=m_device.Destroy(b);if(!r)return r;}m_retired.clear();return Result::Success();}
    Result Initialize()override{
        auto result=RegisterComponents();if(!result)return result;
        result=m_device.CreateComputePipeline({"Cluster build/cull",m_shaders/"cluster.compute.spv",16},m_cluster);if(!result)return result;
        result=m_device.CreateSampler({"Atmosphere transmittance",true,false},m_atmosphereSampler);if(!result)return result;auto lut=LtcTables();result=m_device.CreateBuffer({"LTC fit tables",lut.size()*sizeof(LtcEntry),Rhi::BufferUsage::Storage|Rhi::BufferUsage::DeviceAddress|Rhi::BufferUsage::TransferDestination},m_ltc);if(result)result=m_device.UploadBuffer(m_ltc,std::as_bytes(std::span{lut}),Rhi::ResourceState::StorageRead);if(!result)return result;
for(std::size_t slot=0;slot<Rhi::kFramesInFlight;++slot){
            result=m_device.CreateTextureTable("Cluster resources",m_slots[slot].table);if(!result)return result;
            result=Buffer("GPU lights",LightBufferCache::kCapacity*sizeof(Renderer::GpuLight),m_buffers[slot]);if(!result)return result;
            result=Buffer("Lighting frame",sizeof(GpuLighting),m_slots[slot].frame);if(!result)return result;
            result=Buffer("Area shapes",LightBufferCache::kCapacity*32,m_slots[slot].areas);if(!result)return result;result=Buffer("Cluster counters",16,m_slots[slot].counters);if(!result)return result;
        }return Result::Success();
    }
    Result BeforeGeometry(Renderer::RenderGraph& graph,Extensions::FeatureFrame& frame,Renderer::RenderWorld& world)override{
        if(!frame.lit)return Result::Success();
        const auto before=m_cache.PackCount();auto result=m_cache.Update(world);if(!result)return result;
        const auto slot=frame.slot;auto bytes=std::as_bytes(m_cache.Lights());std::uint64_t uploaded=0;auto pending=m_cache.Pending(slot);
        for(std::size_t i=0;i<pending.size();){auto begin=pending[i],end=begin+1;++i;while(i<pending.size()&&pending[i]==end){++end;++i;}
            auto offset=begin*sizeof(Renderer::GpuLight),length=(end-begin)*sizeof(Renderer::GpuLight);result=m_device.WriteBuffer(m_buffers[slot],bytes.subspan(offset,length),offset);if(!result)return result;uploaded+=length;
        }
        m_cache.Complete(slot);auto shadows=world.lighting;world.lighting={m_buffers[slot],m_device.BufferAddress(m_buffers[slot]),m_cache.DirectionalCount(),m_cache.LocalCount(),uploaded,m_cache.PackCount()-before,m_cache.Rebuilds()};
        world.lighting.exposureEv=world.camera.ev100;world.lighting.exposureTarget=shadows.exposureTarget;world.lighting.histogram=shadows.histogram;world.lighting.shadowAtlas=shadows.shadowAtlas;world.lighting.shadowBuffer=shadows.shadowBuffer;world.lighting.shadowAddress=shadows.shadowAddress;world.lighting.shadowPasses=shadows.shadowPasses;world.lighting.shadowTiles=shadows.shadowTiles;
        auto& s=m_slots[slot];GpuLighting gpu;gpu.view=glm::inverse(glm::make_mat4(world.camera.transform.values.data()));float tangent=std::tan(world.camera.verticalFov*.5f);
        gpu.camera={tangent*frame.width/frame.height,tangent,0,world.lighting.shadowAddress?1.f:0.f};gpu.grid={(frame.width+63)/64,(frame.height+63)/64,24,256};std::uint32_t count=gpu.grid.x*gpu.grid.y*gpu.grid.z;
        gpu.grid.w=ClusterListCapacity(count,std::uint32_t(m_listBudget));if(count>s.capacity||s.indexCapacity!=gpu.grid.w){for(auto b:{s.bounds,s.cells,s.indices})if(b)m_retired.push_back(b);s.bounds=s.cells=s.indices={};
            result=Buffer("Cluster AABBs",count*32,s.bounds);if(result)result=Buffer("Cluster cells",count*8,s.cells);if(result)result=Buffer("Cluster indices",std::size_t(count)*gpu.grid.w*4,s.indices);if(!result)return result;std::uint32_t zero=0;result=m_device.WriteBuffer(s.indices,std::as_bytes(std::span{&zero,1}));if(!result)return result;s.capacity=count;s.indexCapacity=gpu.grid.w;s.projection={};
        }
        auto& info=world.lighting;std::array<std::uint32_t,4> counters{};if(s.used){result=m_device.ReadBuffer(s.counters,std::as_writable_bytes(std::span{counters}));if(!result)return result;}
        info.frameBuffer=s.frame;info.frameAddress=m_device.BufferAddress(s.frame);info.clusterBounds=s.bounds;info.clusterCells=s.cells;info.clusterIndices=s.indices;info.clusterCounters=s.counters;info.clusterCount=count;info.overflow=counters[0];info.maxPerCluster=counters[1];info.visibleLocal=counters[3];
        float nearZ=world.camera.nearPlane,farZ=world.camera.farPlane,scale=24/std::log(farZ/nearZ);gpu.depth={nearZ,farZ,scale,-std::log(nearZ)*scale};
        gpu.screen={float(frame.width),float(frame.height),m_mode,0};gpu.lights=info.address;gpu.bounds=m_device.BufferAddress(s.bounds);gpu.cells=m_device.BufferAddress(s.cells);gpu.indices=m_device.BufferAddress(s.indices);gpu.counters=m_device.BufferAddress(s.counters);gpu.localCount=info.localCount;gpu.directionalCount=info.directionalCount;gpu.shadows=info.shadowAddress;
        struct Shape{glm::vec4 right,up;};m_shapes.resize(4+info.localCount);std::uint32_t dir=0,local=4;for(auto& light:world.lights){auto index=light.type==0?dir++:local++;auto& shape=m_shapes[index];shape={{light.right.x,light.right.y,light.right.z,light.width},{light.up.x,light.up.y,light.up.z,light.height}};}result=m_device.WriteBuffer(s.areas,std::as_bytes(std::span{m_shapes}));if(!result)return result;info.areaBuffer=s.areas;info.ltcBuffer=m_ltc;gpu.areas=m_device.BufferAddress(s.areas);gpu.ltc=m_device.BufferAddress(m_ltc);
for(std::uint32_t i=0;i<4;++i){auto t=world.environment.bodyTransmission[i];gpu.bodyTransmission[i]={t.x,t.y,t.z,0};}gpu.atmosphere={world.atmosphere.x,world.atmosphere.y,world.atmosphere.z,0};if(world.atmosphericTransmittance){result=m_device.WriteTexture(s.table,0,world.atmosphericTransmittance,m_atmosphereSampler);if(!result)return result;}result=m_device.WriteBuffer(s.frame,std::as_bytes(std::span{&gpu,1}));if(!result)return result;
        auto lights=graph.ImportBuffer("GPU physical lights",m_buffers[slot],m_device.StateOf(m_buffers[slot]));auto data=graph.ImportBuffer("Lighting frame",s.frame,m_device.StateOf(s.frame));
        auto bounds=graph.ImportBuffer("Cluster bounds",s.bounds,m_device.StateOf(s.bounds)),cells=graph.ImportBuffer("Cluster cells",s.cells,m_device.StateOf(s.cells)),indices=graph.ImportBuffer("Cluster indices",s.indices,m_device.StateOf(s.indices)),stats=graph.ImportBuffer("Cluster counters",s.counters,m_device.StateOf(s.counters));
        std::array<float,6> projection{gpu.camera.x,gpu.camera.y,nearZ,farZ,float(frame.width),float(frame.height)};bool reuseBounds=s.projection==projection;s.projection=projection;for(std::uint32_t mode=0;mode<2;++mode){if(mode&&m_mode<1.5f&&(m_mode>.5f||info.localCount<=16))continue;using namespace Renderer;using Rhi::ResourceState;std::vector<ResourceUse> uses{{data,mode?ResourceState::StorageRead:ResourceState::StorageReadWrite},{lights,mode?ResourceState::StorageRead:ResourceState::StorageReadWrite},{bounds,mode?ResourceState::StorageRead:ResourceState::StorageReadWrite},{cells,ResourceState::StorageReadWrite},{stats,ResourceState::StorageReadWrite}};if(!mode&&world.lighting.shadowBuffer)uses.push_back({graph.ReuseBuffer("Shadow index records",world.lighting.shadowBuffer,m_device.StateOf(world.lighting.shadowBuffer)),ResourceState::StorageRead});if(mode)uses.push_back({indices,ResourceState::StorageReadWrite});if(world.atmosphericTransmittance)uses.push_back({graph.ReuseImage("Atmospheric sun transmission",world.atmosphericTransmittance,m_device.StateOf(world.atmosphericTransmittance)),ResourceState::SampledRead});
            graph.AddPass({mode?"Lighting light cull":"Lighting cluster build",std::move(uses),[this,address=info.frameAddress,mode,count,local=info.localCount,table=s.table,reuseBounds](Rhi::CommandList& cmd,const PassResources&){struct Push{std::uint64_t frame;std::uint32_t mode,pad;}push{address,mode,reuseBounds?1u:0u};auto r=cmd.BindPipeline(m_cluster);if(r)r=cmd.BindTextureTable(table);if(r)r=cmd.PushConstants(std::as_bytes(std::span{&push,1}));return r?cmd.Dispatch(std::max(1u,mode?local:(std::max(count,local)+63)/64),1,1):r;},{}});
        }s.used=true;return Result::Success();
    }
    Result AddPasses(Renderer::RenderGraph&,Extensions::FeatureFrame&)override{return Result::Success();}
private:
    Result Buffer(const char* name,std::size_t bytes,Rhi::BufferHandle& b){return m_device.CreateBuffer({name,bytes,Rhi::BufferUsage::Storage|Rhi::BufferUsage::DeviceAddress,true},b);}
    Rhi::Device& m_device;std::filesystem::path m_shaders;LightBufferCache m_cache;std::array<Rhi::BufferHandle,Rhi::kFramesInFlight> m_buffers{};std::array<Slot,Rhi::kFramesInFlight> m_slots{};std::vector<Rhi::BufferHandle> m_retired;Rhi::PipelineHandle m_cluster;Rhi::BufferHandle m_ltc;Rhi::SamplerHandle m_atmosphereSampler;struct ShapeCpu{glm::vec4 right,up;};std::vector<ShapeCpu> m_shapes;
    float m_mode=0,m_listBudget=16;std::array<Extensions::FeatureParameter,2> m_parameters{{{"Local path: Auto / Loop / Cluster",&m_mode,0,2},{"Cluster lists MiB per frame",&m_listBudget,1,128}}};
};
}
std::unique_ptr<IModule> MakeLightingModule(Extensions::FeatureRegistry& registry,Rhi::Device& device,const std::filesystem::path& shaders){return std::make_unique<Extensions::FeatureModule>(registry,std::make_unique<LightUpload>(device,shaders));}
}

