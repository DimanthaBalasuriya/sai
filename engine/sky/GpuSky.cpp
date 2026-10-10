#include "engine/sky/GpuSky.hpp"
#include "engine/renderer/Exposure.hpp"
#include "engine/sky/OrbitalMath.hpp"
#include "layers/CallbackLayer.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstring>
#include <cmath>
namespace Engine::Sky {
namespace {
using namespace Renderer;
struct GpuParameters {
 glm::mat4 inverseVP{1},previousVP{1};
 glm::vec4 cameraTime{},sun{},moon{},sunColor{},moonColor{},rayleigh{},mie{},ozone{},planet{},ground{},clouds{},wind{},weather{},stars{},celestial{},quality{},shadowOrigin{},extra{},extra2{},orbital{};
};
struct ComputeData{std::uint64_t settings;std::uint32_t mode,width,height,depth;};
static_assert(sizeof(ComputeData)==24&&sizeof(GpuParameters)==448);
glm::vec3 V(Vec3 v){return {v.x,v.y,v.z};}
enum Image:std::size_t{Trans,Multi,View,Aerial,Noise,Weather,Shadow,Ibl,Cloud,Distance,History0,Distance0,History1,Distance1,Count};
constexpr const char* imageNames[]{"Sky transmittance","Sky multiple scattering","Sky view","Aerial froxels","Perlin Worley volume","Cloud weather map","Cloud shadows","Sky diffuse environment","Cloud current","Cloud depth","Cloud history A","Cloud distance A","Cloud history B","Cloud distance B"};
}
struct ProceduralSky::Impl {
 Rhi::Device& device;std::filesystem::path directory;LayerStack layers;SkyContext context;
 Rhi::PipelineHandle rmsePipeline;std::array<std::array<Rhi::TextureTableHandle,Rhi::kFramesInFlight>,2> rmseTables{};Vec3 referenceError{};std::array<Rhi::PipelineHandle,11> compute{};Rhi::SamplerHandle sampler;
 std::array<std::array<Rhi::TextureTableHandle,Rhi::kFramesInFlight>,11> tables{};
 std::array<std::unique_ptr<Extensions::FullscreenPass>,7> graphics;
 Rhi::ImageHandle fallback;std::vector<Rhi::ImageHandle> retired;
 struct Slot {std::array<Rhi::ImageHandle,Count> images{};Rhi::BufferHandle parameters,rmse;bool measured=false;GpuParameters previous;std::vector<Scene::FieldValue> atmosphereKey;std::uint32_t width=0,height=0,quality=UINT32_MAX,frames=0,ping=0;bool initialized=false,history=false;};
 std::array<Slot,Rhi::kFramesInFlight> slots;
 std::array<ResourceId,Count> resources;ResourceId parameters;
 std::uint32_t debug=0,slot=0;std::uint64_t layerKey=0;double elapsed=0;float forcedAltitude=-1,altitude=0,spaceBlend=0;bool spaceMode=false;
 Impl(Rhi::Device& d,std::filesystem::path p):device(d),directory(std::move(p)){}
 ~Impl(){device.WaitIdle();for(auto& s:slots){for(auto image:s.images)if(image)device.Destroy(image);if(s.parameters)device.Destroy(s.parameters);if(s.rmse)device.Destroy(s.rmse);}for(auto image:retired)device.Destroy(image);for(auto& ts:tables)for(auto t:ts)if(t)device.Destroy(t);if(fallback)device.Destroy(fallback);for(auto pipeline:compute)if(pipeline)device.Destroy(pipeline);if(sampler)device.Destroy(sampler);if(rmsePipeline)device.Destroy(rmsePipeline);for(auto& mode:rmseTables)for(auto table:mode)if(table)device.Destroy(table);}
 bool Enabled(std::string_view name){for(auto& e:layers.Entries())if(e.layer->Name()==name)return e.enabled;return false;}
 Result Allocate(std::uint32_t index,const Extensions::FeatureFrame& f){
  auto& s=slots[index];auto q=Quality(context.settings.clouds.quality);if(s.width==f.width&&s.height==f.height&&s.quality==context.settings.clouds.quality)return Result::Success();
  std::array<Rhi::ImageDesc,Count> desc;auto two=[&](std::size_t i,std::uint32_t w,std::uint32_t h){desc[i]={imageNames[i],w,h,Rhi::Format::Rgba16Float,Rhi::ImageUsage::Sampled|Rhi::ImageUsage::Storage};};
  two(Trans,q.transmittanceWidth,q.transmittanceHeight);two(Multi,q.multiSize,q.multiSize);two(View,q.skyWidth,q.skyHeight);two(Aerial,q.froxelWidth,q.froxelHeight);desc[Aerial].dimension=Rhi::ImageDimension::Texture3D;desc[Aerial].depth=q.froxelDepth;
  two(Noise,32,32);desc[Noise].dimension=Rhi::ImageDimension::Texture3D;desc[Noise].depth=32;two(Weather,128,128);two(Shadow,128,128);two(Ibl,8,8);desc[Ibl].dimension=Rhi::ImageDimension::Cube;
  for(std::size_t i=Cloud;i<Count;++i)two(i,std::max(1u,(f.width+q.cloudDivisor-1)/q.cloudDivisor),std::max(1u,(f.height+q.cloudDivisor-1)/q.cloudDivisor));
  std::array<Rhi::ImageHandle,Count> next{};for(std::size_t i=0;i<Count;++i){auto r=device.CreateImage(desc[i],next[i]);if(!r){for(auto image:next)if(image)retired.push_back(image);return r;}}
  for(auto image:s.images)if(image)retired.push_back(image);s.images=next;s.width=f.width;s.height=f.height;s.quality=context.settings.clouds.quality;s.initialized=s.history=false;s.frames=s.ping=0;s.atmosphereKey.clear();return Result::Success();
 }
 void Import(RenderGraph& graph){auto& s=slots[slot];for(std::size_t i=0;i<Count;++i)resources[i]=graph.ImportImage(imageNames[i],s.images[i],device.StateOf(s.images[i]));parameters=graph.ImportBuffer("Sky parameter snapshot",s.parameters,device.StateOf(s.parameters));}
 Result Dispatch(RenderGraph& graph,const char* name,std::uint32_t mode,std::initializer_list<ResourceId> input,std::initializer_list<ResourceId> output,std::uint32_t width,std::uint32_t height,std::uint32_t depth=1){
  std::vector<ResourceId> in(input),out(output);std::vector<ResourceUse> uses{{parameters,Rhi::ResourceState::StorageRead}};for(auto r:in)if(std::none_of(uses.begin(),uses.end(),[&](auto& u){return u.resource==r;}))uses.push_back({r,Rhi::ResourceState::SampledRead});for(auto r:out)uses.push_back({r,Rhi::ResourceState::StorageReadWrite});auto currentSlot=slot;auto param=parameters;
  auto pass=graph.AddPass({name,std::move(uses),[this,in,out,currentSlot,param,mode,width,height,depth](Rhi::CommandList& cmd,const PassResources& r){auto table=tables[mode][currentSlot];for(std::uint32_t i=0;i<in.size();++i){auto result=device.WriteTexture(table,i,r.Image(in[i]),sampler);if(!result)return result;}for(std::uint32_t i=0;i<out.size();++i){auto result=device.WriteStorageImage(table,i,r.Image(out[i]));if(!result)return result;}auto result=cmd.BindPipeline(compute[mode]);if(result)result=cmd.BindTextureTable(table);ComputeData data{device.BufferAddress(r.Buffer(param)),mode,width,height,depth};if(result)result=cmd.PushConstants(std::as_bytes(std::span{&data,1}));return result?cmd.Dispatch((width+7)/8,(height+7)/8,depth):result;},{}});
  return pass==UINT32_MAX?Result::Failure("Sky compute declaration failed"):Result::Success();
 }
 Result Composite(RenderGraph& graph,SkyContext& c,std::uint32_t mode){
  if(debug==13&&mode!=0)return Result::Success();if(mode==2&&c.celestial.starsVisibility==0&&debug!=6&&spaceBlend==0)return Result::Success();
  auto& f=*c.frame;std::vector<ResourceId> inputs{f.color};
  if(mode==0)inputs.insert(inputs.end(),{resources[View],resources[Trans],resources[Multi]});
  else if(mode==3){auto& s=slots[slot];inputs.insert(inputs.end(),{resources[s.ping?History1:History0],f.depth,resources[s.ping?Distance1:Distance0]});}
  else if(mode==4)inputs.insert(inputs.end(),{f.depth,resources[Aerial]});
  else if(mode==5){auto cube=graph.ImportImage("Fallback cubemap",fallback,device.StateOf(fallback));inputs.push_back(cube);}
  else inputs.insert(inputs.end(),{resources[View],resources[Trans],resources[Multi]});
  // Slots four/five are stable debug resources in every graphics table.
  while(inputs.size()<4)inputs.push_back(resources[Multi]); // Duplicate logical uses must be removed below.
  inputs.push_back(resources[Weather]);inputs.push_back(resources[Shadow]);inputs.push_back(resources[Trans]);inputs.push_back(resources[Multi]);
  auto output=Extensions::HdrImage(graph,std::string("Sky composite ")+std::to_string(mode),f.width,f.height);
  Extensions::PassData data=f.data;data.parameters.w=Renderer::ManualExposure(f.world->camera.ev100,f.world->camera.exposureCompensation)*5000.f;auto address=device.BufferAddress(slots[slot].parameters);data.flags={static_cast<std::uint32_t>(address),static_cast<std::uint32_t>(address>>32),debug,mode};
  // FullscreenPass supports aliased source slots through one dependency per image.
  auto result=graphics[mode]->Add(graph,std::string("Sky ")+ (mode==0?"atmosphere":mode==1?"celestial":mode==2?"stars":mode==3?"cloud composite":mode==4?"aerial perspective":mode==5?"cubemap fallback":"aurora"),inputs,output,f,data,std::array<ResourceUse,1>{{{parameters,Rhi::ResourceState::StorageRead}}});
  if(result){f.color=output;}if(result&&mode==0&&debug==13){auto& s=slots[slot];auto buffer=graph.ReuseBuffer("Sky reference error",s.rmse,device.StateOf(s.rmse));std::uint32_t count=(f.width*f.height+63)/64;if(count>262144)return Result::Failure("Sky reference view exceeds 16 megapixels");for(std::uint32_t phase=0;phase<2;++phase){std::vector<ResourceUse> uses{{buffer,Rhi::ResourceState::StorageReadWrite}};if(!phase)uses.push_back({output,Rhi::ResourceState::SampledRead});struct Push{std::uint64_t values;std::uint32_t mode,width,height,count;}push{device.BufferAddress(s.rmse),phase,f.width,f.height,count};auto table=rmseTables[phase][slot];graph.AddPass({phase?"Sky reference RMSE":"Sky reference squared errors",std::move(uses),[this,push,table,output](Rhi::CommandList& cmd,const PassResources& resources){auto r=Result::Success();if(!push.mode)r=device.WriteTexture(table,0,resources.Image(output),sampler);if(r)r=cmd.BindPipeline(rmsePipeline);if(r)r=cmd.BindTextureTable(table);if(r)r=cmd.PushConstants(std::as_bytes(std::span{&push,1}));return r?cmd.Dispatch(push.mode?1:push.count,1,1):r;},{}});}s.measured=true;}return result;
 }
 Result Clouds(RenderGraph& g,SkyContext& c){if(spaceBlend>.99f)return Result::Success();auto& s=slots[slot];auto q=Quality(s.quality);auto w=std::max(1u,(s.width+q.cloudDivisor-1)/q.cloudDivisor),h=std::max(1u,(s.height+q.cloudDivisor-1)/q.cloudDivisor);auto r=Dispatch(g,"Sky cloud raymarch",8,{c.frame->depth,resources[Noise],resources[Weather],resources[Trans]},{resources[Cloud],resources[Distance]},w,h);if(!r)return r;
  auto old=s.ping?History1:History0,oldDistance=s.ping?Distance1:Distance0;auto next=s.ping?History0:History1,nextDistance=s.ping?Distance0:Distance1;
  if(!s.history){ // Initialize both ping-pong histories before a descriptor may read them.
   r=Dispatch(g,"Sky cloud history initialize",10,{resources[Cloud],resources[Distance],resources[Cloud],resources[Distance]},{resources[old],resources[oldDistance]},w,h);if(!r)return r;
  }
  r=Dispatch(g,"Sky cloud temporal resolve",9,{resources[Cloud],resources[Distance],resources[old],resources[oldDistance]},{resources[next],resources[nextDistance]},w,h);if(!r)return r;s.ping=1-s.ping;s.history=true;c.clouds=resources[next];return Composite(g,c,3);
 }
};
ProceduralSky::ProceduralSky(Rhi::Device& d,std::filesystem::path path):m(std::make_unique<Impl>(d,std::move(path))){}
ProceduralSky::~ProceduralSky()=default;
LayerStack& ProceduralSky::Layers(){return m->layers;}SkyContext& ProceduralSky::Context(){return m->context;}
void ProceduralSky::Snapshot(const Settings& s,const CelestialState& c,double elapsed){m->context.settings=s;m->context.celestial=c;m->context.elapsed=elapsed;}
void ProceduralSky::DebugView(std::uint32_t d){m->debug=std::min(10u,d);}std::uint32_t ProceduralSky::DebugView()const{return m->debug;}
Result ProceduralSky::Initialize(){
 auto r=RegisterComponents();if(!r)return r;r=m->device.CreateComputePipeline({"Sky reference error reduction",m->directory/"sky-rmse.compute.spv",24},m->rmsePipeline);if(!r)return r;for(auto& mode:m->rmseTables)for(auto& table:mode){r=m->device.CreateTextureTable("Sky reference table",table);if(!r)return r;}for(auto& slot:m->slots){r=m->device.CreateBuffer({"Sky RMSE partials",(262144+1)*16,Rhi::BufferUsage::Storage|Rhi::BufferUsage::DeviceAddress,true},slot.rmse);if(!r)return r;}for(std::uint32_t i=0;i<m->compute.size();++i){r=m->device.CreateComputePipeline({"Sky compute "+std::to_string(i),m->directory/("sky"+std::to_string(i)+".compute.spv"),sizeof(ComputeData)},m->compute[i]);if(!r)return r;}
 r=m->device.CreateSampler({"Sky clamp sampler",true,false},m->sampler);if(!r)return r;
 for(auto& mode:m->tables)for(auto& table:mode){r=m->device.CreateTextureTable("Sky compute fenced table",table);if(!r)return r;}
 const char* shaderNames[]{"Atmosphere","Celestial","Stars","Clouds","Aerial","Cubemap","Aurora"};for(std::size_t i=0;i<m->graphics.size();++i){auto& g=m->graphics[i];g=std::make_unique<Extensions::FullscreenPass>(m->device);r=g->Initialize(m->directory,shaderNames[i],Rhi::Format::Rgba16Float);if(!r)return r;}
 for(auto& s:m->slots){r=m->device.CreateBuffer({"Sky frame parameters",sizeof(GpuParameters),Rhi::BufferUsage::Storage|Rhi::BufferUsage::DeviceAddress,true},s.parameters);if(!r)return r;}
 Rhi::ImageDesc fallback{"Fallback sky cube",4,4,Rhi::Format::Rgba8Srgb,Rhi::ImageUsage::Sampled|Rhi::ImageUsage::TransferDestination};fallback.dimension=Rhi::ImageDimension::Cube;
 r=m->device.CreateImage(fallback,m->fallback);if(!r)return r;std::array<std::uint8_t,4*4*4*6> pixels{};for(std::size_t i=0;i<pixels.size();i+=4){pixels[i]=50;pixels[i+1]=75;pixels[i+2]=110;pixels[i+3]=255;}r=m->device.UploadImage(m->fallback,std::as_bytes(std::span{pixels}));if(!r)return r;
 r=m->layers.Add(MakeAtmosphereLayer([this](RenderGraph& g,SkyContext& c){return m->Composite(g,c,0);}));if(!r)return r;
 r=m->layers.Add(MakeCelestialLayer([this](RenderGraph& g,SkyContext& c){return m->Composite(g,c,1);}));if(!r)return r;
 r=m->layers.Add(MakeStarsLayer([this](RenderGraph& g,SkyContext& c){return m->Composite(g,c,2);}));if(!r)return r;
 r=m->layers.Add(MakeCloudsLayer([this](RenderGraph& g,SkyContext& c){return m->Clouds(g,c);}));if(!r)return r;
 r=m->layers.Add(MakeAerialLayer([this](RenderGraph& g,SkyContext& c){return m->Composite(g,c,4);}));if(!r)return r;
 r=m->layers.Add(MakeCubemapLayer([this](RenderGraph& g,SkyContext& c){return m->Composite(g,c,5);}));if(!r)return r;
 r=m->layers.Add(MakeAuroraLayer([this](RenderGraph& g,SkyContext& c){return m->Composite(g,c,6);}));if(!r)return r;m->layers.SetEnabled("Aurora",false);m->layers.SetEnabled("SkyboxCubemap",false);return m->layers.Initialize();
}
Result ProceduralSky::PrepareFrame(){for(auto image:m->retired){auto r=m->device.Destroy(image);if(!r)return r;}m->retired.clear();return Result::Success();}
Result ProceduralSky::BeforeGeometry(RenderGraph& graph,Extensions::FeatureFrame& f,RenderWorld& world){
 if(!f.lit)return Result::Success();m->slot=f.slot;auto r=m->Allocate(f.slot,f);if(!r)return r;auto& s=m->slots[f.slot];if(s.measured){glm::vec4 error;auto read=m->device.ReadBuffer(s.rmse,std::as_writable_bytes(std::span{&error,1}));if(!read)return read;m->referenceError={error.x,error.y,error.z};}auto& settings=m->context.settings;auto q=Quality(settings.clouds.quality);auto camera=f.data.camera;auto c=m->context.celestial;GpuParameters p;double altitude=m->forcedAltitude>=0?double(m->forcedAltitude):double(camera.y);altitude=std::max(1.0,altitude);m->altitude=float(altitude);m->spaceBlend=Engine::Sky::SpaceBlend(altitude);if(altitude>122000)m->spaceMode=true;else if(altitude<48000)m->spaceMode=false;p.orbital={m->spaceBlend,float(altitude),m->debug==14?1.f:0.f,m->spaceMode?1.f:0.f};world.environment.spaceBlend=m->spaceBlend;
 p.inverseVP=f.data.inverseViewProjection;p.previousVP=s.previous.previousVP;p.cameraTime={camera.x,camera.y,camera.z,static_cast<float>(m->context.elapsed)};
 p.sun=glm::vec4(V(c.sun),settings.atmosphere.sunIntensity*settings.sun.intensity);p.moon=glm::vec4(V(c.moon),settings.atmosphere.sunIntensity*settings.moon.intensity);p.sunColor=glm::vec4(V(settings.sun.color),0);p.moonColor=glm::vec4(V(settings.moon.color),c.starsVisibility);
 p.rayleigh=glm::vec4(V(settings.atmosphere.rayleigh),settings.atmosphere.rayleighHeight);p.mie=glm::vec4(V(settings.atmosphere.mie),settings.atmosphere.mieHeight);p.ozone=glm::vec4(V(settings.atmosphere.ozone),settings.atmosphere.ozoneCenter);p.planet={settings.atmosphere.planetRadius,settings.atmosphere.atmosphereHeight,settings.atmosphere.ozoneWidth,settings.atmosphere.mieAbsorption};p.ground=glm::vec4(V(settings.atmosphere.groundAlbedo),settings.atmosphere.mieAnisotropy);
 p.clouds={settings.clouds.altitude,settings.clouds.thickness,settings.clouds.coverage,settings.clouds.density};p.wind=glm::vec4(V(settings.clouds.wind),settings.clouds.shapeScale);p.weather={settings.weather.current.precipitation,settings.clouds.typeBlend,settings.weather.current.fogDensity,settings.clouds.detailScale};p.stars={settings.stars.density,settings.stars.brightness,settings.stars.milkyWay,settings.stars.twinkle};
 std::uint64_t key=0;for(auto& e:m->layers.Entries())key=key*31+e.enabled*7+e.priority;
 bool valid=s.history&&glm::length(glm::vec3(camera)-glm::vec3(s.previous.cameraTime))<300&&glm::dot(glm::vec3(p.sun),glm::vec3(s.previous.sun))>.999&&std::abs(p.clouds.z-s.previous.clouds.z)<.15&&p.cameraTime.w-s.previous.cameraTime.w<.25&&key==m->layerKey&&p.sun.w==s.previous.sun.w&&p.sunColor==s.previous.sunColor;
 p.extra={settings.stars.magnitudeCutoff,settings.stars.colorVariation,settings.moon.albedo,settings.atmosphere.exposureHint};
 p.extra2={static_cast<float>(m->context.elapsed-s.previous.cameraTime.w),c.daylight,settings.weather.current.wetness,0};
 p.celestial={settings.sun.angularSize,settings.moon.angularSize,c.moonPhase,valid?1.f:0.f};p.quality={static_cast<float>(f.width),static_cast<float>(f.height),static_cast<float>(q.cloudSteps),std::min(world.camera.farPlane,100000.f)};p.shadowOrigin={camera.x,camera.y,camera.z,settings.clouds.shadowStrength};
 r=m->device.WriteBuffer(s.parameters,std::as_bytes(std::span{&p,1}));if(!r)return r;m->Import(graph);
 Scene::ModuleComponent atmosphere;EncodeComponent(kAtmosphere,&settings.atmosphere,atmosphere);bool dirty=s.atmosphereKey!=atmosphere.fields;
 if(dirty){r=m->Dispatch(graph,"Sky transmittance LUT",0,{}, {m->resources[Trans]},q.transmittanceWidth,q.transmittanceHeight);if(!r)return r;r=m->Dispatch(graph,"Sky multiple scattering LUT",1,{m->resources[Trans]},{m->resources[Multi]},q.multiSize,q.multiSize);if(!r)return r;s.atmosphereKey=atmosphere.fields;}
 r=m->Dispatch(graph,"Sky view LUT",2,{m->resources[Trans],m->resources[Multi]},{m->resources[View]},q.skyWidth,q.skyHeight);if(!r)return r;
 r=m->Dispatch(graph,"Sky aerial froxels",3,{m->resources[Trans],m->resources[Multi]},{m->resources[Aerial]},q.froxelWidth,q.froxelHeight,q.froxelDepth);if(!r)return r;
 if(!s.initialized){r=m->Dispatch(graph,"Sky cloud noise volume",4,{}, {m->resources[Noise]},32,32,32);if(!r)return r;}
 r=m->Dispatch(graph,"Sky weather map",5,{}, {m->resources[Weather]},128,128);if(!r)return r;
 r=m->Dispatch(graph,"Sky cloud shadows",6,{m->resources[Trans],m->resources[Noise],m->resources[Weather]},{m->resources[Shadow]},128,128);if(!r)return r;
 if(!s.initialized||s.frames%std::max(1u,std::uint32_t(q.iblInterval*(1-m->spaceBlend)+1))==0||!valid){r=m->Dispatch(graph,"Sky diffuse environment",7,{m->resources[View]},{m->resources[Ibl]},8,8,6);if(!r)return r;}
 graph.AddPass({"Sky lighting ready",{{m->resources[Ibl],Rhi::ResourceState::SampledRead},{m->resources[Shadow],Rhi::ResourceState::SampledRead}},{},{}});
 // Legacy sky radiance calibration: 20 sky units correspond to 100,000 lux.
 if(world.settings.environment==2){world.environment.intensity=5000.f*(1-m->spaceBlend);world.environment.diffuseCube=s.images[Ibl];}world.environment.wetness=settings.weather.current.wetness;if(m->Enabled("Volumetric clouds"))world.environment.shadow=s.images[Shadow];glm::mat4 shadow(0);shadow[0][0]=1.f/40000;shadow[2][1]=1.f/40000;shadow[3][0]=.5f-camera.x/40000;shadow[3][1]=.5f-camera.z/40000;shadow[3][3]=1;std::memcpy(world.environment.worldToShadow.values.data(),glm::value_ptr(shadow),sizeof(shadow));
 world.atmosphericTransmittance=s.images[Trans];world.atmosphere={settings.atmosphere.planetRadius,settings.atmosphere.atmosphereHeight,float(altitude)};s.previous=p;s.previous.previousVP=f.viewProjection;s.initialized=true;++s.frames;m->layerKey=key;return Result::Success();
}
Result ProceduralSky::AddPasses(RenderGraph& graph,Extensions::FeatureFrame& f){if(!f.lit)return Result::Success();m->slot=f.slot;m->Import(graph);m->context.frame=&f;m->context.debug.views.clear();auto& r=m->resources;m->context.transmittance=r[Trans];m->context.multiScattering=r[Multi];m->context.skyView=r[View];m->context.aerialPerspective=r[Aerial];m->context.weatherMap=r[Weather];m->context.cloudShadow=r[Shadow];m->context.debug.Add("Transmittance",r[Trans]);m->context.debug.Add("Multiple scattering",r[Multi]);m->context.debug.Add("Sky view",r[View]);m->context.debug.Add("Aerial perspective",r[Aerial]);m->context.debug.Add("Weather",r[Weather]);m->context.debug.Add("Cloud shadow",r[Shadow]);auto result=m->layers.Declare(graph,m->context);graph.AddPass({"Procedural Sky",{{m->parameters,Rhi::ResourceState::StorageRead}},{},{}});m->context.frame=nullptr;return result;}
ProceduralSky* FindSky(Extensions::FeatureRegistry& registry){for(auto& entry:registry.Entries())if(auto sky=dynamic_cast<ProceduralSky*>(entry.feature))return sky;return nullptr;}
namespace {
class Module final:public IModule {
public:Module(Extensions::FeatureRegistry& r,Rhi::Device& d,std::filesystem::path p):registry(r),sky(d,std::move(p)){}const char* Name()const noexcept override{return "Procedural Sky";}
 Result OnStartup(EngineContext&)noexcept override{try{auto r=sky.Initialize();if(!r)return r;r=registry.Register(sky);if(r)registry.SetEnabled("Skybox",false);return r;}catch(const std::exception& e){return Result::Failure(e.what());}}
 void OnShutdown(EngineContext&)noexcept override{registry.Remove(sky);}
private:Extensions::FeatureRegistry& registry;ProceduralSky sky;
};
}
std::unique_ptr<IModule> MakeSkyModule(Extensions::FeatureRegistry& r,Rhi::Device& d,const std::filesystem::path& p){return std::make_unique<Module>(r,d,p);}
Result ApplyWeatherPreset(Scene::World& world,Scene::CommandHistory& history,Guid config,const WeatherParameters& target,float seconds){Scene::EntityDefinition definition;auto r=world.Definition(config,definition);if(!r)return r;auto found=std::find_if(definition.extensions.begin(),definition.extensions.end(),[](auto& c){return c.type==kWeather;});if(found==definition.extensions.end())return Result::Failure("WeatherState missing");WeatherState state;DecodeComponent(*found,&state);WeatherSystem transition;r=transition.Transition(state,target,seconds);if(!r)return r;EncodeComponent(kWeather,&state,*found);std::vector<Scene::EntityDefinition> definitions;std::vector<Guid> ids;world.Entities(ids);for(auto id:ids){Scene::EntityDefinition e;if(id==config)e=definition;else world.Definition(id,e);definitions.push_back(std::move(e));}return history.Execute(world,std::make_unique<Scene::ReplaceSceneCommand>(std::move(definitions)));}
}


namespace Engine::Sky {void ProceduralSky::ForcedAltitude(float meters){m->forcedAltitude=meters;}float ProceduralSky::ForcedAltitude()const{return m->forcedAltitude;}float ProceduralSky::Altitude()const{return m->altitude;}float ProceduralSky::SpaceBlend()const{return m->spaceBlend;}Vec3 ProceduralSky::ReferenceError()const{return m->referenceError;}}