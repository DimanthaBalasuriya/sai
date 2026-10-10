add_library(engine_skybox STATIC src/Skybox.cpp)
target_include_directories(engine_skybox PUBLIC include)
target_link_libraries(engine_skybox PUBLIC engine_extensions)
engine_target(engine_skybox)
