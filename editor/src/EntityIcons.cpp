#include "EditorApp.hpp"
#include "engine/lighting/Lighting.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
Engine::Guid EditorApp::DrawEntityIcons(glm::vec2 mouse){
    Engine::Guid hovered;float nearest=1e30f;if(!m_showEntityIcons||!m_viewDrawList||m_play)return hovered;
    if(!m_world.VisibleEntitiesWithoutMesh(m_iconEntities))return hovered;
    auto* draw=m_viewDrawList;draw->PushClipRect({m_viewPosition.x,m_viewPosition.y},{m_viewPosition.x+m_viewSize.x,m_viewPosition.y+m_viewSize.y},true);
    for(auto id:m_iconEntities){if(m_visibility.Hidden(id))continue;Engine::Mat4 transform;if(!m_world.GlobalTransform(id,transform))continue;
        auto global=m_drag.Active()?m_drag.PreviewTransform(id,transform):glm::make_mat4(transform.values.data());auto clip=m_viewProjection*global[3];if(clip.w<=.001f)continue;auto ndc=glm::vec3(clip)/clip.w;if(ndc.z<0||ndc.z>1||std::abs(ndc.x)>1.05||std::abs(ndc.y)>1.05)continue;
        glm::vec2 p=m_viewPosition+glm::vec2(ndc.x*.5f+.5f,.5f-ndc.y*.5f)*m_viewSize;float radius=11*ImGui::GetIO().FontGlobalScale;
        bool selected=m_selection.Contains(id);ImU32 color=selected?IM_COL32(255,204,70,255):IM_COL32(218,231,246,240);ImVec2 c{p.x,p.y};
        draw->AddCircleFilled(c,radius+3,IM_COL32(14,19,28,180),20);if(selected)draw->AddCircle(c,radius+5,color,24,2);
        bool camera=m_world.HasComponent(id,Engine::Scene::ComponentType::Camera),sun=m_world.HasComponent(id,Engine::Lighting::kDirectionalLight),point=m_world.HasComponent(id,Engine::Lighting::kPointLight),spot=m_world.HasComponent(id,Engine::Lighting::kSpotLight);
        if(camera){draw->AddRect({p.x-8,p.y-5},{p.x+4,p.y+5},color,2,0,2);draw->AddTriangle({p.x+4,p.y-3},{p.x+10,p.y-7},{p.x+10,p.y+7},color,2);}
        else if(sun){draw->AddCircle(c,4,color,16,2);for(int k=0;k<8;++k){float a=k*3.14159265f/4;glm::vec2 d{std::cos(a),std::sin(a)};draw->AddLine({p.x+d.x*7,p.y+d.y*7},{p.x+d.x*11,p.y+d.y*11},color,2);}}
        else if(point||m_world.HasComponent(id,Engine::Scene::ComponentType::Light)){draw->AddCircle({p.x,p.y-2},6,color,16,2);draw->AddLine({p.x-3,p.y+5},{p.x+3,p.y+5},color,2);draw->AddLine({p.x-2,p.y+8},{p.x+2,p.y+8},color,2);}
        else if(m_world.HasComponent(id,Engine::Lighting::kRectLight)){draw->AddRect({p.x-9,p.y-5},{p.x+9,p.y+5},color,1,0,2);draw->AddLine({p.x-6,p.y+8},{p.x+6,p.y+8},color,1);}else if(m_world.HasComponent(id,Engine::Lighting::kDiskLight)){draw->AddEllipse(c,{9,6},color,0,20,2);draw->AddLine({p.x,p.y+6},{p.x,p.y+10},color,1);}else if(spot){draw->AddTriangle({p.x-3,p.y-8},{p.x-9,p.y+8},{p.x+9,p.y+8},color,2);draw->AddLine({p.x-3,p.y-8},{p.x+3,p.y-8},color,3);}
        else{draw->AddQuad({p.x,p.y-9},{p.x+9,p.y},{p.x,p.y+9},{p.x-9,p.y},color,2);draw->AddLine({p.x-4,p.y},{p.x+4,p.y},color,1);draw->AddLine({p.x,p.y-4},{p.x,p.y+4},color,1);}
        if(glm::length(mouse-p)<=radius+5&&clip.w<nearest){nearest=clip.w;hovered=id;}
    }
    if(m_showLightBounds&&m_selected){
        Engine::Mat4 transform;if(m_world.GlobalTransform(m_selected,transform)){auto matrix=glm::make_mat4(transform.values.data());glm::vec3 origin(matrix[3]),right=glm::normalize(glm::vec3(matrix[0])),up=glm::normalize(glm::vec3(matrix[1])),direction=-glm::normalize(glm::vec3(matrix[2]));
            auto project=[&](glm::vec3 p,glm::vec2& out){auto c=m_viewProjection*glm::vec4(p,1);if(c.w<=.001)return false;out=m_viewPosition+glm::vec2(c.x/c.w*.5f+.5f,.5f-c.y/c.w*.5f)*m_viewSize;return true;};
            auto line=[&](glm::vec3 a,glm::vec3 b){glm::vec2 x,y;if(project(a,x)&&project(b,y))draw->AddLine({x.x,x.y},{y.x,y.y},IM_COL32(255,205,80,160),1);};
            auto handle=[&](glm::vec3 point,Engine::Scene::ComponentType type,const char* field,float value,float scale){glm::vec2 p,c;if(!project(point,p)||!project(origin,c))return;bool hover=glm::length(mouse-p)<9;draw->AddCircleFilled({p.x,p.y},5,hover?IM_COL32(255,255,255,255):IM_COL32(255,205,80,240));if(hover&&!m_drag.Active()&&m_viewHovered&&ImGui::IsMouseClicked(0)){m_lightHandleActive=true;m_lightHandleMerged=false;m_lightHandleType=type;m_lightHandleField=field;m_lightHandleEntity=m_selected;m_lightHandleStart=mouse;m_lightHandleAxis=glm::normalize(p-c);m_lightHandleScale=std::max(1.f,glm::length(p-c))/std::max(scale,.001f);m_lightHandleValue=value;m_selecting=false;}};
            for(auto& type:Engine::Lighting::LightTypes())if(m_world.HasComponent(m_selected,type.component)){
                if(type.shaderId==0){line(origin,origin+direction*3.f);line(origin+direction*3.f,origin+direction*2.5f+right*.25f);continue;}
                Engine::Scene::FieldValue value;m_world.ReadField(m_selected,type.component,"range",value);float range=std::get<float>(value);
                for(int plane=0;plane<3;++plane)for(int segment=0;segment<48;++segment){float a=segment*6.2831853f/48,b=(segment+1)*6.2831853f/48;glm::vec3 u=plane==0?glm::vec3(1,0,0):plane==1?glm::vec3(0,1,0):glm::vec3(0,0,1),v=plane==0?glm::vec3(0,1,0):plane==1?glm::vec3(0,0,1):glm::vec3(1,0,0);line(origin+range*(u*std::cos(a)+v*std::sin(a)),origin+range*(u*std::cos(b)+v*std::sin(b)));}handle(origin+right*range,type.component,"range",range,range);
                if(type.shaderId==2){m_world.ReadField(m_selected,type.component,"outerCone",value);float angle=std::get<float>(value),radius=range*std::tan(angle);auto center=origin+direction*range;for(int i=0;i<48;++i){float a=i*6.2831853f/48,b=(i+1)*6.2831853f/48;auto p=center+radius*(right*std::cos(a)+up*std::sin(a)),q=center+radius*(right*std::cos(b)+up*std::sin(b));line(p,q);if(i%12==0)line(origin,p);}handle(center+right*radius,type.component,"outerCone",angle,angle);}
                break;
            }
        }
    }
    if(m_lightHandleActive){if(!ImGui::IsMouseDown(0))m_lightHandleActive=false;else{float value=m_lightHandleValue+glm::dot(mouse-m_lightHandleStart,m_lightHandleAxis)/m_lightHandleScale;value=std::clamp(value,.001f,m_lightHandleField=="outerCone"?1.55f:1000000.f);if(glm::length(mouse-m_lightHandleStart)>1){auto r=m_history.Execute(m_world,std::make_unique<Engine::Scene::SetFieldCommand>(m_lightHandleEntity,m_lightHandleType,m_lightHandleField,value),m_lightHandleMerged);if(r)m_lightHandleMerged=true;}}}
    draw->PopClipRect();if(hovered&&m_viewHovered){Engine::Scene::FieldValue name;if(m_world.ReadField(hovered,Engine::Scene::ComponentType::Name,"value",name))ImGui::SetTooltip("%s",std::get<std::string>(name).c_str());}return hovered;
}
