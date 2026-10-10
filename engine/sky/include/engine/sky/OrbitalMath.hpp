#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
namespace Engine::Sky {
// Atmosphere lengths use meters consistently on CPU and GPU.
template<class T> glm::vec<2,T> StableSphere(glm::vec<3,T> origin,glm::vec<3,T> direction,T radius){T b=glm::dot(origin,direction),length=glm::length(origin),c=(length-radius)*(length+radius);auto perpendicular=glm::cross(origin,direction);T d=std::abs(c)<radius*radius*T(.01)?std::fma(b,b,-c):radius*radius-glm::dot(perpendicular,perpendicular);if(d<0)return {-1,-1};T root=std::sqrt(d),q=-b-std::copysign(root,b);if(std::abs(q)<T(1e-10))return {-b,-b};T a=q,z=c/q;return {std::min(a,z),std::max(a,z)};}
inline float SpaceBlend(double altitude){double t=std::clamp((altitude-50000)/70000,0.0,1.0);return float(t*t*(3-2*t));}
}
