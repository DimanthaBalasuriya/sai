#pragma once
#include "Input.h"
#include "engine/renderer/RenderWorld.hpp"
#include <glm/glm.hpp>
namespace EditorInput {
class Camera {
public:
    glm::vec3 focus{0};float distance=12,yaw=0,pitch=.2754f,speedLog=1.6f;
    void Update(const Actions&,const Frame&,float height);
    void Focus(glm::vec3 point,float radius);
    void Reset(const Engine::Renderer::RenderCamera&);
    glm::vec3 Eye() const;
    glm::mat4 View() const;
    Engine::Renderer::RenderCamera Extract() const;
    bool Focusing() const {return m_focusTime<.25f;}
private:
    glm::vec3 m_velocity{0},m_from{0},m_to{0};float m_focusTime=.25f,m_fromDistance=12,m_toDistance=12;
};
}
