#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
namespace EditorInput {
glm::vec3 Camera::Eye() const {return focus+glm::vec3(std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch))*distance;}
glm::mat4 Camera::View() const {return glm::lookAt(Eye(),focus,glm::vec3(0,1,0));}
void Camera::Focus(glm::vec3 point,float radius) {m_from=focus;m_to=point;m_fromDistance=distance;m_toDistance=std::max(radius*2.6f,.5f);m_focusTime=0;}
void Camera::Reset(const Engine::Renderer::RenderCamera& camera) {if(!camera.valid)return;auto matrix=glm::make_mat4(camera.transform.values.data());auto backward=glm::normalize(glm::vec3(matrix[2]));distance=std::clamp(glm::length(glm::vec3(matrix[3])),2.f,10000.f);pitch=std::asin(std::clamp(backward.y,-.99f,.99f));yaw=std::atan2(backward.x,backward.z);focus=glm::vec3(matrix[3])-backward*distance;m_focusTime=.25f;m_velocity={0,0,0};}
Engine::Renderer::RenderCamera Camera::Extract() const {Engine::Renderer::RenderCamera output;auto matrix=glm::inverse(View());std::copy_n(glm::value_ptr(matrix),16,output.transform.values.begin());output.valid=true;output.farPlane=10000;return output;}
void Camera::Update(const Actions& a,const Frame& f,float height) {
    const float dt=std::clamp(f.delta,0.f,.05f);auto inverse=glm::inverse(View());auto right=glm::vec3(inverse[0]),up=glm::vec3(inverse[1]),forward=-glm::vec3(inverse[2]);
    if(a.Active("Camera.Speed")) speedLog=std::clamp(speedLog+f.wheel*.22f,-3.f,9.f);
    bool flying=a.Active("Camera.Look");
    if(a.Active("Camera.LookDelta")) {auto eye=Eye();yaw-=f.dx*.004f;pitch=std::clamp(pitch+f.dy*.004f,-1.55f,1.55f);focus=eye-glm::vec3(std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch))*distance;m_focusTime=.25f;}
    glm::vec3 move(0);
    if(flying) {move+=forward*float(a.Active("Camera.FlyForward")-a.Active("Camera.FlyBack"));move+=right*float(a.Active("Camera.FlyRight")-a.Active("Camera.FlyLeft"));move+=glm::vec3(0,1,0)*float(a.Active("Camera.FlyUp")-a.Active("Camera.FlyDown"));}
    if(glm::length(move)>0) move=glm::normalize(move)*std::exp(speedLog)*(a.Active("Camera.Boost")?4.f:1.f);
    m_velocity=glm::mix(m_velocity,move,1-std::exp(-dt*12));if(glm::length(m_velocity)<.001f)m_velocity={0,0,0};focus+=m_velocity*dt;
    if(a.Active("Camera.Orbit")) {yaw-=f.dx*.005f;pitch=std::clamp(pitch+f.dy*.005f,-1.55f,1.55f);m_focusTime=.25f;}
    if(a.Active("Camera.Pan")) {focus+=(-right*f.dx+up*f.dy)*(2*distance*std::tan(.5236f)/std::max(height,1.f));m_focusTime=.25f;}
    if(Focusing()) {m_focusTime=std::min(.25f,m_focusTime+dt);float t=m_focusTime/.25f;t=t*t*(3-2*t);focus=glm::mix(m_from,m_to,t);distance=std::lerp(m_fromDistance,m_toDistance,t);}
}
}
