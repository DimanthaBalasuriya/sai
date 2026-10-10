#include "Schema.hpp"
#include <glm/gtc/type_ptr.hpp>
namespace Engine::Lighting {
namespace {
constexpr Scene::FieldInfo fields[]{COMMON_FIELDS(DiskLight),LF(DiskLight,intensity,Float,0,100000000),LF(DiskLight,range,Float,.001f,1000000),LF(DiskLight,radius,Float,.001f,1000)};
Renderer::RenderLight Extract(const Scene::ModuleComponent& c,const Mat4& t,Guid id){DiskLight light;Decode(c,&light);float radiance=light.common.unit==1?light.intensity/(9.8696044f*light.radius*light.radius):light.intensity*(light.common.unit==2?1000.f:1.f);auto out=Pack(kDiskLight,light.common,radiance,light.range,t,id);auto m=glm::make_mat4(t.values.data());auto right=glm::normalize(glm::vec3(m[0])),up=glm::normalize(glm::vec3(m[1]));out.right={right.x,right.y,right.z};out.up={up.x,up.y,up.z};out.width=out.height=light.radius*2;return out;}
}
const LightType& DiskType(){static const LightType type{kDiskLight,4,"DiskLight","EvaluateArea",fields,Extract};return type;}
}
