add_library(editor_ui STATIC ui/Theme.cpp ui/Fonts.cpp ui/Widgets.cpp ui/UiAnim.cpp)
target_include_directories(editor_ui PUBLIC ui)
target_link_libraries(editor_ui PUBLIC engine_core engine_imgui PRIVATE nlohmann_json::nlohmann_json)
engine_target(editor_ui)
add_library(editor_input STATIC input/Input.cpp input/ImGuiInput.cpp input/Camera.cpp)
target_include_directories(editor_input PUBLIC input)
target_link_libraries(editor_input PUBLIC engine_renderer engine_imgui glm::glm PRIVATE glfw nlohmann_json::nlohmann_json)
target_compile_definitions(editor_input PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS)
engine_target(editor_input)
add_library(editor_scene_tools STATIC tools/SceneTools.cpp tools/HierarchyModel.cpp)
target_include_directories(editor_scene_tools PUBLIC tools)
target_link_libraries(editor_scene_tools PUBLIC engine_scene glm::glm)
target_compile_definitions(editor_scene_tools PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS GLM_ENABLE_EXPERIMENTAL)
engine_target(editor_scene_tools)
add_executable(engine_editor src/Main.cpp src/EditorApp.cpp src/EditorControls.cpp src/EntityIcons.cpp src/InputSmoke.cpp src/SkyPanel.cpp "${PROJECT_SOURCE_DIR}/apps/runtime/src/DemoScene.cpp" "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp")
target_include_directories(engine_editor PRIVATE include "${PROJECT_SOURCE_DIR}/apps/runtime/src")
target_include_directories(engine_editor SYSTEM PRIVATE "${imgui_SOURCE_DIR}/backends")
target_link_libraries(engine_editor PRIVATE engine_platform engine_assets engine_imgui engine_imguizmo glfw glm::glm)
target_link_libraries(engine_editor PRIVATE engine_builtin_features)
target_link_libraries(engine_editor PRIVATE editor_ui editor_input editor_scene_tools)
target_compile_definitions(engine_editor PRIVATE ENGINE_EDITOR_BINDINGS_PATH="${PROJECT_SOURCE_DIR}/editor/input/bindings.json")
target_compile_definitions(engine_editor PRIVATE ENGINE_EDITOR_THEME_PATH="${PROJECT_SOURCE_DIR}/editor/ui/theme.json")
target_compile_definitions(engine_editor PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS GLM_ENABLE_EXPERIMENTAL)
add_custom_target(engine_editor_assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_SHADER_OUTPUT_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/shaders"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/content/samples" "${CMAKE_CURRENT_BINARY_DIR}/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/editor/ui/fonts" "${CMAKE_CURRENT_BINARY_DIR}/ui/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/editor/ui/theme.json" "${CMAKE_CURRENT_BINARY_DIR}/ui/theme.json"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/input"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/editor/input/bindings.json" "${CMAKE_CURRENT_BINARY_DIR}/input/bindings.json"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/content/sky" "${CMAKE_CURRENT_BINARY_DIR}/sky"
  DEPENDS engine_shaders)
add_dependencies(engine_editor engine_editor_assets)
engine_target(engine_editor)

