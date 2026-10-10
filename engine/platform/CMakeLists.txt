add_library(engine_platform STATIC src/Window.cpp src/PlatformModule.cpp)
target_include_directories(engine_platform PUBLIC include)
target_link_libraries(engine_platform PUBLIC engine_core PRIVATE glfw)
engine_target(engine_platform)
