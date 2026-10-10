#pragma once
#include <glm/glm.hpp>
#include <cmath>
namespace Engine::Renderer {
// Vulkan zero-to-one, finite reverse Z: near maps to one, far to zero.
inline glm::mat4 PerspectiveReverseZ(float fov,float aspect,float nearPlane,float farPlane) {
    glm::mat4 p(0);const float f=1/std::tan(fov*.5f);
    p[0][0]=f/aspect;p[1][1]=-f;p[2][2]=nearPlane/(farPlane-nearPlane);
    p[2][3]=-1;p[3][2]=nearPlane*farPlane/(farPlane-nearPlane);return p;
}
}
