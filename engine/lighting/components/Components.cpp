#include "Schema.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
namespace Engine::Lighting {
std::span<const LightType> LightTypes(){static const LightType types[]{DirectionalType(),PointType(),SpotType(),RectType(),DiskType()};return types;}
const LightType* FindLightType(Scene::ComponentType type) noexcept {for(const auto& entry:LightTypes())if(entry.component==type)return &entry;return nullptr;}
bool HasLight(const Scene::EntityDefinition& entity) noexcept {if(entity.light)return true;for(const auto& c:entity.extensions)if(FindLightType(c.type))return true;return false;}
Result Encode(Scene::ComponentType type,const void* object,Scene::ModuleComponent& output){
    auto* info=FindLightType(type);if(!info||!object)return Result::Failure("Unknown light component");
    output.type=type;output.fields.clear();output.fields.reserve(info->fields.size());
    for(const auto& field:info->fields){auto* address=static_cast<const std::byte*>(object)+field.offset;
        switch(field.type){
        case Scene::FieldType::Vector3:output.fields.emplace_back(*reinterpret_cast<const Vec3*>(address));break;
        case Scene::FieldType::Float:output.fields.emplace_back(*reinterpret_cast<const float*>(address));break;
        case Scene::FieldType::Unsigned:output.fields.emplace_back(*reinterpret_cast<const std::uint32_t*>(address));break;
        default:return Result::Failure("Unsupported light field");}
    }return Result::Success();
}
Result Decode(const Scene::ModuleComponent& component,void* object){
    auto* info=FindLightType(component.type);if(!info||!object||component.fields.size()!=info->fields.size())return Result::Failure("Invalid light component layout");
    for(std::size_t i=0;i<info->fields.size();++i){const auto& field=info->fields[i];const auto& value=component.fields[i];if(value.index()!=static_cast<std::size_t>(field.type))return Result::Failure("Light field type mismatch");
        auto* address=static_cast<std::byte*>(object)+field.offset;
        switch(field.type){
        case Scene::FieldType::Vector3:*reinterpret_cast<Vec3*>(address)=std::get<Vec3>(value);break;
        case Scene::FieldType::Float:*reinterpret_cast<float*>(address)=std::get<float>(value);break;
        case Scene::FieldType::Unsigned:*reinterpret_cast<std::uint32_t*>(address)=std::get<std::uint32_t>(value);break;
        default:return Result::Failure("Unsupported light field");}
    }return Result::Success();
}
Result RegisterComponents(){
    DirectionalLight sun;PointLight point;SpotLight spot;RectLight rect;DiskLight disk;const void* defaults[]{&sun,&point,&spot,&rect,&disk};Scene::World schema;
    std::size_t index=0;for(const auto& type:LightTypes()){
        bool exists=false;for(const auto& info:schema.Reflection())if(info.type==type.component){if(info.name!=type.name)return Result::Failure("Lighting component identity collision");exists=true;}
        if(!exists){Scene::ModuleComponent component;auto r=Encode(type.component,defaults[index],component);if(!r)return r;r=Scene::RegisterComponent(type.component,type.name,type.fields,component.fields,type.validator);if(!r)return r;}
        auto r=Scene::RegisterRenderLightExtractor(type.component,type.packer);if(!r)return r;++index;
    }return Result::Success();
}
float PointLumensToCandela(float lumens) noexcept{return lumens/(4*std::numbers::pi_v<float>);}
float SpotSolidAngle(float outerHalfAngle) noexcept {float sine=std::sin(outerHalfAngle*.5f);return 4*std::numbers::pi_v<float>*sine*sine;}
float SpotLumensToCandela(float lumens,float outerHalfAngle) noexcept{return lumens/std::max(SpotSolidAngle(outerHalfAngle),1e-8f);}
float PhysicalIntensity(Scene::ComponentType type,float value,std::uint32_t unit,float angle,float area) noexcept{
    if(type==kRectLight||type==kDiskLight){if(unit==1)return value/(std::numbers::pi_v<float>*area);return value*(unit==2?1000.f:1.f);}
    if(unit==2)return value*(type==kDirectionalLight?100000.f:300.f); // Artist value 1: daylight / 300 cd baseline.
    if(unit==1)return type==kSpotLight?SpotLumensToCandela(value,angle):PointLumensToCandela(value);
    return value;
}
float EmittingArea(const Scene::World& world,Guid id,Scene::ComponentType type){Scene::FieldValue a,b;if(type==kRectLight&&world.ReadField(id,type,"width",a)&&world.ReadField(id,type,"height",b))return std::get<float>(a)*std::get<float>(b);if(type==kDiskLight&&world.ReadField(id,type,"radius",a)){float radius=std::get<float>(a);return std::numbers::pi_v<float>*radius*radius;}return 1;}
Vec3 TemperatureTint(float kelvin) noexcept{
    // Three-band Planck tint relative to 6500 K. Approximate color, not a spectral renderer.
    auto band=[&](double wavelength){constexpr double hcOverK=.01438776877;return std::expm1(hcOverK/(wavelength*6500))/std::expm1(hcOverK/(wavelength*std::clamp(kelvin,1000.f,40000.f)));};
    glm::vec3 tint{band(610e-9),band(550e-9),band(460e-9)};tint/=std::max({tint.x,tint.y,tint.z});return {tint.x,tint.y,tint.z};
}
Renderer::RenderLight Pack(Scene::ComponentType type,const CommonLight& common,float intensity,float range,const Mat4& transform,Guid entity,float inner,float outer){
    const auto* definition=FindLightType(type);auto matrix=glm::make_mat4(transform.values.data());auto direction=glm::normalize(glm::vec3(matrix*glm::vec4(0,0,-1,0)));
    Vec3 tint=common.useTemperature?TemperatureTint(common.temperature):Vec3{1,1,1};Vec3 color{common.color.x*tint.x,common.color.y*tint.y,common.color.z*tint.z};
    Renderer::RenderLight light{definition->shaderId,{matrix[3].x,matrix[3].y,matrix[3].z},{direction.x,direction.y,direction.z},color,intensity,range,outer};light.entity=entity;light.modulePacked=true;light.shadow.resolution=common.shadowResolution;light.shadow.filter=common.filterQuality;light.shadow.depthBias=common.depthBias;light.shadow.normalBias=common.normalBias;
    auto& gpu=light.gpu;gpu.position=light.position;gpu.direction=light.direction;gpu.type=definition->shaderId;gpu.invRangeSq=range>0?1/(range*range):0;gpu.color={color.x*intensity,color.y*intensity,color.z*intensity};
    gpu.flags=(common.enabled?Renderer::kLightEnabled:0)|(common.castShadows?Renderer::kLightCastShadows:0);
    if(type==kSpotLight){gpu.spotScale=1/std::max(std::cos(inner)-std::cos(outer),1e-6f);gpu.spotOffset=-std::cos(outer)*gpu.spotScale;}
    return light;
}
}
