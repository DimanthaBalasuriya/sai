#include "engine/sky/Systems.hpp"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
namespace Engine::Sky {
namespace {
constexpr double pi=std::numbers::pi;
Vec3 Solar(const TimeOfDay& t,double hour,double declination){double lat=t.latitude*pi/180,angle=((hour-12)*15+t.longitude)*pi/180;glm::dvec3 d{-std::cos(declination)*std::sin(angle),std::sin(lat)*std::sin(declination)+std::cos(lat)*std::cos(declination)*std::cos(angle),std::cos(lat)*std::sin(declination)-std::sin(lat)*std::cos(declination)*std::cos(angle)};return {float(d.x),float(d.y),float(d.z)};}
Vec3 Manual(Vec3 v){glm::vec3 d{v.x,v.y,v.z};float length=glm::length(d);return length>1e-8f?Vec3{d.x/length,d.y/length,d.z/length}:Vec3{0,1,0};}
float Smooth(float lo,float hi,float v){float t=std::clamp((v-lo)/(hi-lo),0.f,1.f);return t*t*(3-2*t);}
}
Result AdvanceTime(TimeOfDay& t,double seconds){if(!std::isfinite(seconds)||seconds<0||!std::isfinite(t.dayLength)||t.dayLength<=0||!std::isfinite(t.time)||!std::isfinite(t.speed))return Result::Failure("Invalid time-of-day update");if(t.paused)return Result::Success();double total=t.time+seconds*24*t.speed/t.dayLength;double days=std::floor(total/24);if(!std::isfinite(total)||std::abs(t.day+days)>1000000)return Result::Failure("Time-of-day range exceeded");t.time=float(total-days*24);t.day+=float(days);return Result::Success();}
CelestialState EvaluateCelestials(const TimeOfDay& t,const CelestialBody& sun,const CelestialBody& moon){double declination=glm::radians(23.44)*std::sin(2*pi*(t.dayOfYear-81)/365.2422);double phase=std::fmod((t.day+t.time/24)/29.53059+.5,1.0);if(phase<0)phase+=1;CelestialState state;state.sun=sun.directionSource?Manual(sun.direction):Solar(t,t.time,declination);state.moon=moon.directionSource?Manual(moon.direction):Solar(t,t.time+phase*24,declination*.9);state.moonPhase=moon.directionSource?moon.phase:float(phase);state.daylight=Smooth(-.08f,.08f,state.sun.y);state.starsVisibility=1-Smooth(-.18f,-.015f,state.sun.y);return state;}
}
