add_library(engine_lighting STATIC LightingModule.cpp components/Components.cpp components/DirectionalLight.cpp components/PointLight.cpp components/SpotLight.cpp render/LightExtract.cpp render/Presets.cpp render/ShadowMath.cpp render/ShadowMaps.cpp)
target_include_directories(engine_lighting PUBLIC include)
target_link_libraries(engine_lighting PUBLIC engine_scene engine_renderer engine_extensions glm::glm)
engine_target(engine_lighting)
target_sources(engine_lighting PRIVATE components/RectLight.cpp components/DiskLight.cpp render/LtcTables.cpp components/Settings.cpp render/ExponentialHeightFog.cpp render/AutoExposure.cpp render/HdriEnvironment.cpp)
target_include_directories(engine_lighting PRIVATE "${stb_SOURCE_DIR}")
target_link_libraries(engine_builtin_features PUBLIC engine_lighting)
file(GLOB lighting_shader_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.slang")
set(lighting_shader_outputs)
foreach(shader cluster exposure environment)
  set(output "${ENGINE_SHADER_OUTPUT_DIR}/extensions/${shader}.compute.spv")
  add_custom_command(OUTPUT "${output}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ENGINE_SHADER_OUTPUT_DIR}/extensions"
    COMMAND "${ENGINE_SLANGC}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}.slang" -target spirv -profile spirv_1_5 -entry ComputeMain -stage compute -matrix-layout-column-major -emit-spirv-directly -o "${output}"
    DEPENDS ${lighting_shader_sources} VERBATIM)
  list(APPEND lighting_shader_outputs "${output}")
endforeach()
add_custom_target(engine_lighting_shaders DEPENDS ${lighting_shader_outputs})
add_dependencies(engine_shaders engine_lighting_shaders)
foreach(stage vertex fragment)
  if(stage STREQUAL "vertex")
    set(entry VertexMain)
  else()
    set(entry FragmentMain)
  endif()
  set(output "${ENGINE_SHADER_OUTPUT_DIR}/extensions/shadow.${stage}.spv")
  add_custom_command(OUTPUT "${output}"
    COMMAND "${ENGINE_SLANGC}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.slang" -target spirv -profile spirv_1_5 -entry ${entry} -stage ${stage} -matrix-layout-column-major -emit-spirv-directly -o "${output}"
    DEPENDS ${lighting_shader_sources} VERBATIM)
  list(APPEND shadow_outputs "${output}")
endforeach()
add_custom_target(engine_shadow_shaders DEPENDS ${shadow_outputs})
add_dependencies(engine_shaders engine_shadow_shaders)
set(output "${ENGINE_SHADER_OUTPUT_DIR}/extensions/heightfog.fragment.spv")
add_custom_command(OUTPUT "${output}"
  COMMAND "${ENGINE_SLANGC}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/heightfog.slang" -target spirv -profile spirv_1_5 -entry FragmentMain -stage fragment -matrix-layout-column-major -emit-spirv-directly -o "${output}"
  DEPENDS ${lighting_shader_sources} "${PROJECT_SOURCE_DIR}/engine/extensions/shaders/fullscreen.slang" VERBATIM)
add_custom_target(engine_fog_shaders DEPENDS "${output}")
add_dependencies(engine_shaders engine_fog_shaders)
set(atlas_output "${ENGINE_SHADER_OUTPUT_DIR}/extensions/shadowatlas.fragment.spv")
add_custom_command(OUTPUT "${atlas_output}"
  COMMAND "${ENGINE_SLANGC}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadowatlas.slang" -target spirv -profile spirv_1_5 -entry FragmentMain -stage fragment -matrix-layout-column-major -emit-spirv-directly -o "${atlas_output}"
  DEPENDS ${lighting_shader_sources} "${PROJECT_SOURCE_DIR}/engine/extensions/shaders/fullscreen.slang" VERBATIM)
add_custom_target(engine_shadow_debug_shaders DEPENDS "${atlas_output}")
add_dependencies(engine_shaders engine_shadow_debug_shaders)
