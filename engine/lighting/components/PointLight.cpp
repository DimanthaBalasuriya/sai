#include "Schema.hpp"
namespace Engine::Lighting {
namespace {
constexpr Scene::FieldInfo fields[]{COMMON_FIELDS(PointLight),LF(PointLight,intensity,Float,0,100000000),LF(PointLight,range,Float,.001f,1000000),LF(PointLight,sourceRadius,Float,0,1000)};
Renderer::RenderLight Extract(const Scene::ModuleComponent& component,const Mat4& transform,Guid entity){PointLight light;Decode(component,&light);return Pack(kPointLight,light.common,PhysicalIntensity(kPointLight,light.intensity,light.common.unit),light.range,transform,entity);}
}
const LightType& PointType(){static const LightType type{kPointLight,1,"PointLight","EvaluatePoint",fields,Extract};return type;}
}
