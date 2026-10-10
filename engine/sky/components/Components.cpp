#include "engine/sky/Components.hpp"
#include <algorithm>
#include <cmath>
#include <type_traits>
namespace Engine::Sky {
namespace {
using Scene::FieldInfo;using Scene::FieldType;
#define F(T,n,lo,hi) FieldInfo{#n,FieldType::Float,offsetof(T,n),lo,hi}
#define U(T,n,lo,hi) FieldInfo{#n,FieldType::Unsigned,offsetof(T,n),lo,hi}
#define V(T,n,lo,hi) FieldInfo{#n,FieldType::Vector3,offsetof(T,n),lo,hi}
constexpr FieldInfo time[]{F(TimeOfDay,time,0,24),F(TimeOfDay,dayLength,.01f,864000),F(TimeOfDay,speed,-1000,1000),U(TimeOfDay,paused,0,1),F(TimeOfDay,day,-1000000,1000000),F(TimeOfDay,latitude,-90,90),F(TimeOfDay,longitude,-180,180),F(TimeOfDay,dayOfYear,1,366)};
constexpr FieldInfo atmosphere[]{F(SkyAtmosphere,planetRadius,1000,100000000),F(SkyAtmosphere,atmosphereHeight,100,1000000),V(SkyAtmosphere,rayleigh,0,.01f),F(SkyAtmosphere,rayleighHeight,1,100000),V(SkyAtmosphere,mie,0,.01f),F(SkyAtmosphere,mieHeight,1,100000),F(SkyAtmosphere,mieAbsorption,0,.01f),F(SkyAtmosphere,mieAnisotropy,-.95f,.95f),V(SkyAtmosphere,ozone,0,.01f),F(SkyAtmosphere,ozoneCenter,0,1000000),F(SkyAtmosphere,ozoneWidth,1,1000000),V(SkyAtmosphere,groundAlbedo,0,1),F(SkyAtmosphere,sunIntensity,0,1000),F(SkyAtmosphere,exposureHint,.01f,100)};
constexpr FieldInfo celestial[]{U(CelestialBody,type,0,1),U(CelestialBody,directionSource,0,2),V(CelestialBody,direction,-1,1),V(CelestialBody,color,0,100),F(CelestialBody,angularSize,.001f,10),F(CelestialBody,intensity,0,2e12f),F(CelestialBody,phase,0,1),F(CelestialBody,albedo,0,1),FieldInfo{"light",FieldType::Guid,offsetof(CelestialBody,light),0,0}};
constexpr FieldInfo stars[]{F(StarField,density,0,1),F(StarField,brightness,0,100),F(StarField,magnitudeCutoff,-2,12),F(StarField,twinkle,0,1),F(StarField,colorVariation,0,1),F(StarField,milkyWay,0,10)};
constexpr FieldInfo clouds[]{F(VolumetricClouds,altitude,100,30000),F(VolumetricClouds,thickness,100,30000),F(VolumetricClouds,coverage,0,1),F(VolumetricClouds,density,0,10),F(VolumetricClouds,typeBlend,0,1),V(VolumetricClouds,wind,-1000,1000),F(VolumetricClouds,shapeScale,.000001f,.1f),F(VolumetricClouds,detailScale,.000001f,1),F(VolumetricClouds,shadowStrength,0,1),U(VolumetricClouds,quality,0,2)};
#define W(group,n,type,lo,hi) FieldInfo{#group "." #n,FieldType::type,offsetof(WeatherState,group)+offsetof(WeatherParameters,n),lo,hi}
constexpr FieldInfo weather[]{W(current,coverage,Float,0,1),W(current,type,Float,0,1),W(current,precipitation,Float,0,1),W(current,wind,Vector3,-1000,1000),W(current,fogDensity,Float,0,.1f),W(current,wetness,Float,0,1),W(target,coverage,Float,0,1),W(target,type,Float,0,1),W(target,precipitation,Float,0,1),W(target,wind,Vector3,-1000,1000),W(target,fogDensity,Float,0,.1f),W(target,wetness,Float,0,1),F(WeatherState,duration,0,86400),U(WeatherState,transitioning,0,1)};
const ComponentSchema schemas[]{{"TimeOfDay",sizeof(TimeOfDay),time},{"SkyAtmosphere",sizeof(SkyAtmosphere),atmosphere},{"CelestialBody",sizeof(CelestialBody),celestial},{"StarField",sizeof(StarField),stars},{"VolumetricClouds",sizeof(VolumetricClouds),clouds},{"WeatherState",sizeof(WeatherState),weather}};
bool Range(float v,const FieldInfo& field){return std::isfinite(v)&&v>=field.minimum&&v<=field.maximum;}
}
std::span<const ComponentSchema> Schemas(){return schemas;}
Result ReadField(const ComponentSchema& schema,const void* data,std::string_view name,Scene::FieldValue& value){
    if(!data)return Result::Failure("Missing sky component");for(const auto& field:schema.fields)if(field.name==name){auto p=static_cast<const std::byte*>(data)+field.offset;switch(field.type){case FieldType::Float:value=*reinterpret_cast<const float*>(p);break;case FieldType::Unsigned:value=*reinterpret_cast<const std::uint32_t*>(p);break;case FieldType::Vector3:value=*reinterpret_cast<const Vec3*>(p);break;case FieldType::Guid:value=*reinterpret_cast<const Guid*>(p);break;default:return Result::Failure("Unsupported sky field");}return Result::Success();}return Result::Failure("Unknown sky field");
}
Result WriteField(const ComponentSchema& schema,void* data,std::string_view name,const Scene::FieldValue& value){
    if(!data)return Result::Failure("Missing sky component");for(const auto& field:schema.fields)if(field.name==name){auto p=static_cast<std::byte*>(data)+field.offset;
        if(field.type==FieldType::Float&&std::holds_alternative<float>(value)&&Range(std::get<float>(value),field))*reinterpret_cast<float*>(p)=std::get<float>(value);
        else if(field.type==FieldType::Unsigned&&std::holds_alternative<std::uint32_t>(value)&&Range(float(std::get<std::uint32_t>(value)),field))*reinterpret_cast<std::uint32_t*>(p)=std::get<std::uint32_t>(value);
        else if(field.type==FieldType::Vector3&&std::holds_alternative<Vec3>(value)){auto v=std::get<Vec3>(value);if(!Range(v.x,field)||!Range(v.y,field)||!Range(v.z,field))return Result::Failure("Sky vector outside reflected range");*reinterpret_cast<Vec3*>(p)=v;}
        else if(field.type==FieldType::Guid&&std::holds_alternative<Guid>(value))*reinterpret_cast<Guid*>(p)=std::get<Guid>(value);
        else return Result::Failure("Sky field type/range mismatch");return Result::Success();}return Result::Failure("Unknown sky field");
}
Result Validate(const Settings& s){const void* components[]{&s.time,&s.atmosphere,&s.sun,&s.stars,&s.clouds,&s.weather};for(std::size_t i=0;i<std::size(schemas);++i)for(const auto& field:schemas[i].fields){Scene::FieldValue value;auto r=ReadField(schemas[i],components[i],field.name,value);if(!r)return r;auto copy=value;r=std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,float>)return Range(v,field);else if constexpr(std::is_same_v<T,std::uint32_t>)return Range(float(v),field);else if constexpr(std::is_same_v<T,Vec3>)return Range(v.x,field)&&Range(v.y,field)&&Range(v.z,field);else return true;},copy)?Result::Success():Result::Failure("Invalid sky component field: "+std::string(field.name));if(!r)return r;}for(const auto& field:celestial){Scene::FieldValue value;auto r=ReadField(schemas[2],&s.moon,field.name,value);if(!r)return r;CelestialBody check=s.moon;r=WriteField(schemas[2],&check,field.name,value);if(!r)return r;}return Result::Success();}
QualitySettings Quality(std::uint32_t tier){if(tier==0)return {128,32,16,128,72,24,16,24,4,16,24};if(tier==2)return {512,128,48,256,144,48,32,48,2,96,4};return {256,64,32,192,108,32,24,32,4,32,12};}
}
