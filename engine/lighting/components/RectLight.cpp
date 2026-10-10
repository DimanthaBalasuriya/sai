#include "Schema.hpp"
#include <glm/gtc/type_ptr.hpp>
namespace Engine::Lighting {
namespace {
constexpr Scene::FieldInfo fields[]{COMMON_FIELDS(RectLight),LF(RectLight,intensity,Float,0,100000000),LF(RectLight,range,Float,.001f,1000000),LF(RectLight,width,Float,.001f,1000),LF(RectLight,height,Float,.001f,1000)};
Renderer::RenderLight Extract(const Scene::ModuleComponent& c,const Mat4& t,Guid id){RectLight light;Decode(c,&light);float radiance=light.common.unit==1?light.intensity/(3.14159265f*light.width*light.height):light.intensity*(light.common.unit==2?1000.f:1.f);auto out=Pack(kRectLight,light.common,radiance,light.range,t,id);auto m=glm::make_mat4(t.values.data());auto right=glm::normalize(glm::vec3(m[0])),up=glm::normalize(glm::vec3(m[1]));out.right={right.x,right.y,right.z};out.up={up.x,up.y,up.z};out.width=light.width;out.height=light.height;return out;}
}
const LightType& RectType(){static const LightType type{kRectLight,3,"RectLight","EvaluateArea",fields,Extract};return type;}
}
