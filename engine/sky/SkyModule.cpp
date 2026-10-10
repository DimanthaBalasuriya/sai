#include "engine/sky/SceneBridge.hpp"
#include "engine/lighting/Lighting.hpp"
#include "engine/scene/Commands.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <algorithm>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
namespace Engine::Sky {
namespace {
constexpr Scene::ComponentType types[]{kTime,kAtmosphere,kCelestial,kStars,kClouds,kWeather};
std::size_t Index(Scene::ComponentType type){for(std::size_t i=0;i<std::size(types);++i)if(type==types[i])return i;return std::size(types);}
Result Write(Scene::World& world,Guid id,Scene::ComponentType type,const void* data){if(!id)return Result::Success();Scene::ModuleComponent encoded;auto r=EncodeComponent(type,data,encoded);if(!r)return r;auto schema=Schemas()[Index(type)];for(std::size_t i=0;i<schema.fields.size();++i){Scene::FieldValue before;r=world.ReadField(id,type,schema.fields[i].name,before);if(!r)return r;if(before!=encoded.fields[i]){Scene::SetFieldCommand command(id,type,std::string(schema.fields[i].name),encoded.fields[i]);r=command.Do(world);if(!r)return r;}}return Result::Success();}
Result Light(Scene::World& world,const CelestialBody& body,Vec3 direction,float visibility){
    if(!body.light||!world.Contains(body.light)||body.directionSource==2)return Result::Success();
    Scene::EntityDefinition entity;auto r=world.Definition(body.light,entity);if(!r)return r;
    const bool physical=world.HasComponent(body.light,Lighting::kDirectionalLight);
    if(!physical&&(!entity.light||entity.light->type!=0))return Result::Success();
    glm::vec3 incoming(-direction.x,-direction.y,-direction.z);glm::vec3 up=std::abs(incoming.y)>.99f?glm::vec3(0,0,1):glm::vec3(0,1,0);
    auto orientation=glm::inverse(glm::lookAt(glm::vec3(0),incoming,up));float z,y,x;glm::extractEulerAngleZYX(orientation,z,y,x);
    auto transform=entity.transform;transform.rotation={x,y,z};
    if(transform.rotation!=entity.transform.rotation){Scene::SetTransformCommand command(body.light,transform);r=command.Do(world);if(!r)return r;}
    auto type=physical?Lighting::kDirectionalLight:Scene::ComponentType::Light;
    // Celestial authoring value 1 denotes 100,000 lux sun or 5 lux moon.
    float illuminance=body.intensity*visibility*(body.type==0?100000.f:5.f);
    for(auto field:{"color",physical?"illuminance":"intensity"}){
        Scene::FieldValue value=std::string_view(field)=="color"?Scene::FieldValue(body.color):Scene::FieldValue(illuminance),before;
        r=world.ReadField(body.light,type,field,before);if(!r)return r;
        if(before!=value){Scene::SetFieldCommand command(body.light,type,field,value);r=command.Do(world);if(!r)return r;}
    }
    if(physical){Scene::FieldValue unit;r=world.ReadField(body.light,type,"unit",unit);if(!r)return r;if(std::get<std::uint32_t>(unit)!=0){Scene::SetFieldCommand command(body.light,type,"unit",std::uint32_t{0});r=command.Do(world);}}
    return r;
}
Result ResolveAuthoredLight(Scene::World& world,CelestialBody& body){
    if(body.directionSource!=2||!body.light||!world.Contains(body.light))return Result::Success();
    Scene::EntityDefinition entity;Mat4 transform;auto r=world.Definition(body.light,entity);if(!r)return r;r=world.GlobalTransform(body.light,transform);if(!r)return r;
    auto matrix=glm::make_mat4(transform.values.data());auto direction=glm::normalize(glm::vec3(matrix*glm::vec4(0,0,1,0)));body.direction={direction.x,direction.y,direction.z};
    for(auto& component:entity.extensions)if(component.type==Lighting::kDirectionalLight){
        auto light=Lighting::FindLightType(component.type)->packer(component,transform,body.light);Lighting::DirectionalLight authored;Lighting::Decode(component,&authored);
        body.color=light.color;body.intensity=(light.gpu.flags&Renderer::kLightEnabled)?light.intensity/(body.type==0?100000.f:5.f):0;body.angularSize=authored.angularDiameter;return Result::Success();
    }
    if(entity.light){body.color=entity.light->color;body.intensity=entity.light->intensity/(body.type==0?100000.f:5.f);}
    return Result::Success();
}

}
Result EncodeComponent(Scene::ComponentType type,const void* object,Scene::ModuleComponent& output){auto index=Index(type);if(index==std::size(types))return Result::Failure("Unknown sky component");Scene::ModuleComponent next{type,{}};for(auto& field:Schemas()[index].fields){Scene::FieldValue value;auto r=ReadField(Schemas()[index],object,field.name,value);if(!r)return r;next.fields.push_back(std::move(value));}output=std::move(next);return Result::Success();}
Result DecodeComponent(const Scene::ModuleComponent& component,void* object){auto index=Index(component.type);if(index==std::size(types)||component.fields.size()!=Schemas()[index].fields.size())return Result::Failure("Sky component schema mismatch");for(std::size_t i=0;i<component.fields.size();++i){auto r=WriteField(Schemas()[index],object,Schemas()[index].fields[i].name,component.fields[i]);if(!r)return r;}return Result::Success();}
Result RegisterComponents(){auto lighting=Lighting::RegisterComponents();if(!lighting)return lighting;Settings defaults;const void* objects[]{&defaults.time,&defaults.atmosphere,&defaults.sun,&defaults.stars,&defaults.clouds,&defaults.weather};Scene::World schema;for(std::size_t i=0;i<std::size(types);++i){bool registered=false;for(auto& info:schema.Reflection())if(info.type==types[i]){if(info.name!=Schemas()[i].name)return Result::Failure("Sky component identity collision");registered=true;}if(registered)continue;Scene::ModuleComponent component;auto r=EncodeComponent(types[i],objects[i],component);if(!r)return r;r=Scene::RegisterComponent(types[i],Schemas()[i].name,Schemas()[i].fields,component.fields);if(!r)return r;}return Result::Success();}
Result ReadSettings(Scene::World& world,Settings& settings,Guid& settingsEntity,Guid& sunEntity,Guid& moonEntity){settingsEntity=sunEntity=moonEntity={};std::vector<Guid> ids,scratch;auto r=Result::Success();for(auto type:types){r=world.EntitiesWithComponent(type,scratch);if(!r)return r;ids.insert(ids.end(),scratch.begin(),scratch.end());}auto less=[](Guid a,Guid b){return a.high<b.high||(a.high==b.high&&a.low<b.low);};std::sort(ids.begin(),ids.end(),less);ids.erase(std::unique(ids.begin(),ids.end()),ids.end());for(auto id:ids){Scene::EntityDefinition entity;r=world.Definition(id,entity);if(!r)return r;for(auto& component:entity.extensions){if(component.type==kCelestial){CelestialBody body;r=DecodeComponent(component,&body);if(!r)return r;if(body.type==0&&!sunEntity){settings.sun=body;sunEntity=id;}if(body.type==1&&!moonEntity){settings.moon=body;moonEntity=id;}}else {void* object=nullptr;if(component.type==kTime){if(settingsEntity)continue;settingsEntity=id;object=&settings.time;}else if(component.type==kAtmosphere)object=&settings.atmosphere;else if(component.type==kStars)object=&settings.stars;else if(component.type==kClouds)object=&settings.clouds;else if(component.type==kWeather)object=&settings.weather;if(object){r=DecodeComponent(component,object);if(!r)return r;}}}}r=ResolveAuthoredLight(world,settings.sun);if(r)r=ResolveAuthoredLight(world,settings.moon);return r?Validate(settings):r;}
Result EnsureSettings(Scene::World& world,Scene::CommandHistory& history){Settings settings;Guid config,sun,moon;auto r=ReadSettings(world,settings,config,sun,moon);if(!r)return r;if(config&&sun&&moon)return Result::Success();std::vector<Scene::EntityDefinition> definitions;std::vector<Guid> ids;world.Entities(ids);for(auto id:ids){Scene::EntityDefinition e;world.Definition(id,e);definitions.push_back(std::move(e));}
    auto append=[&](Scene::EntityDefinition& e,Scene::ComponentType type,const void* object){Scene::ModuleComponent c;auto result=EncodeComponent(type,object,c);if(result)e.extensions.push_back(std::move(c));return result;};
    auto fresh=[&](std::uint64_t index){Guid id{0x50524f43534b59,index};while(world.Contains(id)||std::any_of(definitions.begin(),definitions.end(),[&](auto& e){return e.id==id;}))++id.low;return id;};
    if(!config){Scene::EntityDefinition e;e.id=fresh(1);e.name.value="Procedural Sky";const void* objects[]{&settings.time,&settings.atmosphere,&settings.stars,&settings.clouds,&settings.weather};Scene::ComponentType configTypes[]{kTime,kAtmosphere,kStars,kClouds,kWeather};for(std::size_t i=0;i<5;++i){r=append(e,configTypes[i],objects[i]);if(!r)return r;}definitions.push_back(std::move(e));}
    if(!sun){auto it=std::find_if(definitions.begin(),definitions.end(),[](auto& e){return (e.light&&e.light->type==0)||std::any_of(e.extensions.begin(),e.extensions.end(),[](auto& c){return c.type==Lighting::kDirectionalLight;});});if(it==definitions.end()){Scene::EntityDefinition e;e.id=fresh(2);e.name.value="Sky Sun";e.transform.rotation={-.9f,-.45f,0};Lighting::DirectionalLight light;Scene::ModuleComponent component;r=Lighting::Encode(Lighting::kDirectionalLight,&light,component);if(!r)return r;e.extensions.push_back(std::move(component));definitions.push_back(std::move(e));it=definitions.end()-1;}settings.sun.light=it->id;if(!it->light)settings.sun.directionSource=2;r=append(*it,kCelestial,&settings.sun);if(!r)return r;}
    if(!moon){Scene::EntityDefinition e;e.id=fresh(3);e.name.value="Sky Moon";Lighting::DirectionalLight light;light.illuminance=0;Scene::ModuleComponent component;r=Lighting::Encode(Lighting::kDirectionalLight,&light,component);if(!r)return r;e.extensions.push_back(std::move(component));settings.moon.light=e.id;r=append(e,kCelestial,&settings.moon);if(!r)return r;definitions.push_back(std::move(e));}
    return history.Execute(world,std::make_unique<Scene::ReplaceSceneCommand>(std::move(definitions)));
}
Result SceneSystems::Update(Scene::World& world,double seconds,Settings& settings,CelestialState& celestial){Guid config,sun,moon;auto r=ReadSettings(world,settings,config,sun,moon);if(!r)return r;if(m_previous&&!settings.weather.transitioning&&settings.weather.current==m_previousWeather){if(settings.clouds.coverage!=m_previousClouds.coverage)settings.weather.current.coverage=settings.clouds.coverage;if(settings.clouds.typeBlend!=m_previousClouds.typeBlend)settings.weather.current.type=settings.clouds.typeBlend;if(settings.clouds.wind!=m_previousClouds.wind)settings.weather.current.wind=settings.clouds.wind;settings.weather.target=settings.weather.current;}r=AdvanceTime(settings.time,seconds);if(!r)return r;r=m_weather.Update(settings.weather,settings.clouds,seconds);if(!r)return r;m_previousWeather=settings.weather.current;m_previousClouds=settings.clouds;m_previous=true;celestial=EvaluateCelestials(settings.time,settings.sun,settings.moon);if(!settings.moon.directionSource)settings.moon.phase=celestial.moonPhase;if(config){r=Write(world,config,kTime,&settings.time);if(r&&world.HasComponent(config,kWeather))r=Write(world,config,kWeather,&settings.weather);if(r&&world.HasComponent(config,kClouds))r=Write(world,config,kClouds,&settings.clouds);if(!r)return r;}if(moon){r=Write(world,moon,kCelestial,&settings.moon);if(!r)return r;}r=Light(world,settings.sun,celestial.sun,celestial.daylight);if(!r)return r;float phase=(1-std::cos(celestial.moonPhase*6.2831853f))*.5f;return Light(world,settings.moon,celestial.moon,phase*(1-celestial.daylight)*std::clamp(celestial.moon.y*8,0.f,1.f));}
}
