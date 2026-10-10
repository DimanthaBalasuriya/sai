#include "Schema.hpp"
namespace Engine::Lighting {
namespace {
constexpr Scene::FieldInfo fields[]{COMMON_FIELDS(SpotLight),LF(SpotLight,intensity,Float,0,100000000),LF(SpotLight,range,Float,.001f,1000000),LF(SpotLight,sourceRadius,Float,0,1000),LF(SpotLight,innerCone,Float,0,1.56f),LF(SpotLight,outerCone,Float,.001f,1.56f)};
Renderer::RenderLight Extract(const Scene::ModuleComponent& component,const Mat4& transform,Guid entity){SpotLight light;Decode(component,&light);return Pack(kSpotLight,light.common,PhysicalIntensity(kSpotLight,light.intensity,light.common.unit,light.outerCone),light.range,transform,entity,light.innerCone,light.outerCone);}
Result Validate(const Scene::ModuleComponent& component){SpotLight light;Decode(component,&light);return light.innerCone<=light.outerCone?Result::Success():Result::Failure("Spot inner cone must not exceed outer cone");}
}
const LightType& SpotType(){static const LightType type{kSpotLight,2,"SpotLight","EvaluateSpot",fields,Extract,Validate};return type;}
}
