add_library(engine_core STATIC src/ModuleRegistry.cpp src/Log.cpp src/SpatialIndex.cpp)
target_include_directories(engine_core PUBLIC include)
target_link_libraries(engine_core PRIVATE spdlog::spdlog PUBLIC Tracy::TracyClient)
engine_target(engine_core)
