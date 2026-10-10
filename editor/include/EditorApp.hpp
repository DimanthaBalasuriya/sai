#pragma once
#include "engine/assets/Registry.hpp"
#include "engine/extensions/Builtins.hpp"
#include "engine/scene/Serialization.hpp"
#include "engine/platform/Window.hpp"
#include <functional>
#include "EditorPanelRegistry.hpp"
#include "Widgets.h"
#include "ImGuiInput.h"
#include "Camera.h"
#include "SceneTools.h"
#include "HierarchyModel.h"
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
#include "engine/sky/GpuSky.hpp"
#endif
// Editor owns UI/targets; engine services are borrowed and outlive the editor.
class EditorApp {
public:
    EditorApp(Engine::Rhi::Device& device,Engine::Window& window,Engine::Scene::World& world,Engine::Assets::Registry& registry,std::filesystem::path directory);
    ~EditorApp();
    Engine::Result Initialize(const std::filesystem::path& source,bool smoke);
    Engine::Result Run(int smokeFrames,bool exercise);
    void ConfigureInput(std::filesystem::path path,bool smoke) {m_bindingsPath=std::move(path);m_inputSmoke=smoke;}
    void ConfigureEntityBenchmark(bool enabled){m_entityBenchmark=enabled;}
    void ConfigureUi(std::filesystem::path theme,float dpi,bool themePanel,bool uiSmoke=false,std::string panel={}) {m_themePath=std::move(theme);m_dpiOverride=dpi;m_showTheme=themePanel;m_uiSmoke=uiSmoke;m_focusPanel=panel.empty()&&themePanel?"Theme":std::move(panel);}
private:
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
    Engine::Sky::SceneSystems m_skySystems;double m_skyElapsed=0;void SkyPanel();
#endif
    struct ViewportTarget { Engine::Rhi::ImageHandle image;std::uint64_t texture=0;std::uint32_t width=0,height=0; };
    void CreateLight(Engine::Scene::ComponentType);
    void CreateLightingSettings(Engine::Scene::ComponentType);
    void DrawUi();void Hierarchy();void Inspector();void Assets();void Console();void Statistics();void Viewport();void RenderFeatures();
    void Report(Engine::Result result,std::string_view success={});
    void Save();void Load();void LoadNow();void ImportNow();
    Engine::Result SmokeStep(int frame);
    Engine::Result ResetCamera();
    void ProcessInput();void DrawGizmo();void ApplyPreview();void FocusSelection();void TogglePlay();void CancelInteraction();
    Engine::Guid DrawEntityIcons(glm::vec2 mouse);
    std::vector<Engine::Guid> m_iconEntities;
    bool m_showEntityIcons=true;
    bool m_showLightBounds=true,m_lightHandleActive=false,m_lightHandleMerged=false;
    Engine::Scene::ComponentType m_lightHandleType{};std::string m_lightHandleField;Engine::Guid m_lightHandleEntity;
    glm::vec2 m_lightHandleStart{},m_lightHandleAxis{};float m_lightHandleValue=0,m_lightHandleScale=1;
    Engine::Result InputSmokeStep(int frame);void InjectInputSmoke(int frame);
    std::size_t m_inputSmokeCount=0,m_inputSmokeBranch=0;Engine::Guid m_inputSmokeSelected;std::uint64_t m_inputSmokeRevision=0;
    std::vector<Engine::Guid> m_inputSmokeOriginal;
    glm::vec3 m_inputSmokeEye{0};float m_inputSmokeSpeed=0;
    std::string m_inputSmokeName;
    EditorInput::Actions m_actions;EditorInput::ImGuiInput m_inputBridge;EditorInput::Frame m_inputFrame;
    EditorInput::Camera m_camera;EditorTools::Selection m_selection;EditorTools::Geometry m_geometry;
    EditorTools::TransformDrag m_drag;EditorTools::Visibility m_visibility;
    std::filesystem::path m_bindingsPath;bool m_inputSmoke=false,m_flying=false,m_blockGizmo=false,m_blockLook=false,m_orbiting=false,m_panning=false;
    bool m_viewHovered=false,m_viewFocused=false,m_hierarchyHovered=false,m_hierarchyFocused=false,m_selecting=false,m_box=false,m_addSelection=false,m_toggleSelection=false;
    glm::vec2 m_viewPosition{0},m_viewSize{0},m_selectStart{0};glm::mat4 m_viewProjection{1};ImDrawList* m_viewDrawList=nullptr;
    Engine::Guid m_hierarchyClick,m_renameEntity;std::array<char,512> m_renameName{};bool m_renameStarted=false,m_renameCommit=false;
    Engine::Guid m_pressHit;
    int m_pivotMode=0;float m_gridSnap=.5f,m_angleSnap=15,m_scaleSnap=.1f;std::uint64_t m_previewRevision=0;
    std::vector<Engine::Scene::EntityDefinition> m_clipboard,m_playDefinitions;bool m_play=false;
    std::vector<Engine::Guid> m_editHidden,m_editSelection;Engine::Scene::CommandHistory m_editHistory;
    Engine::Result RefreshUi(bool force=false);void ThemePanel();void ComponentAction(Engine::Scene::ComponentType,bool remove);
    EditorUi::ThemeTokens m_theme;EditorUi::Fonts m_fonts;EditorUi::UiAnim m_anim;EditorUi::Widgets m_widgets{m_theme,m_fonts,m_anim};
    std::filesystem::path m_themePath;std::filesystem::file_time_type m_themeTime{};
    float m_dpi=0,m_dpiOverride=0,m_fontSize=0,m_monoSize=0;bool m_themeChanged=true,m_showTheme=false,m_uiSmoke=false;
    Engine::Scene::EntityDefinition m_uiSmokeEntity;
    std::array<char,256> m_search{},m_consoleSearch{};
    std::string m_focusPanel;
    Engine::Rhi::Device& m_device;Engine::Window& m_window;Engine::Scene::World& m_world;Engine::Assets::Registry& m_registry;
    std::filesystem::path m_directory;Engine::Extensions::FeaturePipeline m_pipeline;
    Engine::Scene::CommandHistory m_history;std::vector<Engine::Guid> m_assetScenes;
    Engine::Guid m_selected;std::vector<Engine::Guid> m_entities;
    EditorTools::HierarchyModel m_hierarchyModel;std::uint64_t m_entityListVersion=0,m_inspectorVersion=0;
    Engine::Guid m_inspectorSelected;
    struct CachedField {Engine::Scene::ComponentType type;std::string name;Engine::Scene::FieldValue value;};
    std::vector<CachedField> m_inspectorFields;
    EditorPanelRegistry m_panels;
    std::vector<std::pair<std::string,double>> m_uiCpuTimes;
    std::array<ViewportTarget,Engine::Rhi::kFramesInFlight> m_targets{};std::vector<ViewportTarget> m_retired;
    Engine::Rhi::FrameInfo m_frame;Engine::Renderer::RenderWorld m_snapshot;
    Engine::Renderer::RenderWorld m_inputSnapshot;
    bool m_entityBenchmark=false;std::vector<double> m_hierarchyTimes,m_entityFrameTimes;
    Engine::Renderer::RenderCamera m_editorCamera;
    std::array<char,1024> m_importPath{},m_scenePath{};
    std::string m_iniPath;
    std::array<char,256> m_gpuOverride{};bool m_gpuSettingLoaded=false;
    std::array<float,120> m_frameTimes{};std::size_t m_frameTimeIndex=0;
    int m_operation=0,m_mode=0,m_view=0;bool m_uiReady=false,m_layoutReady=false;
    bool m_pendingImport=false,m_pendingLoad=false,m_contextReady=false,m_inputReady=false;
    std::string m_lastEdit;
    std::string m_actionError;
    std::vector<std::array<float,2>> m_featureCenters;
    Engine::Scene::Transform m_smokeBefore;
};
