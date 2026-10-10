add_library(engine_sky STATIC components/Components.cpp systems/TimeOfDaySystem.cpp systems/WeatherSystem.cpp layers/LayerStack.cpp layers/Atmosphere.cpp layers/Celestial.cpp layers/Stars.cpp layers/Clouds.cpp layers/Aerial.cpp layers/Cubemap.cpp layers/Aurora.cpp SkyModule.cpp GpuSky.cpp)
target_include_directories(engine_sky PUBLIC include)
target_link_libraries(engine_sky PUBLIC engine_scene engine_lighting engine_extensions glm::glm PRIVATE nlohmann_json::nlohmann_json)
target_compile_definitions(engine_sky PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS GLM_ENABLE_EXPERIMENTAL)
engine_target(engine_sky)
target_link_libraries(engine_builtin_features PUBLIC engine_sky)
target_compile_definitions(engine_builtin_features PUBLIC ENGINE_FEATURE_PROCEDURAL_SKY)
file(GLOB sky_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.slang")
set(sky_outputs)
foreach(layer Atmosphere Celestial Stars Clouds Aerial Cubemap Aurora)
  set(output "${ENGINE_SHADER_OUTPUT_DIR}/extensions/${layer}.fragment.spv")
  add_custom_command(OUTPUT "${output}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ENGINE_SHADER_OUTPUT_DIR}/extensions"
    COMMAND "${ENGINE_SLANGC}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${layer}.slang" -target spirv -profile spirv_1_5 -entry FragmentMain -stage fragment -matrix-layout-column-major -emit-spirv-directly -o "${output}"
    DEPENDS ${sky_sources} VERBATIM)
  list(APPEND sky_outputs "${output}")
endforeach()
foreach(mode RANGE 0 10)
  set(output "${ENGINE_SHADER_OUTPUT_DIR}/extensions/sky${mode}.compute.spv")
  add_custom_command(OUTPUT "${output}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ENGINE_SHADER_OUTPUT_DIR}/extensions"
    COMMAND "${ENGINE_SLANGC}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compute.slang" -DENGINE_SKY_COMPUTE_MODE=${mode} -target spirv -profile spirv_1_5 -entry ComputeMain -stage compute -matrix-layout-column-major -emit-spirv-directly -o "${output}"
    DEPENDS ${sky_sources} VERBATIM)
  list(APPEND sky_outputs "${output}")
endforeach()
add_custom_target(engine_sky_shaders DEPENDS ${sky_outputs})
add_dependencies(engine_shaders engine_sky_shaders)
set(rmse_output "${ENGINE_SHADER_OUTPUT_DIR}/extensions/sky-rmse.compute.spv")
add_custom_command(OUTPUT "${rmse_output}"
  COMMAND "${ENGINE_SLANGC}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/rmse.slang" -target spirv -profile spirv_1_5 -entry ComputeMain -stage compute -matrix-layout-column-major -emit-spirv-directly -o "${rmse_output}"
  DEPENDS ${sky_sources} VERBATIM)
add_custom_target(engine_sky_reference_shaders DEPENDS "${rmse_output}")
add_dependencies(engine_shaders engine_sky_reference_shaders)
target_compile_definitions(engine_sky PUBLIC ENGINE_SKY_PRESETS_PATH="${PROJECT_SOURCE_DIR}/content/sky/weather-presets.json")
