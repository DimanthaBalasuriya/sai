#include "Schema.hpp"
#include <cmath>
namespace Engine::Lighting {
namespace {
constexpr Scene::FieldInfo fields[]{COMMON_FIELDS(DirectionalLight),LF(DirectionalLight,illuminance,Float,0,100000000),LF(DirectionalLight,angularDiameter,Float,.001f,10),LF(DirectionalLight,cascadeCount,Unsigned,1,4),LF(DirectionalLight,maxShadowDistance,Float,.1f,100000),LF(DirectionalLight,splitBlend,Float,0,1),LF(DirectionalLight,cascadeBlend,Float,0,.5f)};
Renderer::RenderLight Extract(const Scene::ModuleComponent& component,const Mat4& transform,Guid entity){DirectionalLight light;Decode(component,&light);auto packed=Pack(kDirectionalLight,light.common,PhysicalIntensity(kDirectionalLight,light.illuminance,light.common.unit),0,transform,entity);packed.gpu.spotScale=std::sin(light.angularDiameter*.00872664626f);packed.shadow.cascades=light.cascadeCount;packed.shadow.distance=light.maxShadowDistance;packed.shadow.split=light.splitBlend;packed.shadow.blend=light.cascadeBlend;return packed;}
Result Validate(const Scene::ModuleComponent& component){DirectionalLight light;Decode(component,&light);return light.common.unit==1?Result::Failure("Directional lights use lux or unitless, not lumens"):Result::Success();}
}
const LightType& DirectionalType(){static const LightType type{kDirectionalLight,0,"DirectionalLight","EvaluateDirectional",fields,Extract,Validate};return type;}
}
