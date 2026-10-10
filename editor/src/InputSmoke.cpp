#include "EditorApp.hpp"
#include "engine/core/Log.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
// Integration checks use the normal ImGui event queue and action bindings.
void EditorApp::InjectInputSmoke(int frame) {
    if(frame<180)return;
    if(m_viewSize.x>16)ImGui::GetIO().AddMousePosEvent(m_viewPosition.x+m_viewSize.x*.85f,m_viewPosition.y+m_viewSize.y*.85f);
    struct Step {int frame;const char* action;};
    constexpr Step steps[]{{184,"Tool.Select"},{188,"Tool.Move"},{192,"Tool.Rotate"},{196,"Tool.Scale"},{200,"Tool.Axes"},{204,"Tool.Pivot"},{208,"Edit.Duplicate"},{212,"Edit.Undo"},{216,"Edit.Redo"},{220,"Edit.Copy"},{224,"Edit.Paste"},{228,"Edit.Delete"},{232,"Edit.Undo"},{236,"Edit.Hide"},{240,"Edit.Undo"},{244,"View.Cycle"},{248,"Scene.Play"},{252,"Edit.Delete"},{256,"Scene.Play"},{260,"Edit.Rename"},{264,"Edit.Duplicate"},{268,"Text.Cancel"},{272,"Tool.Select"},{278,"Edit.Cancel"},{292,"Selection.All"},{296,"Edit.Cancel"},{300,"Scene.Save"}};
    for(auto step:steps)if(frame==step.frame||frame==step.frame+1) {
        // The first configured binding is sufficient; alternate redo chords are
        // covered by the router tests. This also tests rebinding on restart.
        for(const auto& binding:m_actions.Bindings())if(binding.action==step.action) {m_inputBridge.Inject(EditorInput::FormatChord(binding.chord),frame==step.frame);break;}
    }
    if(frame==286||frame==287)m_inputBridge.Inject("RMB",frame==286);
    if(frame>=308&&frame<=318) {
        EditorInput::Frame pointer;pointer.x=m_viewPosition.x+m_viewSize.x*.85f;pointer.y=m_viewPosition.y+m_viewSize.y*.85f;
        pointer.keys["RMB"]={frame<316,frame==308,frame==316};pointer.dy=frame>308&&frame<316?1.f:0;pointer.wheel=frame==314?2.f:0;m_inputBridge.OverridePointerForSmoke(std::move(pointer));
    }
    if(frame==310||frame==312)m_inputBridge.Inject("E",frame==310);
    if(frame==320||frame==321)m_inputBridge.Inject("E",frame==320);
    if(frame==326||frame==327)m_inputBridge.Inject(m_actions.Shortcut("Edit.Rename"),frame==326);
    if(frame==330)ImGui::GetIO().AddInputCharactersUTF8("F2 rename verified");
    if(frame==332||frame==333)m_inputBridge.Inject("Enter",frame==332);
    if(frame==336||frame==337)m_inputBridge.Inject(m_actions.Shortcut("Edit.Undo"),frame==336);
}
Engine::Result EditorApp::InputSmokeStep(int frame) {
    auto fail=[&](const char* message){return Engine::Result::Failure("F2 input smoke frame "+std::to_string(frame)+": "+message);};
    std::vector<Engine::Guid> ids;m_world.Entities(ids);
    if(frame==180) {
        for(auto id:ids)if(m_world.HasComponent(id,Engine::Scene::ComponentType::MeshRenderer)){m_selection.Click(id,false,false);m_selected=id;break;}
        m_inputSmokeOriginal=ids;m_inputSmokeSelected=m_selected;m_inputSmokeCount=ids.size();m_inputSmokeBranch=EditorTools::Expand(m_world,m_selection.ids).size();m_focusPanel="Viewport";m_view=0;m_mode=m_pivotMode=0;
    }
    if(frame==186&&m_operation!=-1)return fail("Q select");if(frame==190&&m_operation!=0)return fail("W move");if(frame==194&&m_operation!=1)return fail("E rotate");if(frame==198&&m_operation!=2)return fail("R scale");
    if(frame==202&&m_mode!=1)return fail("X axes");if(frame==206&&m_pivotMode!=1)return fail("Z pivot");
    auto count=m_inputSmokeCount,branch=m_inputSmokeBranch;
    if((frame==210||frame==218||frame==230)&&ids.size()!=count+branch)return fail("duplicate/redo/delete count");
    if(frame==214&&ids.size()!=count)return fail("duplicate undo");
    if(frame==218||frame==234)for(auto id:ids)if(std::find(m_inputSmokeOriginal.begin(),m_inputSmokeOriginal.end(),id)==m_inputSmokeOriginal.end()) {Engine::Scene::EntityDefinition e;m_world.Definition(id,e);if(!e.hierarchy.parent){m_selection.Click(id,false,false);m_selected=id;break;}}
    if((frame==226||frame==234||frame==258||frame==266)&&ids.size()!=count+2*branch)return fail("paste/delete undo/play restore/text capture count");
    if(frame==238&&!m_visibility.Hidden(m_selected))return fail("H hide");if(frame==242&&m_visibility.Hidden(m_selected))return fail("hide undo");
    if(frame==246&&m_view!=1)return fail("F3 debug view");if(frame==250&&!m_play)return fail("play starts");if(frame==254&&ids.size()!=count+branch)return fail("play edit is isolated");if(frame==258&&m_play)return fail("play stops");
    if(frame==262&&!m_renameEntity)return fail("F2 rename");if(frame==266&&!m_inputFrame.textInput)return fail("rename input capture");if(frame==270&&m_renameEntity)return fail("Escape cancels rename");
    if(frame==276||frame==284) {
        m_history.Clear();m_selection.Click(m_inputSmokeSelected,false,false);m_selected=m_selection.Primary();m_operation=-1;
        Engine::Mat4 global;auto r=m_world.GlobalTransform(m_selected,global);if(!r)return r;auto pivot=glm::make_mat4(global.values.data());r=m_drag.Begin(m_world,m_selection.ids,pivot,false);if(!r)return r;
        r=m_drag.Update(glm::translate(pivot,glm::vec3(3,0,0)));m_inputSmokeRevision=m_world.Revision();return r;
    }
    if((frame==280||frame==288)&&(m_drag.Active()||m_history.CanUndo()||m_world.Revision()!=m_inputSmokeRevision))return fail("Escape/RMB preview cancellation mutated history or world");
    if(frame==294&&m_selection.ids.size()!=ids.size())return fail("Ctrl+A all");if(frame==298&&!m_selection.ids.empty())return fail("Escape deselect");
    if(frame==308) {m_inputSmokeEye=m_camera.Eye();m_inputSmokeSpeed=m_camera.speedLog;}
    if(frame==310&&(!m_flying||(ImGui::GetIO().ConfigFlags&ImGuiConfigFlags_NoMouse)==0))return fail("RMB native cursor capture");
    if(frame==313&&(m_operation!=-1||m_camera.Eye().y<=m_inputSmokeEye.y))return fail("E flies up without rotating tool");
    if(frame==315&&m_camera.speedLog<=m_inputSmokeSpeed)return fail("captured scroll changes fly speed");
    if(frame==318&&(m_flying||(ImGui::GetIO().ConfigFlags&ImGuiConfigFlags_NoMouse)!=0))return fail("RMB release restores pointer");
    if(frame==322&&m_operation!=1)return fail("E rotates tool after RMB release");
    if(frame==324) {m_selection.Click(m_inputSmokeSelected,false,false);m_selected=m_selection.Primary();Engine::Scene::FieldValue name;m_world.ReadField(m_selected,Engine::Scene::ComponentType::Name,"value",name);m_inputSmokeName=std::get<std::string>(name);}
    if(frame==334||frame==338) {Engine::Scene::FieldValue name;m_world.ReadField(m_selected,Engine::Scene::ComponentType::Name,"value",name);auto value=std::get<std::string>(name);if(frame==334&&(value.find("F2 rename verified")==std::string::npos||m_renameEntity))return Engine::Result::Failure("F2 rename did not commit: "+value+", active="+std::to_string(bool(m_renameEntity)));if(frame==338&&value!=m_inputSmokeName)return fail("rename undo restores name");}
    if(frame==350)Engine::Log::Success("F2 input smoke passed: bound actions, copy/paste/delete undo, text capture, play restore, Escape/RMB cancel, captured fly/scroll, rename commit/undo");
    return Engine::Result::Success();
}
