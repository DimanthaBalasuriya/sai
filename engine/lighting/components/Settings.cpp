#include "engine/lighting/Providers.hpp"
namespace Engine::Lighting {
namespace {
void Environment(const Scene::ModuleComponent& c,Renderer::RenderWorld& w){auto& s=w.settings;s.environment=std::get<std::uint32_t>(c.fields[0]);s.ambient=std::get<Vec3>(c.fields[1]);s.environmentIntensity=std::get<float>(c.fields[2]);s.hdri=std::get<std::string>(c.fields[3]);}
void Fog(const Scene::ModuleComponent& c,Renderer::RenderWorld& w){auto& s=w.settings;s.fog=std::get<std::uint32_t>(c.fields[0]);s.density=std::get<float>(c.fields[1]);s.heightFalloff=std::get<float>(c.fields[2]);s.baseHeight=std::get<float>(c.fields[3]);s.fogColor=std::get<Vec3>(c.fields[4]);s.fogDistance=std::get<float>(c.fields[5]);}
void Exposure(const Scene::ModuleComponent& c,Renderer::RenderWorld& w){auto& s=w.settings;s.autoExposure=std::get<std::uint32_t>(c.fields[0]);s.minEv=std::get<float>(c.fields[1]);s.maxEv=std::get<float>(c.fields[2]);s.speedUp=std::get<float>(c.fields[3]);s.speedDown=std::get<float>(c.fields[4]);}
Result ExposureValid(const Scene::ModuleComponent& c){return std::get<float>(c.fields[1])<=std::get<float>(c.fields[2])?Result::Success():Result::Failure("Minimum exposure must not exceed maximum exposure");}
}
Result RegisterSettings(){
    using namespace Scene;
    static const FieldInfo environment[]{{"provider",FieldType::Unsigned,0,0,2},{"ambient",FieldType::Vector3,0,0,100000},{"intensity",FieldType::Float,0,0,100000},{"hdriPath",FieldType::String,0,0,0}};
    static const FieldValue envDefaults[]{std::uint32_t(2),Vec3{.08f,.08f,.08f},1.f,std::string{}};
    static const FieldInfo fog[]{{"provider",FieldType::Unsigned,0,0,1},{"density",FieldType::Float,0,0,1},{"heightFalloff",FieldType::Float,0,0,10},{"baseHeight",FieldType::Float,0,-1000000,1000000},{"color",FieldType::Vector3,0,0,100000},{"maxDistance",FieldType::Float,0,1,1000000}};
    static const FieldValue fogDefaults[]{std::uint32_t(1),.001f,.1f,0.f,Vec3{.5f,.6f,.7f},1000.f};
    static const FieldInfo exposure[]{{"autoExposure",FieldType::Unsigned,0,0,1},{"minEV100",FieldType::Float,0,-16,32},{"maxEV100",FieldType::Float,0,-16,32},{"speedUp",FieldType::Float,0,.01f,20},{"speedDown",FieldType::Float,0,.01f,20}};
    static const FieldValue exposureDefaults[]{std::uint32_t(1),-10.f,20.f,3.f,1.f};
    World schema;auto add=[&](ComponentType type,const char* name,std::span<const FieldInfo> fields,std::span<const FieldValue> defaults,RenderSettingsExtractor extractor,ComponentValidator validator=nullptr){bool exists=false;for(auto& c:schema.Reflection())if(c.type==type)exists=true;auto r=exists?Result::Success():RegisterComponent(type,name,fields,defaults,validator);return r?RegisterRenderSettingsExtractor(type,extractor):r;};
    auto r=add(kEnvironmentSettings,"EnvironmentSettings (0 Ambient / 1 HDRI / 2 Sky)",environment,envDefaults,Environment);if(r)r=add(kFogSettings,"ExponentialHeightFog",fog,fogDefaults,Fog);if(r)r=add(kExposureSettings,"ExposureSettings",exposure,exposureDefaults,Exposure,ExposureValid);return r;
}
}
