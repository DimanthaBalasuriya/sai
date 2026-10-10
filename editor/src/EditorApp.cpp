#include "EditorApp.hpp"
#include "engine/lighting/Lighting.hpp"
#include "engine/lighting/Providers.hpp"
#include <random>
#include <fstream>
#include "DemoScene.hpp"
#include "engine/scene/EntityFeatures.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/Profile.hpp"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
namespace {
std::string Key(Engine::Guid id) { return std::to_string(id.high)+"-"+std::to_string(id.low); }
Engine::Result LightCapacity(Engine::Scene::World& world,Engine::Scene::ComponentType type){
    using namespace Engine;
    std::vector<Guid> ids;std::size_t directional=0,local=0;
    for(auto component:{Lighting::kDirectionalLight,Lighting::kPointLight,Lighting::kSpotLight,Scene::ComponentType::Light}){
        auto result=world.EntitiesWithComponent(component,ids);if(!result)return result;
        for(auto id:ids){bool sun=component==Lighting::kDirectionalLight;if(component==Scene::ComponentType::Light){Scene::FieldValue value;result=world.ReadField(id,component,"type",value);if(!result)return result;sun=std::get<std::uint32_t>(value)==0;}if(sun)++directional;else ++local;}
    }
    if(type==Lighting::kDirectionalLight?directional>=4:local>=4096)return Result::Failure("Light capacity reached (4 directional / 4096 local, including hidden lights)");
    return Result::Success();
}
}
EditorApp::EditorApp(Engine::Rhi::Device& device,Engine::Window& window,Engine::Scene::World& world,Engine::Assets::Registry& registry,std::filesystem::path directory)
    :m_device(device),m_window(window),m_world(world),m_registry(registry),m_directory(std::move(directory)),m_pipeline(device){}
EditorApp::~EditorApp() {
    m_inputBridge.Detach();
    m_device.WaitIdle();
    for (const auto& target:m_targets) if (target.image) { m_device.GuiRemoveTexture(target.texture);m_device.Destroy(target.image); }
    for (const auto& target:m_retired) { m_device.GuiRemoveTexture(target.texture);m_device.Destroy(target.image); }
    if (m_uiReady) m_device.GuiShutdown();
    if (m_inputReady) ImGui_ImplGlfw_Shutdown();
    if (m_contextReady) ImGui::DestroyContext();
}
Engine::Result EditorApp::Initialize(const std::filesystem::path& source,bool smoke) {
    auto result=Engine::Scene::RegisterEntityFeatures();if(!result)return result;
    result=Engine::Extensions::RegisterBuiltins(m_pipeline,m_device,m_directory/"shaders");if (!result) return result;
    result=m_pipeline.Initialize(m_directory/"shaders",Engine::Rhi::Format::Rgba8Srgb);if (!result) return result;
    auto path=source.empty()?m_directory/"assets/material_gallery.gltf":source;
    Engine::Assets::ImportedScene imported;result=m_registry.Import(path,imported);if (!result) return result;
    m_geometry.Store(imported.assets);
    result=m_pipeline.UploadAssets(imported.assets);if (!result) return result;
    result=BuildImportedScene(imported,m_world,m_history);if (!result) return result;m_assetScenes={imported.id};
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
    result=Engine::Sky::EnsureSettings(m_world,m_history);if(!result)return result;
#endif
    if(m_entityBenchmark){std::vector<Engine::Scene::EntityDefinition> definitions;auto capture=EditorTools::Capture(m_world,definitions);if(!capture)return capture;auto count=definitions.size();definitions.resize(100000);for(std::size_t i=count;i<definitions.size();++i){auto& d=definitions[i];d.id={0xf3,i+1};d.name.value="Entity "+std::to_string(i+1);d.transform.position={float(int(i%316)-158)*3,0,-float(i/316)*3};if(i%100==0)d.mesh=Engine::Scene::MeshRenderer{};}Engine::Scene::ReplaceSceneCommand replace(std::move(definitions));auto generated=replace.Do(m_world);if(!generated)return generated;m_history.Clear();m_focusPanel="Hierarchy";m_hierarchyTimes.reserve(512);m_entityFrameTimes.reserve(512);}
    result=ResetCamera();if (!result) return result;
    m_world.Entities(m_entities);for (auto id:m_entities) if (m_world.HasComponent(id,Engine::Scene::ComponentType::MeshRenderer)) { m_selected=id;break; }
    m_selection.Click(m_selected,false,false);
    std::snprintf(m_importPath.data(),m_importPath.size(),"%s",path.string().c_str());
    std::snprintf(m_scenePath.data(),m_scenePath.size(),"%s",(m_directory/"scenes/gallery.scene.json").string().c_str());
    IMGUI_CHECKVERSION();ImGui::CreateContext();m_contextReady=true;auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
    m_iniPath=(m_directory/"editor-layout.ini").string();io.IniFilename=smoke?nullptr:m_iniPath.c_str();
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.ConfigDragClickToInputText=true;
    // ImGui owns the native callbacks; the action router resolves editor contexts.
    glfwSetKeyCallback(static_cast<GLFWwindow*>(m_window.NativeHandle()),nullptr);
    if (!ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(m_window.NativeHandle()),true)) return Engine::Result::Failure("Editor input initialization failed");
    m_inputReady=true;
    m_inputBridge.Attach(static_cast<GLFWwindow*>(m_window.NativeHandle()));
    if(m_bindingsPath.empty()) {
#ifndef NDEBUG
        m_bindingsPath=ENGINE_EDITOR_BINDINGS_PATH;
#else
        m_bindingsPath=m_directory/"input/bindings.json";
#endif
    }
    result=m_actions.Load(m_bindingsPath);if(!result)return result;
    io.MouseDragThreshold=4;
    result=m_device.GuiInitialize();if (!result) return result;m_uiReady=true;
    if(m_themePath.empty()) {
#ifndef NDEBUG
        m_themePath=ENGINE_EDITOR_THEME_PATH;
#else
        m_themePath=m_directory/"ui/theme.json";
#endif
    }
    result=EditorUi::LoadTheme(m_themePath,m_theme);if(!result) return result;
    if(m_uiSmoke) {auto testPath=m_directory/"ui/smoke.theme.json";std::filesystem::copy_file(m_themePath,testPath,std::filesystem::copy_options::overwrite_existing);m_themePath=testPath;}
    m_themeTime=std::filesystem::last_write_time(m_themePath);result=RefreshUi(true);if(!result) return result;
    m_panels.Register("Hierarchy",[&]{auto start=std::chrono::steady_clock::now();Hierarchy();if(m_entityBenchmark&&ImGui::GetFrameCount()>15)m_hierarchyTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());});m_panels.Register("Inspector",[&]{Inspector();});
    m_panels.Register("Asset Browser",[&]{Assets();});m_panels.Register("Console",[&]{Console();});
    m_panels.Register("Render Stats",[&]{Statistics();});m_panels.Register("Viewport",[&]{Viewport();});
    m_panels.Register("Render Features",[&]{RenderFeatures();});
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
    m_panels.Register("Procedural Sky",[&]{SkyPanel();});
#endif
    m_panels.Register("Theme",[&]{ThemePanel();});for(auto& panel:m_panels) if(panel.name=="Theme") panel.open=m_showTheme;
    Engine::Log::Info("Editor ready: edits -> commands -> extraction -> live viewport");return Engine::Result::Success();
}
Engine::Result EditorApp::RefreshUi(bool force) {
#ifndef NDEBUG
    std::error_code error;auto time=std::filesystem::last_write_time(m_themePath,error);
    if(!error&&time!=m_themeTime) {m_themeTime=time;auto result=EditorUi::LoadTheme(m_themePath,m_theme);if(result) {m_themeChanged=true;Engine::Log::Info("Theme reloaded: "+m_themePath.string());} else Engine::Log::Error(result.Error());}
#endif
    float x=1,y=1;glfwGetWindowContentScale(static_cast<GLFWwindow*>(m_window.NativeHandle()),&x,&y);float dpi=m_dpiOverride>0?m_dpiOverride:std::max(x,y);
    const float size=m_theme.metrics.at("font-size"),mono=m_theme.metrics.at("mono-size");
    if(force||m_dpi!=dpi||m_fontSize!=size||m_monoSize!=mono) {
        auto result=m_device.GuiRebuildFonts();if(!result) return result;result=m_fonts.Rebuild(m_directory/"ui/fonts",dpi,size,mono);if(!result) return result;
        m_dpi=dpi;m_fontSize=size;m_monoSize=mono;m_themeChanged=true;
        Engine::Log::Info("UI font atlas rebuilt at "+std::to_string(dpi)+" DPI scale");
    }
    // This backend converts vertex sRGB in its fragment shader, preserving all
    // 8-bit palette values. CPU conversion would quantize dark linear colors.
    if(m_themeChanged) {m_widgets.linear=false;m_widgets.dpi=dpi;EditorUi::ApplyTheme(ImGui::GetStyle(),m_theme,false,dpi);m_themeChanged=false;}
    return Engine::Result::Success();
}
void EditorApp::ThemePanel() {
    ImGui::TextWrapped("%s",m_themePath.string().c_str());ImGui::Text("DPI %.0f%% | %s target",m_dpi*100,m_device.GuiUsesSrgb()?"sRGB":"UNORM");
    if(m_widgets.IconButton("Save to theme.json")) {Report(EditorUi::SaveTheme(m_themePath,m_theme),"Theme saved");m_themeChanged=true;}
    ImGui::SameLine();if(m_widgets.IconButton("Reload")) {Report(EditorUi::LoadTheme(m_themePath,m_theme));m_themeChanged=true;}
    for(auto& [name,color]:m_theme.colors) {ImGui::PushID(name.c_str());if(ImGui::ColorEdit3(name.c_str(),&color.x,ImGuiColorEditFlags_NoInputs)) m_themeChanged=true;ImGui::PopID();}
    if(ImGui::CollapsingHeader("Spacing and fonts")) for(auto& [name,value]:m_theme.metrics) if(ImGui::DragFloat(name.c_str(),&value,.05f,(name=="font-size"||name=="mono-size")?8.f:.01f,name.ends_with("-alpha")?1.f:64.f)) m_themeChanged=true;
}
void EditorApp::ComponentAction(Engine::Scene::ComponentType type,bool remove) {
    if(!remove&&Engine::Lighting::FindLightType(type)&&!m_world.HasComponent(m_selected,type)){auto result=LightCapacity(m_world,type);if(!result){Report(result);return;}}
    std::vector<Engine::Scene::EntityDefinition> definitions;
    for(auto id:m_entities) {Engine::Scene::EntityDefinition entity;auto result=m_world.Definition(id,entity);if(!result) {Report(result);return;}
        if(id==m_selected&&Engine::Scene::IsModuleComponent(type)) {
            std::erase_if(entity.extensions,[&](const auto& c){return c.type==type;});
            if(!remove){Engine::Scene::ModuleComponent component;result=Engine::Scene::DefaultComponent(type,component);if(!result){Report(result);return;}entity.extensions.push_back(std::move(component));}
        }else if(id==m_selected) switch(type) {
        case Engine::Scene::ComponentType::Name:entity.name={};break;
        case Engine::Scene::ComponentType::Transform:entity.transform={};break;
        case Engine::Scene::ComponentType::Hierarchy:entity.hierarchy={};break;
        case Engine::Scene::ComponentType::MeshRenderer:if(remove) entity.mesh.reset();else entity.mesh=Engine::Scene::MeshRenderer{};break;
        case Engine::Scene::ComponentType::Camera:if(remove) entity.camera.reset();else entity.camera=Engine::Scene::Camera{};break;
        case Engine::Scene::ComponentType::Light:if(remove) entity.light.reset();else entity.light=Engine::Scene::Light{};break;
        }definitions.push_back(std::move(entity));
    }
    Report(m_history.Execute(m_world,std::make_unique<Engine::Scene::ReplaceSceneCommand>(std::move(definitions))));
}
void EditorApp::Report(Engine::Result result,std::string_view success) {
    if (!result) { m_actionError=result.Error();Engine::Log::Error(result.Error()); }else if (!success.empty()) Engine::Log::Success(success);
}
void EditorApp::Save() {if(m_play) {Engine::Log::Warning("Stop the play preview before saving the edit scene");return;}Report(Engine::Scene::SaveScene(m_world,m_scenePath.data(),m_assetScenes),"Scene saved"); }
Engine::Result EditorApp::ResetCamera() {
    auto result=m_world.Extract(m_snapshot);if (!result) return result;
    if (!m_snapshot.camera.valid) return Engine::Result::Success();
    m_camera.Reset(m_snapshot.camera);
    return Engine::Result::Success();
}
void EditorApp::Load() { m_pendingLoad=true; }
void EditorApp::LoadNow() {
    std::vector<Engine::Scene::EntityDefinition> definitions;std::vector<Engine::Guid> assets;
    auto result=Engine::Scene::ReadScene(m_scenePath.data(),definitions,assets);if (!result) { Report(result);return; }
    CancelInteraction();if(m_play)TogglePlay();
    for (auto id:assets) { Engine::Assets::ImportedScene scene;result=m_registry.Load(id,scene);if (result) {m_geometry.Store(scene.assets);result=m_pipeline.UploadAssets(scene.assets);}if (!result) { Report(result);return; } }
    result=m_history.Execute(m_world,std::make_unique<Engine::Scene::ReplaceSceneCommand>(std::move(definitions)));
    // Retain dependencies of undoable worlds as well as the current world.
    if (result) for (auto id:assets) if (std::find(m_assetScenes.begin(),m_assetScenes.end(),id)==m_assetScenes.end()) m_assetScenes.push_back(id);
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
    if(result)result=Engine::Sky::EnsureSettings(m_world,m_history);
#endif
    if (result) result=ResetCamera();
    Report(result,"Scene loaded");
}
void EditorApp::CreateLightingSettings(Engine::Scene::ComponentType type){
    std::vector<Engine::Guid> existing;m_world.EntitiesWithComponent(type,existing);if(!existing.empty()){m_selected=existing.front();m_selection.Click(m_selected,false,false);return;}
    Engine::Scene::EntityDefinition entity;std::mt19937_64 random(std::random_device{}());do{entity.id={random(),random()};}while(!entity.id||m_world.Contains(entity.id));entity.name.value=type==Engine::Lighting::kEnvironmentSettings?"Environment settings":type==Engine::Lighting::kFogSettings?"Height fog":"Exposure settings";Engine::Scene::ModuleComponent component;auto r=Engine::Scene::DefaultComponent(type,component);if(r){entity.extensions.push_back(std::move(component));r=m_history.Execute(m_world,std::make_unique<Engine::Scene::CreateEntityCommand>(entity));}if(r){m_selected=entity.id;m_selection.Click(entity.id,false,false);}Report(r);
}
void EditorApp::CreateLight(Engine::Scene::ComponentType type) {
    const auto* info=Engine::Lighting::FindLightType(type);if(!info)return;
    auto capacity=LightCapacity(m_world,type);if(!capacity){Report(capacity);return;}
    Engine::Scene::EntityDefinition entity;std::mt19937_64 random(std::random_device{}());do{entity.id={random(),random()};}while(!entity.id||m_world.Contains(entity.id));
    entity.name.value=info->name;Engine::Scene::ModuleComponent component;auto result=Engine::Scene::DefaultComponent(type,component);if(!result){Report(result);return;}
    entity.extensions.push_back(std::move(component));if(type==Engine::Lighting::kDirectionalLight)entity.transform.rotation={-.9f,-.45f,0};
    result=m_history.Execute(m_world,std::make_unique<Engine::Scene::CreateEntityCommand>(entity));if(result)m_selected=entity.id;Report(result,"Light created");
}
void EditorApp::DrawUi() {
    m_viewHovered=m_viewFocused=m_hierarchyHovered=m_hierarchyFocused=false;m_viewSize={0,0};m_viewDrawList=nullptr;
    m_selection.Prune(m_world);if(m_world.Contains(m_selected)&&!m_selection.Contains(m_selected))m_selection.Click(m_selected,false,false);m_selected=m_selection.Primary();
    auto* viewport=ImGui::GetMainViewport();ImGui::SetNextWindowPos(viewport->WorkPos);ImGui::SetNextWindowSize(viewport->WorkSize);ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::Begin("Engine workspace",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoDocking);
    ImGui::PopStyleVar(2);
    if (ImGui::BeginMenuBar()) {
        auto item=[&](const char* label,const char* action,bool enabled=true){if(ImGui::MenuItem(label,m_actions.Shortcut(action).c_str(),false,enabled))m_actions.Queue(action);};
        if (ImGui::BeginMenu("Scene")) {item("Save","Scene.Save",!m_play);if (ImGui::MenuItem("Load")) Load();item(m_play?"Stop play preview":"Play preview","Scene.Play");if (ImGui::MenuItem("Exit")) m_window.RequestClose();ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Edit")) {item("Undo","Edit.Undo",m_history.CanUndo());item("Redo","Edit.Redo",m_history.CanRedo());ImGui::Separator();item("Duplicate","Edit.Duplicate",bool(m_selected));item("Delete","Edit.Delete",bool(m_selected));item("Copy","Edit.Copy",bool(m_selected));item("Paste","Edit.Paste",!m_clipboard.empty());item("Rename","Edit.Rename",bool(m_selected));item("Hide / show","Edit.Hide",bool(m_selected));item("Select all","Selection.All");ImGui::EndMenu(); }
        if(ImGui::BeginMenu("Create",!m_play)){
            if(ImGui::BeginMenu("Light")){for(const auto& type:Engine::Lighting::LightTypes())if(ImGui::MenuItem(type.name))CreateLight(type.component);ImGui::EndMenu();}
            if(ImGui::MenuItem("Environment settings"))CreateLightingSettings(Engine::Lighting::kEnvironmentSettings);
            if(ImGui::MenuItem("Height fog"))CreateLightingSettings(Engine::Lighting::kFogSettings);
            if(ImGui::MenuItem("Exposure settings"))CreateLightingSettings(Engine::Lighting::kExposureSettings);
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Lighting presets",!m_play)){
            auto presets=Engine::Lighting::Presets();for(std::size_t i=0;i<presets.size();++i)if(ImGui::MenuItem(presets[i].name))Report(Engine::Lighting::ApplyPreset(m_world,m_history,i),"Lighting preset applied");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Panels")) { for (auto& panel:m_panels) ImGui::MenuItem(panel.name.c_str(),nullptr,&panel.open);ImGui::EndMenu(); }
        m_widgets.StatusChip(m_play?"PLAY PREVIEW":"ENGINE / EDIT SCENE",m_play?"positive":"info");ImGui::EndMenuBar();
    }
    auto dock=ImGui::GetID("EngineDock");
    if (!m_layoutReady && !ImGui::DockBuilderGetNode(dock)) {
        ImGui::DockBuilderAddNode(dock,ImGuiDockNodeFlags_DockSpace);ImGui::DockBuilderSetNodeSize(dock,viewport->WorkSize);
        ImGuiID center=dock,left,right,bottom;ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,0.21f,&left,&center);ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,0.29f,&right,&center);ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,0.23f,&bottom,&center);
        ImGui::DockBuilderDockWindow("Hierarchy",left);ImGui::DockBuilderDockWindow("Asset Browser",left);ImGui::DockBuilderDockWindow("Inspector",right);ImGui::DockBuilderDockWindow("Render Stats",right);ImGui::DockBuilderDockWindow("Theme",right);ImGui::DockBuilderDockWindow("Render Features",right);ImGui::DockBuilderDockWindow("Procedural Sky",right);ImGui::DockBuilderDockWindow("Console",bottom);ImGui::DockBuilderDockWindow("Viewport",center);ImGui::DockBuilderFinish(dock);
    }
    m_layoutReady=true;ImGui::DockSpace(dock);ImGui::End();
    if(m_entityListVersion!=m_world.HierarchyVersion()){m_world.Entities(m_entities);m_entityListVersion=m_world.HierarchyVersion();}
    for (auto& panel:m_panels) if (panel.open) {
        // DockBuilder creates the tab bar on the following frame. Delay explicit
        // startup focus until every panel exists, otherwise later appearing tabs win.
        if(panel.name==m_focusPanel&&ImGui::GetFrameCount()>2) {
            if(auto* window=ImGui::FindWindowByName(panel.name.c_str());window&&window->DockNode&&window->DockNode->TabBar) window->DockNode->TabBar->NextSelectedTabId=window->TabId;
            ImGui::SetNextWindowFocus();m_focusPanel.clear();
        }
        if (ImGui::Begin(panel.name.c_str(),&panel.open)) panel.draw();ImGui::End();
    }
}
void EditorApp::Hierarchy() {
    m_hierarchyHovered=ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);m_hierarchyFocused=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    m_widgets.SearchField("Search entities",m_search.data(),m_search.size());ImGui::TextColored(m_widgets.Token("text-muted"),"%zu entities",m_entities.size());ImGui::Separator();
    ENGINE_PROFILE_SCOPE("Editor.HierarchyDraw");
    if(!m_hierarchyModel.Refresh(m_world))return;m_hierarchyModel.Filter(m_search.data());if(m_renameEntity)m_hierarchyModel.Reveal(m_renameEntity);
    auto rows=m_hierarchyModel.Rows();
    ImGui::BeginChild("Entity rows",{0,-ImGui::GetFrameHeightWithSpacing()},false);
    ImGuiListClipper clipper;clipper.Begin(static_cast<int>(rows.size()),ImGui::GetFrameHeightWithSpacing());
    while(clipper.Step())for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row){auto& entry=m_hierarchyModel.Entry(rows[row]);auto id=entry.id;
        auto flags=ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_NoTreePushOnOpen|ImGuiTreeNodeFlags_FramePadding;
        if(m_selection.Contains(id))flags|=ImGuiTreeNodeFlags_Selected;if(!entry.children)flags|=ImGuiTreeNodeFlags_Leaf;
        float indent=entry.depth*16.f;ImGui::Indent(indent);ImGui::PushID(Key(id).c_str());
        ImGui::SetNextItemOpen(m_hierarchyModel.Open(id),ImGuiCond_Always);
        auto text=entry.name;if(m_visibility.Hidden(id))text+=" [hidden]";
        bool open=m_widgets.TreeRow(text.c_str(),flags);if(entry.children&&ImGui::IsItemToggledOpen())m_hierarchyModel.SetOpen(id,open);
        if(m_inputBridge.ItemClicked()&&!ImGui::IsItemToggledOpen()){m_hierarchyClick=id;m_actions.Queue("Selection.HierarchyClick");}
        if(m_inputBridge.ItemDoubleClicked()){m_hierarchyClick=id;m_actions.Queue("Camera.FocusClicked");}
        if(ImGui::BeginDragDropSource()){ImGui::SetDragDropPayload("EntityGuid",&id,sizeof(id));ImGui::TextUnformatted(entry.name.c_str());ImGui::EndDragDropSource();}
        if(ImGui::BeginDragDropTarget()){if(auto* payload=ImGui::AcceptDragDropPayload("EntityGuid"))Report(m_history.Execute(m_world,std::make_unique<Engine::Scene::ReparentCommand>(*static_cast<const Engine::Guid*>(payload->Data),id)));ImGui::EndDragDropTarget();}
        if(m_renameEntity==id){ImGui::SameLine();bool first=!m_renameStarted;if(first)ImGui::SetKeyboardFocusHere();bool enter=ImGui::InputText("##rename",m_renameName.data(),m_renameName.size(),ImGuiInputTextFlags_EnterReturnsTrue);if(first){ImGui::SetScrollHereY(.5f);m_renameStarted=ImGui::IsItemActive();}if(m_renameStarted&&(enter||ImGui::IsItemDeactivated()))m_renameCommit=true;}
        ImGui::PopID();ImGui::Unindent(indent);
    }
    ImGui::EndChild();
    if (m_selected&&m_widgets.IconButton("Move selected to root")) Report(m_history.Execute(m_world,std::make_unique<Engine::Scene::ReparentCommand>(m_selected,Engine::Guid{})));
}
void EditorApp::Inspector() {
    if (!m_world.Contains(m_selected)) { ImGui::TextColored(m_widgets.Token("text-muted"),"Select an entity in Hierarchy");return; }
    if(m_inspectorSelected!=m_selected||m_inspectorVersion!=m_world.ComponentVersion(m_selected)){
        m_inspectorFields.clear();for(auto& component:m_world.Reflection())if(m_world.HasComponent(m_selected,component.type))for(auto& field:component.fields){Engine::Scene::FieldValue value;if(m_world.ReadField(m_selected,component.type,field.name,value))m_inspectorFields.push_back({component.type,std::string(field.name),std::move(value)});}
        m_inspectorSelected=m_selected;m_inspectorVersion=m_world.ComponentVersion(m_selected);
    }
    ImGui::PushStyleColor(ImGuiCol_Text,m_widgets.Token("text-muted"));ImGui::TextWrapped("GUID %s",Key(m_selected).c_str());ImGui::PopStyleColor();ImGui::Separator();
    for (const auto& component:m_world.Reflection()) if (m_world.HasComponent(m_selected,component.type)) {
        ImGui::PushID(static_cast<int>(component.type));
        bool removed=false;
        bool open=m_widgets.ComponentHeader(std::string(component.name).c_str(),"\xef\x86\xb2",nullptr,[&]{
            if(ImGui::MenuItem("Reset component")) ComponentAction(component.type,false);
            bool optional=Engine::Scene::IsModuleComponent(component.type)||component.type==Engine::Scene::ComponentType::MeshRenderer||component.type==Engine::Scene::ComponentType::Camera||component.type==Engine::Scene::ComponentType::Light;
            ImGui::PushStyleColor(ImGuiCol_Text,m_widgets.Token("negative"));if(ImGui::MenuItem("Remove component",nullptr,false,optional)) {ComponentAction(component.type,true);removed=true;}ImGui::PopStyleColor();
            if(!optional) m_widgets.Tooltip("Required scene components cannot be removed");
        });if(!open||removed) {ImGui::PopID();continue;}
        const bool lightComponent=Engine::Lighting::FindLightType(component.type)!=nullptr;
        auto drawField=[&](const Engine::Scene::FieldInfo& field){
            auto cached=std::find_if(m_inspectorFields.begin(),m_inspectorFields.end(),[&](auto& f){return f.type==component.type&&f.name==field.name;});if(cached==m_inspectorFields.end())return;auto value=cached->value;
            bool changed=false;std::string label(field.name);
            if(lightComponent&&field.name=="unit"){
                auto current=std::get<std::uint32_t>(value);const char* labels[]{component.type==Engine::Lighting::kDirectionalLight?"Lux":component.type==Engine::Lighting::kRectLight||component.type==Engine::Lighting::kDiskLight?"Nit":"Candela","Lumen","Artist / unitless"};
                m_widgets.PropertyRow("Intensity unit",[&]{bool edited=false;if(ImGui::BeginCombo("##unit",labels[current])){for(std::uint32_t unit=0;unit<3;++unit){if(component.type==Engine::Lighting::kDirectionalLight&&unit==1)continue;if(ImGui::Selectable(labels[unit],unit==current)){Report(Engine::Lighting::ChangeIntensityUnit(m_world,m_history,m_selected,component.type,unit));edited=true;}}ImGui::EndCombo();}return edited;});return;
            }
            if(component.type==Engine::Lighting::kEnvironmentSettings&&field.name=="provider"){int provider=int(std::get<std::uint32_t>(value));const char* choices[]{"Constant ambient","HDRI environment","Procedural sky"};changed=m_widgets.PropertyRow("Provider",[&]{return ImGui::Combo("##provider",&provider,choices,3);});if(changed)Report(m_history.Execute(m_world,std::make_unique<Engine::Scene::SetFieldCommand>(m_selected,component.type,std::string(field.name),std::uint32_t(provider))));return;}
            switch(field.type) {
            case Engine::Scene::FieldType::String: { std::array<char,512> text{};std::snprintf(text.data(),text.size(),"%s",std::get<std::string>(value).c_str());changed=m_widgets.PropertyRow(label.c_str(),[&]{return ImGui::InputText("##value",text.data(),text.size());});if (changed) value=std::string(text.data());break; }
            case Engine::Scene::FieldType::Vector3: { auto vector=std::get<Engine::Vec3>(value);Engine::Vec3 reset{};if(field.name=="scale"||field.name=="color") reset={1,1,1};if(lightComponent&&field.name=="color")changed=m_widgets.PropertyRow("Color (linear)",[&]{return ImGui::ColorEdit3("##color",&vector.x,ImGuiColorEditFlags_Float);});else changed=m_widgets.DragVec3(label.c_str(),vector,field.minimum,field.maximum,reset);if (changed) value=vector;break; }
            case Engine::Scene::FieldType::Float: { auto v=std::get<float>(value);if(lightComponent&&field.name=="temperature")changed=m_widgets.PropertyRow("Temperature",[&]{return ImGui::SliderFloat("##temperature",&v,field.minimum,field.maximum,"%.0f K");});
                else if(lightComponent&&(field.name=="innerCone"||field.name=="outerCone")){float degrees=v*57.2957795f;changed=m_widgets.PropertyRow(label.c_str(),[&]{return ImGui::DragFloat("##angle",&degrees,.1f,field.minimum*57.2957795f,field.maximum*57.2957795f,"%.1f degrees");});v=degrees*.01745329252f;}
                else changed=m_widgets.PropertyRow(label.c_str(),[&]{ImGui::PushFont(m_fonts.mono);float step=(lightComponent&&(field.name=="illuminance"||field.name=="intensity"))?std::max(.01f,std::abs(v)*.01f):field.maximum-field.minimum<.1f?(field.maximum-field.minimum)*.0001f:.02f;bool c=ImGui::DragFloat("##value",&v,step,field.minimum,field.maximum,field.maximum-field.minimum<.1f?"%.6g":"%.3f");ImGui::PopFont();return c;});if (changed) value=v;break; }
            case Engine::Scene::FieldType::Unsigned: { int v=static_cast<int>(std::get<std::uint32_t>(value));if((lightComponent||component.type==Engine::Lighting::kExposureSettings)&&field.maximum==1){bool on=v!=0;changed=m_widgets.PropertyRow(label.c_str(),[&]{return ImGui::Checkbox("##value",&on);});v=on?1:0;}else changed=m_widgets.PropertyRow(label.c_str(),[&]{return ImGui::SliderInt("##value",&v,static_cast<int>(field.minimum),static_cast<int>(field.maximum));});if (changed) value=static_cast<std::uint32_t>(v);break; }
            case Engine::Scene::FieldType::Guid: {
                auto guid=std::get<Engine::Guid>(value);changed=m_widgets.PropertyRow(label.c_str(),[&]{bool c=false;if(ImGui::BeginCombo("##link",guid?Key(guid).c_str():"None")){if(ImGui::Selectable("None",!guid)){guid={};c=true;}for(auto id:m_entities){if(field.name=="light"&&!m_world.HasComponent(id,Engine::Scene::ComponentType::Light)&&!m_world.HasComponent(id,Engine::Lighting::kDirectionalLight))continue;Engine::Scene::FieldValue name;m_world.ReadField(id,Engine::Scene::ComponentType::Name,"value",name);ImGui::PushID(Key(id).c_str());if(ImGui::Selectable(std::get<std::string>(name).c_str(),id==guid)){guid=id;c=true;}ImGui::PopID();}ImGui::EndCombo();}return c;});if(changed)value=guid;break;
            }
            }
            auto editKey=Key(m_selected)+std::to_string(static_cast<int>(component.type))+label;
            if (changed) { bool merge=m_lastEdit==editKey&&!ImGui::IsItemActivated();Report(m_history.Execute(m_world,std::make_unique<Engine::Scene::SetFieldCommand>(m_selected,component.type,label,value),merge));m_lastEdit=editKey; }
            if (ImGui::IsItemDeactivated()) m_lastEdit.clear();
        };
        auto shadowField=[](std::string_view name){return name=="castShadows"||name=="shadowResolution"||name=="filterQuality"||name=="depthBias"||name=="normalBias"||name=="cascadeCount"||name=="maxShadowDistance"||name=="splitBlend"||name=="cascadeBlend";};
        for(const auto& field:component.fields)if(!lightComponent||!shadowField(field.name))drawField(field);
        if(lightComponent){
            Engine::Scene::FieldValue value,unit,angle;auto field=component.type==Engine::Lighting::kDirectionalLight?"illuminance":"intensity";
            m_world.ReadField(m_selected,component.type,field,value);m_world.ReadField(m_selected,component.type,"unit",unit);
            float cone=.523599f;if(component.type==Engine::Lighting::kSpotLight){m_world.ReadField(m_selected,component.type,"outerCone",angle);cone=std::get<float>(angle);}
            float physical=Engine::Lighting::PhysicalIntensity(component.type,std::get<float>(value),std::get<std::uint32_t>(unit),cone,Engine::Lighting::EmittingArea(m_world,m_selected,component.type));
            ImGui::Text("Equivalent: %.5g %s",physical,component.type==Engine::Lighting::kDirectionalLight?"lux":component.type==Engine::Lighting::kRectLight||component.type==Engine::Lighting::kDiskLight?"nit":"cd");if(ImGui::Button("Solo light"))Report(Engine::Lighting::SoloLight(m_world,m_history,m_selected));ImGui::SameLine();Engine::Scene::FieldValue enabled;m_world.ReadField(m_selected,component.type,"enabled",enabled);if(ImGui::Button(std::get<std::uint32_t>(enabled)?"Mute":"Unmute"))Report(m_history.Execute(m_world,std::make_unique<Engine::Scene::SetFieldCommand>(m_selected,component.type,"enabled",std::uint32_t(!std::get<std::uint32_t>(enabled)))));
            if(std::get<std::uint32_t>(unit)==2)ImGui::TextDisabled("Artist mode: unitless authoring value");
            
            if(ImGui::CollapsingHeader("Shadow settings")){for(const auto& f:component.fields)if(shadowField(f.name))drawField(f);}
        }
ImGui::PopID();
    }
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
    if(m_world.HasComponent(m_selected,Engine::Sky::kTime)&&ImGui::CollapsingHeader("Sky layers"))if(auto sky=Engine::Sky::FindSky(m_pipeline.Features()))for(auto& entry:sky->Layers().Entries()){bool enabled=entry.enabled;ImGui::PushID(entry.layer->Name());if(ImGui::Checkbox(entry.layer->Name(),&enabled))sky->Layers().SetEnabled(entry.layer->Name(),enabled);ImGui::PopID();}
#endif
    if(ImGui::Button("Add component"))ImGui::OpenPopup("Add reflected component");
    if(ImGui::BeginPopup("Add reflected component")){Engine::Scene::EntityDefinition selected;m_world.Definition(m_selected,selected);for(auto& component:m_world.Reflection())if(Engine::Scene::IsModuleComponent(component.type)&&!m_world.HasComponent(m_selected,component.type)){if(ImGui::MenuItem(std::string(component.name).c_str(),nullptr,false,!Engine::Lighting::FindLightType(component.type)||!Engine::Lighting::HasLight(selected)))ComponentAction(component.type,false);}ImGui::EndPopup();}
}
void EditorApp::ImportNow() {
        CancelInteraction();if(m_play)TogglePlay();
        Engine::Assets::ImportedScene imported;auto result=m_registry.Import(m_importPath.data(),imported);if (result) result=m_pipeline.UploadAssets(imported.assets);
        if(result)m_geometry.Store(imported.assets);
        if (result) {
            Engine::Scene::World replacement;Engine::Scene::CommandHistory history;result=BuildImportedScene(imported,replacement,history);
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
            if(result)result=Engine::Sky::EnsureSettings(replacement,history);
#endif
            if (result) { std::vector<Engine::Guid> ids;replacement.Entities(ids);std::vector<Engine::Scene::EntityDefinition> definitions;for (auto id:ids) { Engine::Scene::EntityDefinition entity;replacement.Definition(id,entity);definitions.push_back(std::move(entity)); }result=m_history.Execute(m_world,std::make_unique<Engine::Scene::ReplaceSceneCommand>(std::move(definitions))); }
            if (result&&std::find(m_assetScenes.begin(),m_assetScenes.end(),imported.id)==m_assetScenes.end()) m_assetScenes.push_back(imported.id);
            if (result) result=ResetCamera();
        }Report(result,"Imported scene opened");
}
void EditorApp::Assets() {
    ImGui::TextColored(m_widgets.Token("text-muted"),"Import glTF / GLB");ImGui::InputText("Source",m_importPath.data(),m_importPath.size());
    if (m_widgets.IconButton("Open imported scene")) m_pendingImport=true;
    ImGui::Separator();ImGui::InputText("Scene file",m_scenePath.data(),m_scenePath.size());if (m_widgets.IconButton("Save scene")) Save();ImGui::SameLine();if (m_widgets.IconButton("Load scene")) Load();
    ImGui::Separator();ImGui::TextColored(m_widgets.Token("text-muted"),"GUID registry / cooked scenes");
    for (const auto& entry:m_registry.Entries()) { ImGui::BulletText("%s",entry.name.c_str());m_widgets.Tooltip(("GUID "+Key(entry.id)+"\n"+entry.source).c_str()); }
}
void EditorApp::Console() {
    m_widgets.SearchField("Filter console",m_consoleSearch.data(),m_consoleSearch.size());ImGui::Separator();ImGui::BeginChild("Log scroll");ImGui::PushFont(m_fonts.mono);
    for (const auto& message:Engine::Log::RecentDetailed()) {
        if(m_consoleSearch[0]&&message.text.find(m_consoleSearch.data())==std::string::npos) continue;
        const char* token=message.level==Engine::Log::Level::Error?"negative":message.level==Engine::Log::Level::Warning?"warning":message.level==Engine::Log::Level::Success?"positive":"info";
        ImGui::PushStyleColor(ImGuiCol_Text,m_widgets.Token(token));ImGui::TextWrapped("%s",message.text.c_str());ImGui::PopStyleColor();
    }ImGui::PopFont();
    if (ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-5) ImGui::SetScrollHereY(1);
    ImGui::EndChild();
}
void EditorApp::Statistics() {
    auto statistics=m_device.Statistics();m_widgets.StatusChip(statistics.validationMessages?"Validation messages present":"Validation clean",statistics.validationMessages?"negative":"positive");ImGui::Text("Instances: %zu",m_pipeline.LastInstanceCount());ImGui::Text("GPU frame: %.3f ms",statistics.gpuFrameMilliseconds);ImGui::Text("Local GPU memory: %.1f / %.1f MiB",double(statistics.memoryUsedBytes)/(1024*1024),double(statistics.memoryBudgetBytes)/(1024*1024));
    const auto& adapter=m_device.Adapter();ImGui::Text("GPU: %s | %.0f MiB",adapter.name.c_str(),double(adapter.localMemoryBytes)/(1024*1024));ImGui::Text("Camera EV100: %.2f | EC: %+.2f",m_snapshot.camera.ev100,m_snapshot.camera.exposureCompensation);
    if(!m_gpuSettingLoaded){std::ifstream settings(m_directory/"gpu.txt");std::string value;if(settings)std::getline(settings,value);std::copy_n(value.data(),std::min(value.size(),m_gpuOverride.size()-1),m_gpuOverride.data());m_gpuSettingLoaded=true;}
    ImGui::InputText("GPU override (index/name)",m_gpuOverride.data(),m_gpuOverride.size());
    if(ImGui::Button("Save GPU preference for next launch")){std::ofstream settings(m_directory/"gpu.txt");settings<<m_gpuOverride.data();settings.flush();if(settings)Engine::Log::Info("GPU preference saved; restart the editor to apply. Blank selects automatically.");else Engine::Log::Error("Cannot save GPU preference");}
    const auto& lights=m_pipeline.LightingStatistics();ImGui::Text("Rendered EV100: %.2f | target %.2f",lights.exposureEv,lights.exposureTarget);if(m_snapshot.settings.autoExposure)ImGui::PlotHistogram("Luminance histogram",lights.histogram.data(),256,0,nullptr,0,FLT_MAX,{0,60});ImGui::Text("Shadow tiles: %u | depth passes: %u",lights.shadowTiles,lights.shadowPasses);ImGui::Text("Clusters: %u | peak: %u | overflow: %u | visible locals: %u",lights.clusterCount,lights.maxPerCluster,lights.overflow,lights.visibleLocal);ImGui::Text("Light slots: %u directional / %u local",lights.directionalCount,lights.localCount);ImGui::Text("Lights packed: %llu / uploaded bytes: %llu",static_cast<unsigned long long>(lights.packedLights),static_cast<unsigned long long>(lights.uploadedBytes));
    const auto& entities=m_world.Statistics();ImGui::Separator();ImGui::Text("Entities: %zu (%zu static / %zu dynamic)",m_world.EntityCount(),entities.staticEntities,entities.dynamicEntities);ImGui::Text("Dirty: %zu / updated: %zu",entities.dirty,entities.updated);ImGui::Text("Transform %.4f ms / extract %.4f ms",entities.updateMs,entities.extractMs);ImGui::Text("Visible instances: %zu / draw calls: %zu",m_pipeline.LastVisibleInstanceCount(),m_pipeline.LastDrawCallCount());
    if(ImGui::TreeNode("Depth histogram")){for(std::size_t i=0;i<entities.depthHistogram.size();++i)if(entities.depthHistogram[i])ImGui::Text("Depth %zu: %zu",i,entities.depthHistogram[i]);ImGui::TreePop();}
    ImGui::PlotLines("Frame ms",m_frameTimes.data(),static_cast<int>(m_frameTimes.size()),static_cast<int>(m_frameTimeIndex),nullptr,0,33,{0,90});
    ImGui::Separator();for (const auto& pass:m_device.PassTimings()) ImGui::Text("%s: %.3f ms",pass.name.c_str(),pass.gpuMilliseconds);
    ImGui::Separator();ImGui::TextColored(m_widgets.Token("text-muted"),"CPU pass recording");
    for (const auto& [name,time]:m_pipeline.CpuPassTimes()) ImGui::Text("%s: %.3f ms",name.c_str(),time);
    for (const auto& [name,time]:m_uiCpuTimes) ImGui::Text("%s: %.3f ms",name.c_str(),time);
}
void EditorApp::RenderFeatures() {
    ImGui::TextColored(m_widgets.Token("text-muted"),"Registered render modules");
    m_featureCenters.clear();
    for (auto& entry:m_pipeline.Features().Entries()) {
        ImGui::PushID(entry.feature->Name());
        if (m_widgets.ToggleSwitch(entry.feature->Name(),&entry.enabled)) Engine::Log::Info(std::string("Render feature ")+entry.feature->Name()+(entry.enabled?" enabled":" disabled"));
        auto minimum=ImGui::GetItemRectMin(),maximum=ImGui::GetItemRectMax();m_featureCenters.push_back({(minimum.x+maximum.x)*0.5f,(minimum.y+maximum.y)*0.5f});
        ImGui::BeginDisabled(!entry.enabled);
        for (auto& parameter:entry.feature->Parameters()) ImGui::SliderFloat(parameter.name,parameter.value,parameter.minimum,parameter.maximum);
        ImGui::EndDisabled();ImGui::Separator();ImGui::PopID();
    }
    ImGui::TextWrapped("Sky and post effects operate in Lit view. Debug Draw uses x-ray lines. Feature toggles are editor session settings.");
}
void EditorApp::Viewport() {
    ImGui::Checkbox("Light bounds",&m_showLightBounds);ImGui::SameLine();ImGui::Checkbox("Entity icons",&m_showEntityIcons);ImGui::SameLine();
    const char* views[]{"Lit","Albedo","Normals","Depth","Wireframe","Lighting only","Roughness","Metallic","AO","Cluster heatmap","Shadow cascades","Shadow atlas","Shadow bias"};ImGui::SetNextItemWidth(140);ImGui::Combo("View",&m_view,views,13);ImGui::SameLine();
    const char* actions[]{"Tool.Select","Tool.Move","Tool.Rotate","Tool.Scale"};const char* names[]{"Select","Move","Rotate","Scale"};
    for(int i=0;i<4;++i) {if(i)ImGui::SameLine();auto label=std::string(names[i])+" ["+m_actions.Shortcut(actions[i])+"]###"+names[i];if(m_widgets.IconButton(label.c_str()))m_actions.Queue(actions[i]);}
    ImGui::SetNextItemWidth(95);const char* modes[]{"World","Local"};ImGui::Combo("Axes",&m_mode,modes,2);ImGui::SameLine();ImGui::SetNextItemWidth(100);const char* pivots[]{"Center","Individual"};ImGui::Combo("Pivot",&m_pivotMode,pivots,2);ImGui::SameLine();
    if(m_widgets.IconButton("Snap"))ImGui::OpenPopup("Snap settings");if(ImGui::BeginPopup("Snap settings")) {ImGui::DragFloat("Grid",&m_gridSnap,.05f,.01f,100);ImGui::DragFloat("Angle",&m_angleSnap,1,1,90);ImGui::DragFloat("Scale",&m_scaleSnap,.01f,.01f,1);ImGui::TextWrapped("Ctrl snaps to grid/angle/scale. Move also snaps the pivot to unselected surfaces under the cursor.");ImGui::EndPopup();}
    ImGui::SameLine();ImGui::TextDisabled("%zu selected",m_selection.ids.size());
    auto available=ImGui::GetContentRegionAvail();if (available.x<16||available.y<16) return;
    auto width=static_cast<std::uint32_t>(std::clamp(available.x,16.0f,4096.0f)),height=static_cast<std::uint32_t>(std::clamp(available.y,16.0f,4096.0f));
    auto& target=m_targets[m_frame.frameSlot];
    if (!target.image||target.width!=width||target.height!=height) {
        ViewportTarget replacement;replacement.width=width;replacement.height=height;
        auto result=m_device.CreateImage({"Live viewport",width,height,Engine::Rhi::Format::Rgba8Srgb,Engine::Rhi::ImageUsage::ColorAttachment|Engine::Rhi::ImageUsage::Sampled},replacement.image);
        if (result) result=m_device.GuiTexture(replacement.image,replacement.texture);if (!result) { Report(result);return; }
        if (target.image) m_retired.push_back(target);target=replacement;
    }
    auto position=ImGui::GetCursorScreenPos();ImGui::Image(static_cast<ImTextureID>(target.texture),available);
    m_viewPosition={position.x,position.y};m_viewSize={available.x,available.y};m_viewHovered=ImGui::IsItemHovered();m_viewFocused=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);m_viewDrawList=ImGui::GetWindowDrawList();
}
Engine::Result EditorApp::SmokeStep(int frame) {
    if(m_uiSmoke&&frame>=180) {
        if(frame==180) {m_world.Entities(m_entities);for(auto id:m_entities) if(m_world.HasComponent(id,Engine::Scene::ComponentType::MeshRenderer)) {m_selected=id;break;}m_focusPanel="Inspector";}
        if(frame==185) m_dpiOverride=1.25f;if(frame==200) m_dpiOverride=1.5f;if(frame==215) m_dpiOverride=1.f;
        if(frame==186||frame==201||frame==216) {
            float expected=frame==186?1.25f:frame==201?1.5f:1.f;
            if(m_dpi!=expected||ImGui::GetStyle().WindowPadding.x!=8*expected||m_fonts.regular->FontSize!=std::floor(15*expected)) return Engine::Result::Failure("Live DPI rebuild failed");
        }
#ifndef NDEBUG
        if(frame==190) {auto changed=m_theme;changed.colors.at("btn")={.2f,.3f,.4f,1};return EditorUi::SaveTheme(m_themePath,changed);}
        if(frame==191&&std::abs(m_theme.colors.at("btn").x-.2f)>.001f) return Engine::Result::Failure("Theme file change did not hot reload");
        if(frame==195) {std::ofstream file(m_themePath);file<<"{invalid json}";}
        if(frame==196&&std::abs(m_theme.colors.at("btn").x-.2f)>.001f) return Engine::Result::Failure("Invalid reload replaced the working theme");
        if(frame==197) return EditorUi::SaveTheme(m_themePath,m_theme);
#endif
        if(frame==220) {auto result=m_world.Definition(m_selected,m_uiSmokeEntity);if(!result) return result;ComponentAction(Engine::Scene::ComponentType::Transform,false);}
        if(frame==221) {Engine::Scene::EntityDefinition entity;m_world.Definition(m_selected,entity);if(entity.transform.position.x!=0||entity.transform.scale.x!=1) return Engine::Result::Failure("Component reset failed");return m_history.Undo(m_world);}
        if(frame==222) {Engine::Scene::EntityDefinition entity;m_world.Definition(m_selected,entity);if(entity.transform.position.x!=m_uiSmokeEntity.transform.position.x) return Engine::Result::Failure("Component reset undo failed");ComponentAction(Engine::Scene::ComponentType::MeshRenderer,true);}
        if(frame==223) {if(m_world.HasComponent(m_selected,Engine::Scene::ComponentType::MeshRenderer)) return Engine::Result::Failure("Component removal failed");return m_history.Undo(m_world);}
        if(frame==224&&!m_world.HasComponent(m_selected,Engine::Scene::ComponentType::MeshRenderer)) return Engine::Result::Failure("Component removal undo failed");
        if(frame==230) Engine::Log::Success("F1 UI smoke passed: live DPI rebuilds, component reset/remove undo (hot reload checked in Debug)");
    }
    if(frame==135)m_focusPanel="Render Features";
    if (frame==5) { Engine::Scene::EntityDefinition entity;auto result=m_world.Definition(m_selected,entity);if (!result) return result;m_smokeBefore=entity.transform;auto transform=entity.transform;transform.position.x+=1;return m_history.Execute(m_world,std::make_unique<Engine::Scene::SetTransformCommand>(m_selected,transform)); }
    if (frame==10) return m_history.Undo(m_world);if (frame==15) return m_history.Redo(m_world);
    if (frame==20) return Engine::Scene::SaveScene(m_world,m_scenePath.data(),m_assetScenes);
    if (frame==25) { auto result=Engine::Scene::LoadScene(m_world,m_history,m_scenePath.data(),m_assetScenes);if (!result) return result;Engine::Scene::EntityDefinition entity;result=m_world.Definition(m_selected,entity);if (!result||std::abs(entity.transform.position.x-m_smokeBefore.position.x-1)>0.001f) return Engine::Result::Failure("Editor save/load lost transform"); }
    if (frame==26) Load();
    if (frame==27) {
        auto sample=m_directory/"assets/khronos/MetalRoughSpheresNoTextures.gltf";
        if (std::filesystem::exists(sample)) std::snprintf(m_importPath.data(),m_importPath.size(),"%s",sample.string().c_str());
        m_pendingImport=true;
    }
    if (frame==28) return m_history.Undo(m_world);
    if (frame==29) return m_history.Redo(m_world);
    if(frame==31)return m_history.Execute(m_world,std::make_unique<Engine::Scene::SetFieldCommand>(Engine::Guid{1,3},Engine::Scene::ComponentType::Transform,"rotation",Engine::Vec3{-.6f,.8f,0}));
    if(frame==32)return m_history.Undo(m_world);if(frame==33)return m_history.Redo(m_world);
    if (frame==30) m_window.Resize(1280,800);if (frame==60) m_window.Resize(1920,1080);if (frame==90) m_window.Resize(1400,900);
    if (frame==120) m_window.Minimize();if (frame==130) { m_window.Restore();m_window.Resize(1600,1000); }
    if (frame==35||frame==45||frame==55||frame==65||frame==75) m_view=(frame-35)/10;
    if (frame==85) m_view=0;
    if (frame>=95&&frame<115) {
        auto entries=m_pipeline.Features().Entries();
        for (std::size_t i=0;i<entries.size();++i) entries[i].enabled=frame>=100&&i==static_cast<std::size_t>((frame-100)/4);
    }
    if (frame==115) for (auto& entry:m_pipeline.Features().Entries()) entry.enabled=true;
    return Engine::Result::Success();
}
Engine::Result EditorApp::Run(int smokeFrames,bool exercise) {
    const auto started=std::chrono::steady_clock::now();auto entityPrevious=started;int iteration=0;
    if (exercise) ImGui::GetIO().ConfigInputTrickleEventQueue=false;
    while (!m_window.ShouldClose()) {
        if(m_entityBenchmark&&iteration>15)m_entityFrameTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-entityPrevious).count());
        entityPrevious=std::chrono::steady_clock::now();
        auto frameStarted=std::chrono::steady_clock::now();if (exercise) { auto result=SmokeStep(iteration);if (!result) return result; }
        if(m_inputSmoke) {auto result=InputSmokeStep(iteration);if(!result)return result;}
        m_window.PollEvents();
        auto uiResult=RefreshUi();if(!uiResult) return uiResult;
        // Uploads wait for GPU completion and must precede command recording.
        if (m_pendingImport) { m_pendingImport=false;ImportNow(); }
        if (m_pendingLoad) { m_pendingLoad=false;LoadNow(); }
        if (exercise&&!m_actionError.empty()) return Engine::Result::Failure(m_actionError);
        if (!m_window.IsMinimized()) {
            ENGINE_PROFILE_SCOPE("Editor frame");
            for (const auto& target:m_retired) { auto result=m_device.GuiRemoveTexture(target.texture);if (result) result=m_device.Destroy(target.image);if (!result) return result; }m_retired.clear();
            auto result=m_pipeline.PrepareFrame();if (!result) return result;result=m_device.GuiNewFrame();if (!result) return result;
            ImGui_ImplGlfw_NewFrame();
            // Feed the normal ImGui input path deterministically during smoke;
            // desktop cursor movement must not make automated UI checks flaky.
            if (exercise&&iteration>=140&&iteration<172) {
                auto index=static_cast<std::size_t>((iteration-140)/8);int phase=(iteration-140)%8;
                if (index<m_featureCenters.size()) {
                    auto center=m_featureCenters[index];auto& io=ImGui::GetIO();io.AddMousePosEvent(center[0],center[1]);
                    if (phase==0||phase==4) io.AddMouseButtonEvent(0,true);
                    if (phase==1||phase==5) io.AddMouseButtonEvent(0,false);
                }
            }
            if(m_inputSmoke)InjectInputSmoke(iteration);
            ImGui::NewFrame();ImGuizmo::BeginFrame();m_anim.BeginFrame(ImGui::GetIO().DeltaTime,ImGui::GetFrameCount());
            bool available=false;result=m_device.BeginFrame(m_frame,available);if (!result) return result;
            if (!available) { ImGui::EndFrame();continue; }
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
            if(auto sky=Engine::Sky::FindSky(m_pipeline.Features())) {
                Engine::Sky::Settings settings;Engine::Sky::CelestialState celestial;
                if(!m_drag.Active()){result=m_skySystems.Update(m_world,ImGui::GetIO().DeltaTime,settings,celestial);if(!result)return result;}
                else {Engine::Guid a,b,c;result=Engine::Sky::ReadSettings(m_world,settings,a,b,c);if(!result)return result;celestial=Engine::Sky::EvaluateCelestials(settings.time,settings.sun,settings.moon);}
                m_skyElapsed+=ImGui::GetIO().DeltaTime;sky->Snapshot(settings,celestial,m_skyElapsed);
            }
#endif
            DrawUi();m_world.BeginFrame();ProcessInput();ImGui::Render();m_widgets.PolishDrawData(ImGui::GetDrawData());result=m_world.Extract(m_snapshot);if (!result) return result;ApplyPreview();
            if (exercise&&iteration>=140&&iteration<172) {
                auto index=static_cast<std::size_t>((iteration-140)/8);int phase=(iteration-140)%8;auto entries=m_pipeline.Features().Entries();
                if (index<entries.size()&&(phase==2||phase==6)) {
                    bool expected=phase==6;if(entries[index].enabled!=expected) return Engine::Result::Failure("Editor feature checkbox input failed");
                }
            }
            if (m_editorCamera.valid) {auto exposure=m_snapshot.camera; m_snapshot.camera=m_editorCamera; m_snapshot.camera.ev100=exposure.ev100;m_snapshot.camera.exposureCompensation=exposure.exposureCompensation;}
            auto& target=m_targets[m_frame.frameSlot];
            if (target.image) {
                m_pipeline.SetDebugView(static_cast<std::uint32_t>(m_view));m_pipeline.SetSelection(m_world.Contains(m_selected)?m_selected:Engine::Guid{});result=m_pipeline.RenderToTarget(m_snapshot,{target.image,target.width,target.height,m_frame.frameSlot});if (!result) return result;
                if (exercise) for (const auto& entry:m_pipeline.Features().Entries()) {
                    bool recorded=false;for (const auto& timing:m_pipeline.CpuPassTimes()) if (std::string_view(timing.first).starts_with(entry.feature->Name())) recorded=true;
                    auto name=std::string_view(entry.feature->Name());bool expected=entry.enabled&&(m_view==0||m_view>=5||name=="Debug Draw");if(name=="Environment Lighting")expected&=m_snapshot.settings.environment==1;if(name=="Height Fog")expected&=m_snapshot.settings.fog!=0&&m_snapshot.settings.density>0;if(name=="Auto Exposure")expected&=m_snapshot.settings.autoExposure!=0&&m_view==0;
                    if (recorded!=expected) return Engine::Result::Failure("Feature toggle did not change recorded passes");
                }
            }
            Engine::Renderer::RenderGraph uiGraph;auto color=uiGraph.ImportImage("Editor swapchain",m_frame.color,m_device.StateOf(m_frame.color));
            std::vector<Engine::Renderer::ResourceUse> uses{{color,Engine::Rhi::ResourceState::ColorWrite}};
            if (target.image) { auto viewport=uiGraph.ImportImage("Live scene image",target.image,m_device.StateOf(target.image));uses.push_back({viewport,Engine::Rhi::ResourceState::SampledRead}); }
            uiGraph.AddPass({"Editor UI",std::move(uses),[&](Engine::Rhi::CommandList&,const Engine::Renderer::PassResources& resources){return m_device.GuiDraw(ImGui::GetDrawData(),resources.Image(color));},{}});
            uiGraph.AddPass({"Present",{{color,Engine::Rhi::ResourceState::Present}},{},{}});Engine::Renderer::CompiledGraph plan;result=uiGraph.Compile(plan);if (!result) return result;
            Engine::Renderer::TransientImageCache unused(m_device);result=uiGraph.Execute(plan,m_device.Commands(),unused);if (!result) return result;
            m_uiCpuTimes.clear();for (std::size_t i=0;i<uiGraph.Passes().size();++i) m_uiCpuTimes.emplace_back(uiGraph.Passes()[i].name,uiGraph.CpuPassMilliseconds()[i]);
            result=m_device.EndFrame();if (!result) return result;ENGINE_PROFILE_FRAME();
            if (exercise&&target.image&&m_pipeline.LastInstanceCount()!=m_snapshot.instances.size()) return Engine::Result::Failure("Live viewport did not render extracted scene");
        } else m_window.PumpEvents(0.01);
        m_frameTimes[m_frameTimeIndex]=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-frameStarted).count();m_frameTimeIndex=(m_frameTimeIndex+1)%m_frameTimes.size();
        if (smokeFrames>0&&++iteration>=smokeFrames) m_window.RequestClose();else if (smokeFrames==0) ++iteration;
        if (smokeFrames>0&&std::chrono::steady_clock::now()-started>std::chrono::seconds(45)) return Engine::Result::Failure("Editor smoke timed out");
    }
    if(m_entityBenchmark){auto report=[&](std::vector<double> samples,const char* label){if(samples.empty())return;std::sort(samples.begin(),samples.end());double sum=0;for(auto v:samples)sum+=v;Engine::Log::Info(std::string("Entity benchmark ")+label+": samples="+std::to_string(samples.size())+", mean_ms="+std::to_string(sum/samples.size())+", p95_ms="+std::to_string(samples[(samples.size()-1)*95/100]));};report(m_hierarchyTimes,"hierarchy 100k");report(m_entityFrameTimes,"whole frame");}
    if (exercise) Engine::Log::Info("Editor smoke passed: live transform, undo/redo, save/load, debug views, feature toggles, resizing");return Engine::Result::Success();
}

