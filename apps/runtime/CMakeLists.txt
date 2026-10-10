add_executable(engine_runtime src/Main.cpp src/DemoScene.cpp)
target_link_libraries(engine_runtime PRIVATE engine_builtin_features)
target_link_libraries(engine_runtime PRIVATE engine_platform engine_assets)
add_custom_target(engine_runtime_assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_SHADER_OUTPUT_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/shaders"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/content/samples" "${CMAKE_CURRENT_BINARY_DIR}/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/content/sky" "${CMAKE_CURRENT_BINARY_DIR}/sky"
  DEPENDS engine_shaders)
add_dependencies(engine_runtime engine_runtime_assets)
engine_target(engine_runtime)
