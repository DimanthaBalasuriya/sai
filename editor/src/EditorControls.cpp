#include "EditorApp.hpp"
#include "engine/core/Log.hpp"
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>
#include <cstdio>
namespace {
glm::vec3 PointUnderCursor(const EditorTools::Geometry& geometry,const Engine::Renderer::RenderWorld& world,const EditorTools::Ray& ray,glm::vec3 fallback) {
    auto hit=geometry.Pick(world,ray);if(hit)return hit.point;if(std::abs(ray.direction.y)>.00001f) {float t=-ray.origin.y/ray.direction.y;if(t>0)return ray.origin+ray.direction*t;}return fallback;
}
}
void EditorApp::CancelInteraction() {if(m_drag.Active()||m_selecting)++m_previewRevision;m_drag.Cancel();m_selecting=m_box=false;m_blockGizmo=true;m_blockLook=true;ImGuizmo::Enable(false);}
void EditorApp::FocusSelection() {
    auto expanded=EditorTools::Expand(m_world,m_selection.ids);Engine::Renderer::RenderWorld world;if(!m_world.Extract(world))return;auto bounds=m_geometry.SelectionBounds(world,expanded);
    if(bounds.valid)m_camera.Focus((bounds.minimum+bounds.maximum)*.5f,std::max(glm::length(bounds.maximum-bounds.minimum)*.5f,.2f));
    else if(m_selected) {Engine::Mat4 global;if(m_world.GlobalTransform(m_selected,global))m_camera.Focus({global.values[12],global.values[13],global.values[14]},1);}
}
void EditorApp::TogglePlay() {
    CancelInteraction();
    if(!m_play) {auto result=EditorTools::Capture(m_world,m_playDefinitions);if(!result){Report(result);return;}m_editHidden=m_visibility.hidden;m_editSelection=m_selection.ids;m_editHistory=std::move(m_history);m_history=Engine::Scene::CommandHistory{};m_play=true;Engine::Log::Success("Play preview started; stop restores the edit scene");}
    else {Engine::Scene::ReplaceSceneCommand restore(m_playDefinitions);auto result=restore.Do(m_world);if(!result){Report(result);return;}m_playDefinitions.clear();m_history=std::move(m_editHistory);m_visibility.hidden=std::move(m_editHidden);m_selection.ids=std::move(m_editSelection);++m_visibility.revision;m_play=false;Engine::Log::Success("Play preview stopped; edit scene restored");}
}
void EditorApp::ProcessInput() {
    m_inputFrame=m_inputBridge.Capture(m_flying);if(m_inputSmoke)m_inputFrame.focused=true;
    if(!m_inputFrame.focused) {CancelInteraction();m_flying=false;m_inputBridge.CapturePointer(false);}
    bool look=m_actions.Requested(m_inputFrame,"Camera.Look");if(!look)m_blockLook=false;
    bool flyingContext=m_flying||(!m_blockLook&&m_viewHovered&&!m_drag.Active()&&!m_selecting&&look&&!m_inputFrame.textInput);
    std::array<bool,static_cast<std::size_t>(EditorInput::Context::Count)> contexts{};
    auto enable=[&](EditorInput::Context c,bool state){contexts[static_cast<std::size_t>(c)]=state;};
    enable(EditorInput::Context::Global,true);enable(EditorInput::Context::Viewport,m_viewHovered||m_viewFocused||m_selecting||m_orbiting||m_panning);
    enable(EditorInput::Context::Hierarchy,m_hierarchyHovered||m_hierarchyFocused);
    enable(EditorInput::Context::ViewportFlying,flyingContext);enable(EditorInput::Context::Interaction,m_drag.Active()||m_selecting||m_blockGizmo);
    enable(EditorInput::Context::TextInput,m_inputFrame.textInput);m_actions.Route(m_inputFrame,contexts);
    auto action=[&](const char* name){return m_actions.Active(name);};
    if(action("Text.Cancel")) {m_renameEntity={};m_renameCommit=m_renameStarted=false;}
    if(m_renameCommit&&m_renameEntity) {Engine::Scene::FieldValue before;if(m_world.ReadField(m_renameEntity,Engine::Scene::ComponentType::Name,"value",before)&&std::get<std::string>(before)!=m_renameName.data())Report(m_history.Execute(m_world,std::make_unique<Engine::Scene::SetFieldCommand>(m_renameEntity,Engine::Scene::ComponentType::Name,"value",std::string(m_renameName.data()))));m_renameEntity={};m_renameCommit=m_renameStarted=false;}
    if(action("Edit.Cancel")) {if(m_renameEntity){m_renameEntity={};m_renameCommit=m_renameStarted=false;}else if(m_drag.Active()||m_selecting)CancelInteraction();else {m_selection.ids.clear();m_selected={};}m_flying=false;m_inputBridge.CapturePointer(false);}
    if(action("Selection.HierarchyClick")&&m_hierarchyClick) {CancelInteraction();m_selection.Click(m_hierarchyClick,action("Selection.AddMode"),action("Selection.ToggleMode"));m_selected=m_selection.Primary();}
    if(action("Camera.FocusClicked")&&m_hierarchyClick) {m_selection.Click(m_hierarchyClick,false,false);m_selected=m_selection.Primary();FocusSelection();}m_hierarchyClick={};
    if(action("Selection.All")) {CancelInteraction();m_world.Entities(m_selection.ids);}
    if(action("Edit.Undo")||action("Edit.Redo")) {CancelInteraction();if(action("Edit.Undo")&&m_history.CanUndo())Report(m_history.Undo(m_world));if(action("Edit.Redo")&&m_history.CanRedo())Report(m_history.Redo(m_world));}
    if(action("Edit.Copy"))Report(EditorTools::Copy(m_world,m_selection.ids,m_clipboard));
    if(action("Edit.Duplicate")) {CancelInteraction();std::vector<Engine::Scene::EntityDefinition> clipboard;auto result=EditorTools::Copy(m_world,m_selection.ids,clipboard);if(result)result=EditorTools::Paste(m_world,m_history,clipboard,m_selection.ids);Report(result);}
    if(action("Edit.Paste")) {CancelInteraction();Report(EditorTools::Paste(m_world,m_history,m_clipboard,m_selection.ids));}
    if(action("Edit.Delete")) {CancelInteraction();Report(EditorTools::Delete(m_world,m_history,m_selection.ids));m_selection.Prune(m_world);}
    if(action("Edit.Hide")) {CancelInteraction();Report(m_visibility.Toggle(m_world,m_history,m_selection.ids));}
    if(action("Edit.Rename")&&m_selection.Primary()) {m_renameEntity=m_selection.Primary();Engine::Scene::FieldValue name;if(m_world.ReadField(m_renameEntity,Engine::Scene::ComponentType::Name,"value",name))std::snprintf(m_renameName.data(),m_renameName.size(),"%s",std::get<std::string>(name).c_str());m_renameStarted=m_renameCommit=false;m_focusPanel="Hierarchy";}
    if(action("Scene.Save"))Save();if(action("Scene.Play"))TogglePlay();if(action("View.Cycle"))m_view=(m_view+1)%13;
    const char* tools[]{"Tool.Select","Tool.Move","Tool.Rotate","Tool.Scale"};for(int i=0;i<4;++i)if(action(tools[i])) {CancelInteraction();m_operation=i-1;}
    if(action("Tool.Axes")) {CancelInteraction();m_mode=1-m_mode;}if(action("Tool.Pivot")) {CancelInteraction();m_pivotMode=1-m_pivotMode;}
    m_selection.Prune(m_world);m_selected=m_selection.Primary();
    if(m_blockGizmo&&!m_actions.Requested(m_inputFrame,"Pointer.Held")) {m_blockGizmo=false;ImGuizmo::Enable(true);}
    bool flying=action("Camera.Look")&&!m_blockLook&&!m_drag.Active()&&!m_selecting&&!m_inputFrame.textInput&&m_inputFrame.focused&&(m_flying||m_viewHovered);
    if(flying!=m_flying) {m_flying=flying;m_inputBridge.CapturePointer(flying);}
    if(action("Camera.Focus"))FocusSelection();
    if(m_viewSize.x<16||m_viewSize.y<16) {CancelInteraction();m_flying=false;m_inputBridge.CapturePointer(false);return;}
    auto projection=glm::perspective(glm::radians(60.f),m_viewSize.x/m_viewSize.y,.1f,10000.f);
    m_viewProjection=projection*m_camera.View();glm::vec2 mouse{m_inputFrame.x,m_inputFrame.y};auto normalized=(mouse-m_viewPosition)/m_viewSize;
    auto& render=m_inputSnapshot;m_world.Extract(render);m_visibility.Filter(m_world,render);auto ray=EditorTools::ScreenRay(glm::inverse(m_viewProjection),normalized);
    bool orbit=action("Camera.Orbit")&&(m_viewHovered||m_orbiting)&&!m_drag.Active();bool pan=action("Camera.Pan")&&(m_viewHovered||m_panning)&&!m_drag.Active();
    if(orbit&&!m_orbiting) {auto eye=m_camera.Eye();auto expanded=EditorTools::Expand(m_world,m_selection.ids);auto bounds=m_geometry.SelectionBounds(render,expanded);auto pivot=bounds.valid?(bounds.minimum+bounds.maximum)*.5f:PointUnderCursor(m_geometry,render,ray,m_camera.focus);auto backward=eye-pivot;float length=glm::length(backward);if(length>.001f){m_camera.focus=pivot;m_camera.distance=length;backward/=length;m_camera.yaw=std::atan2(backward.x,backward.z);m_camera.pitch=std::asin(std::clamp(backward.y,-.99f,.99f));}}
    if(action("Camera.Zoom")&&m_viewHovered&&!m_drag.Active()) {auto pivot=PointUnderCursor(m_geometry,render,ray,m_camera.focus);float old=m_camera.distance;m_camera.distance=std::clamp(old*std::exp(-m_inputFrame.wheel*.12f),.2f,10000.f);float ratio=m_camera.distance/old;m_camera.focus=pivot+(m_camera.focus-pivot)*ratio;}
    EditorInput::Actions idle;
    m_camera.Update((m_flying||orbit||pan||m_viewHovered)?m_actions:idle,m_inputFrame,m_viewSize.y);m_orbiting=orbit;m_panning=pan;
    m_viewProjection=projection*m_camera.View();m_editorCamera=m_camera.Extract();
    DrawGizmo();
    auto iconHit=DrawEntityIcons(mouse);
    bool gizmoHovered=m_operation>=0&&!m_selection.ids.empty()&&!m_inputFrame.textInput&&ImGuizmo::IsOver();
    if(action("Selection.Begin")&&m_viewHovered&&!m_flying&&!orbit&&!pan&&!m_drag.Active()&&!m_blockGizmo&&!gizmoHovered&&!m_lightHandleActive) {
        m_selectStart=mouse;m_selecting=true;m_box=false;m_addSelection=action("Selection.AddMode");m_toggleSelection=action("Selection.ToggleMode");
        auto hit=m_geometry.Pick(render,EditorTools::ScreenRay(glm::inverse(m_viewProjection),normalized));m_pressHit=iconHit?iconHit:hit.id;
    }
    if(m_selecting) {
        m_box|=glm::length(mouse-m_selectStart)>=4.f;
        if(m_box&&!m_pressHit&&m_viewDrawList) {auto lo=glm::min(mouse,m_selectStart),hi=glm::max(mouse,m_selectStart);m_viewDrawList->PushClipRect({m_viewPosition.x,m_viewPosition.y},{m_viewPosition.x+m_viewSize.x,m_viewPosition.y+m_viewSize.y},true);auto color=m_widgets.Token("info");color.w=.15f;m_viewDrawList->AddRectFilled({lo.x,lo.y},{hi.x,hi.y},ImGui::GetColorU32(color));m_viewDrawList->AddRect({lo.x,lo.y},{hi.x,hi.y},ImGui::GetColorU32(m_widgets.Token("info")));m_viewDrawList->PopClipRect();}
        if(action("Selection.Click")) {if(m_box&&!m_pressHit) {auto lo=(glm::min(mouse,m_selectStart)-m_viewPosition)/m_viewSize,hi=(glm::max(mouse,m_selectStart)-m_viewPosition)/m_viewSize;auto selected=m_geometry.Box(render,m_viewProjection,lo,hi);if(m_showEntityIcons)for(auto id:m_iconEntities){if(m_visibility.Hidden(id))continue;Engine::Mat4 transform;if(!m_world.GlobalTransform(id,transform))continue;auto clip=m_viewProjection*glm::make_mat4(transform.values.data())[3];if(clip.w<=0||clip.z<0||clip.z>clip.w)continue;glm::vec2 p{clip.x/clip.w*.5f+.5f,.5f-clip.y/clip.w*.5f};if(p.x>=lo.x&&p.x<=hi.x&&p.y>=lo.y&&p.y<=hi.y)selected.push_back(id);}if(!m_addSelection&&!m_toggleSelection)m_selection.ids.clear();for(auto id:selected)m_selection.Click(id,true,m_toggleSelection);}else if(!m_box)m_selection.Click(m_pressHit,m_addSelection,m_toggleSelection);m_selecting=m_box=false;}
    }
    m_selected=m_selection.Primary();
}
void EditorApp::DrawGizmo() {
    if(!m_viewDrawList||m_operation<0||m_selection.ids.empty()||m_flying||m_orbiting||m_panning||m_blockGizmo||m_inputFrame.textInput)return;
    glm::mat4 pivot(1);
    if(m_drag.Active())pivot=m_drag.Pivot();
    else {Engine::Mat4 global;if(!m_world.GlobalTransform(m_selected,global))return;auto primary=glm::make_mat4(global.values.data());pivot=primary;
        // Strip scale from the gizmo basis so delta is applied to stored transforms.
        for(int i=0;i<3;++i)pivot[i]=glm::vec4(glm::normalize(glm::vec3(primary[i])),0);
        if(m_pivotMode==0) {Engine::Renderer::RenderWorld render;m_world.Extract(render);auto expanded=EditorTools::Expand(m_world,m_selection.ids);auto bounds=m_geometry.SelectionBounds(render,expanded);if(bounds.valid)pivot[3]=glm::vec4((bounds.minimum+bounds.maximum)*.5f,1);}
    }
    auto before=pivot;auto view=m_camera.View(),projection=glm::perspective(glm::radians(60.f),m_viewSize.x/m_viewSize.y,.1f,10000.f);
    ImGuizmo::SetOrthographic(false);ImGuizmo::SetDrawlist(m_viewDrawList);ImGuizmo::SetRect(m_viewPosition.x,m_viewPosition.y,m_viewSize.x,m_viewSize.y);
    auto operation=m_operation==0?ImGuizmo::TRANSLATE:m_operation==1?ImGuizmo::ROTATE:ImGuizmo::SCALE;bool snap=m_actions.Requested(m_inputFrame,"Tool.Snap");float amount=m_operation==0?m_gridSnap:m_operation==1?m_angleSnap:m_scaleSnap;float increments[]{amount,amount,amount};
    bool changed=ImGuizmo::Manipulate(glm::value_ptr(view),glm::value_ptr(projection),operation,m_mode==0?ImGuizmo::WORLD:ImGuizmo::LOCAL,glm::value_ptr(pivot),nullptr,snap?increments:nullptr);
    if(ImGuizmo::IsUsing()&&!m_drag.Active()) {Report(m_drag.Begin(m_world,m_selection.ids,before,m_pivotMode==1));m_selecting=false;}
    if(changed&&m_drag.Active()) {
        if(snap&&m_operation==0&&m_viewHovered) {auto& render=m_inputSnapshot;m_world.Extract(render);m_visibility.Filter(m_world,render);auto ray=EditorTools::ScreenRay(glm::inverse(m_viewProjection),(glm::vec2(m_inputFrame.x,m_inputFrame.y)-m_viewPosition)/m_viewSize);auto hit=m_geometry.Pick(render,ray,m_drag.Affected());if(hit)pivot[3]=glm::vec4(hit.point,1);}
        auto result=m_drag.Update(pivot);if(result)++m_previewRevision;else Engine::Log::Warning(result.Error());
    }
    if(m_drag.Active()&&!ImGuizmo::IsUsing()) {Report(m_drag.Commit(m_world,m_history));++m_previewRevision;}
}
void EditorApp::ApplyPreview() {m_drag.Preview(m_snapshot);m_visibility.Filter(m_world,m_snapshot,&m_drag);if(m_drag.Active())m_snapshot.revision+=m_previewRevision;}

