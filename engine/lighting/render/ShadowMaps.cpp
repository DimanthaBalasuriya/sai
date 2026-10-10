#include "ShadowMaps.hpp"
#include "LightBufferCache.hpp"
#include "engine/renderer/Projection.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstring>
#include <unordered_map>
namespace Engine::Lighting {
namespace {
Mat4 Store(const glm::mat4& m){Mat4 out;std::copy_n(glm::value_ptr(m),16,out.values.begin());return out;}
std::uint64_t HashBytes(std::uint64_t h,const void* p,std::size_t n){auto* b=static_cast<const unsigned char*>(p);for(std::size_t i=0;i<n;++i)h=(h^b[i])*1099511628211ull;return h;}
class ShadowMaps final:public Extensions::IRenderFeature,public IShadowTechnique {
    struct Slot {Rhi::ImageHandle atlas;Rhi::BufferHandle data;std::unordered_map<std::uint32_t,std::uint64_t> hashes;Guid source;};
public:
    explicit ShadowMaps(Rhi::Device& d,const std::filesystem::path& path):m_device(d),m_debug(d),m_path(path){}
    ~ShadowMaps(){m_device.WaitIdle();for(auto& s:m_slots){if(s.atlas)m_device.Destroy(s.atlas);if(s.data)m_device.Destroy(s.data);}}
    const char* Name()const noexcept override{return "Shadow Maps";}int Order()const noexcept override{return 0;}
    int PostOrder()const noexcept override{return 38;}
    Result Initialize()override{for(auto& s:m_slots){auto r=m_device.CreateImage({"Cached 4096 shadow atlas",4096,4096,Rhi::Format::D32Float,Rhi::ImageUsage::DepthAttachment|Rhi::ImageUsage::Sampled},s.atlas);if(r)r=m_device.CreateBuffer({"Shadow records",LightBufferCache::kCapacity*sizeof(ShadowRecord),Rhi::BufferUsage::Storage|Rhi::BufferUsage::DeviceAddress,true},s.data);if(!r)return r;}return m_debug.Initialize(m_path,"shadowatlas",Rhi::Format::Rgba16Float);}
    std::span<const Extensions::FeatureParameter> Parameters()noexcept override{return m_parameters;}
    Result BeforeGeometry(Renderer::RenderGraph& g,Extensions::FeatureFrame& f,Renderer::RenderWorld& w)override{return f.lit?Declare(g,f,w):Result::Success();}
    Result AddPasses(Renderer::RenderGraph& g,Extensions::FeatureFrame& f)override{if(f.debugView!=11||!f.world->lighting.shadowAtlas)return Result::Success();auto source=g.ReuseImage("Shadow atlas viewer",f.world->lighting.shadowAtlas,m_device.StateOf(f.world->lighting.shadowAtlas));auto output=Extensions::HdrImage(g,"Shadow atlas display",f.width,f.height);auto r=m_debug.Add(g,"Shadow Maps atlas viewer",std::span{&source,1},output,f,{});if(r)f.color=output;return r;}
    Result Declare(Renderer::RenderGraph& g,Extensions::FeatureFrame& f,Renderer::RenderWorld& world)override{
        auto& s=m_slots[f.slot];if(s.source!=world.source){s.hashes.clear();s.source=world.source;}
        if(m_device.StateOf(s.atlas)==Rhi::ResourceState::Undefined){auto atlas=g.ReuseImage("Initialize shadow atlas",s.atlas,Rhi::ResourceState::Undefined);g.AddPass({"Shadow Maps initialize",{{atlas,Rhi::ResourceState::DepthWrite}},[handle=s.atlas](Rhi::CommandList& cmd,const Renderer::PassResources&){Rhi::RenderingDesc desc;desc.depth=handle;desc.clearDepth=0;auto r=cmd.BeginRendering(desc);return r?cmd.EndRendering():r;},{}});}m_allocator.Clear();std::uint32_t directional=0,local=4; m_mapping.resize(world.lights.size());m_order.clear();
        for(std::uint32_t i=0;i<world.lights.size();++i){auto& l=world.lights[i];m_mapping[i]=l.type==0?directional++:local++;if(l.modulePacked&&(l.gpu.flags&3)==3&&glm::length(glm::vec3(l.gpu.color.x,l.gpu.color.y,l.gpu.color.z))>0&&l.type<=2)m_order.push_back(i);}
        if(directional>4||local>LightBufferCache::kCapacity)return Result::Failure("Lighting capacity exceeded: maximum 4 directional / 4096 local lights");m_records.assign(local,{});auto camera=glm::vec3(f.data.camera);auto score=[&](const auto& l){auto p=glm::vec3(l.position.x,l.position.y,l.position.z)-camera;return l.range*l.range/(glm::dot(p,p)+1);};
        std::stable_sort(m_order.begin(),m_order.end(),[&](auto a,auto b){auto& x=world.lights[a];auto& y=world.lights[b];if((x.type==0)!=(y.type==0))return x.type==0;return score(x)>score(y);});
        std::uint32_t passes=0,tiles=0,localLights=0;
        for(auto i:m_order){auto& light=world.lights[i];if(light.type==0&&world.environment.spaceBlend>.99f)continue;if(light.type&&localLights>=std::uint32_t(m_localBudget))continue;auto index=m_mapping[i];auto& record=m_records[index];record.options={light.shadow.depthBias,light.shadow.normalBias,light.shadow.blend,light.shadow.distance};record.meta.y=light.type;record.meta.z=light.shadow.filter;
            auto stableLight=light;stableLight.shadow.resolution=std::min(2048u,light.shadow.resolution);auto cascades=BuildCascades(world.camera,stableLight,float(f.width)/f.height);std::uint32_t faces=light.type==0?std::clamp(light.shadow.cascades,1u,4u):light.type==1?6u:1u;faces=std::min(6u,faces);
            std::array<AtlasTile,6> allocated{};bool complete=true;auto saved=m_allocator;
            auto resolution=light.type==0?std::min(2048u,light.shadow.resolution):std::min(1024u,std::max(256u,std::uint32_t(light.shadow.resolution*std::clamp(std::sqrt(score(light))*.2f,.25f,1.f))));
            for(std::uint32_t face=0;face<faces;++face){allocated[face]=m_allocator.Allocate(resolution);complete&=bool(allocated[face]);}
            if(!complete){m_allocator=saved;continue;}if(light.type)++localLights;
            for(std::uint32_t face=0;face<faces;++face){auto tile=allocated[face];glm::mat4 matrix;
                if(light.type==0){matrix=cascades[face].matrix;record.splits[face]=cascades[face].split;}
                else {glm::vec3 p(light.position.x,light.position.y,light.position.z),d(light.direction.x,light.direction.y,light.direction.z),up(0,1,0);if(light.type==1){static constexpr std::array<glm::vec3,6> directions{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};static constexpr std::array<glm::vec3,6> ups{{{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}}};d=directions[face];up=ups[face];}else if(std::abs(d.y)>.99f)up={0,0,1};
                    matrix=Renderer::PerspectiveReverseZ(light.type==1?1.57079633f:2*light.spotAngle,1,.01f,std::max(.02f,light.range))*glm::lookAt(p,p+d,up);
                }
                record.matrices[face]=matrix;record.tiles[face]={float(tile.x)/4096,float(tile.y)/4096,float(tile.size)/4096,float(tile.size)};
                auto stored=Store(matrix);f.geometry->ShadowCasters(stored,m_candidates);std::sort(m_candidates.begin(),m_candidates.end());std::uint64_t hash=HashBytes(1469598103934665603ull,&matrix,sizeof(matrix));hash=HashBytes(hash,&tile,sizeof(tile));hash=HashBytes(hash,&light.entity,sizeof(light.entity));
                for(auto caster:m_candidates){auto& item=world.instances[caster];hash=HashBytes(hash,&item.entity,sizeof(item.entity));hash=HashBytes(hash,&item.model,sizeof(item.model));hash=HashBytes(hash,&item.mesh,sizeof(item.mesh));hash=HashBytes(hash,&item.material,sizeof(item.material));}
                auto key=index*6+face;auto previous=s.hashes.find(key);if(previous==s.hashes.end()||previous->second!=hash){auto r=f.geometry->AddShadowPass(g,world,s.atlas,stored,m_candidates,f.slot,tile.x,tile.y,tile.size,std::string("Shadow Maps ")+(light.type==0?"cascade ":"tile ")+std::to_string(key));if(!r)return r;s.hashes[key]=hash;++passes;}++tiles;
            }record.meta.x=faces;
        }
        std::erase_if(s.hashes,[&](auto& entry){auto index=entry.first/6,face=entry.first%6;return index>=m_records.size()||face>=m_records[index].meta.x;});auto r=m_device.WriteBuffer(s.data,std::as_bytes(std::span{m_records}));if(!r)return r;
        world.lighting.shadowAtlas=s.atlas;world.lighting.shadowBuffer=s.data;world.lighting.shadowAddress=m_device.BufferAddress(s.data);world.lighting.shadowPasses=passes;world.lighting.shadowTiles=tiles;
        auto atlas=g.ReuseImage("Shadow Maps atlas ready",s.atlas,m_device.StateOf(s.atlas));auto records=g.ReuseBuffer("Shadow Maps records",s.data,m_device.StateOf(s.data));g.AddPass({"Shadow Maps ready",{{atlas,Rhi::ResourceState::SampledRead},{records,Rhi::ResourceState::StorageRead}},{},{}});return Result::Success();
    }
private:Rhi::Device& m_device;Extensions::FullscreenPass m_debug;std::filesystem::path m_path;std::array<Slot,Rhi::kFramesInFlight> m_slots;ShadowAtlasAllocator m_allocator;std::vector<ShadowRecord> m_records;std::vector<std::uint32_t> m_order,m_mapping,m_candidates;float m_localBudget=8;std::array<Extensions::FeatureParameter,1>m_parameters{{{"Shadowed local lights",&m_localBudget,0,32}}};
};
}
std::unique_ptr<IModule> MakeShadowModule(Extensions::FeatureRegistry& registry,Rhi::Device& device,const std::filesystem::path& path){return std::make_unique<Extensions::FeatureModule>(registry,std::make_unique<ShadowMaps>(device,path));}
}
