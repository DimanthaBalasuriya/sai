add_library(engine_rhi STATIC src/vulkan/VulkanDevice.cpp src/vulkan/Vma.cpp)
if(ENGINE_BUILD_EDITOR)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(ENGINE_GUI_SLANGC NAMES slangc HINTS "${PROJECT_SOURCE_DIR}/.tools/slang-2026.19/bin" "$ENV{VULKAN_SDK}/Bin" REQUIRED)
set(gui_spv "${CMAKE_CURRENT_BINARY_DIR}/GuiColor.spv")
set(gui_backend "${CMAKE_CURRENT_BINARY_DIR}/imgui_impl_vulkan.cpp")
add_custom_command(OUTPUT "${gui_spv}"
  COMMAND "${ENGINE_GUI_SLANGC}" "${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/GuiColor.slang" -target spirv -profile spirv_1_5 -entry main -stage fragment -emit-spirv-directly -o "${gui_spv}"
  DEPENDS src/vulkan/GuiColor.slang VERBATIM)
add_custom_command(OUTPUT "${gui_backend}"
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/cmake/PrepareGuiBackend.py" "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp" "${gui_spv}" "${gui_backend}"
  DEPENDS "${gui_spv}" "${PROJECT_SOURCE_DIR}/cmake/PrepareGuiBackend.py" "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp" VERBATIM)
target_sources(engine_rhi PRIVATE "${gui_backend}")
else()
target_sources(engine_rhi PRIVATE "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp")
endif()
target_include_directories(engine_rhi SYSTEM PRIVATE "${imgui_SOURCE_DIR}/backends")
target_include_directories(engine_rhi PUBLIC include)
target_compile_definitions(engine_rhi PRIVATE VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_USE_VOLK)
target_link_libraries(engine_rhi PUBLIC engine_core PRIVATE engine_platform glfw volk vk-bootstrap::vk-bootstrap GPUOpen::VulkanMemoryAllocator engine_imgui)
engine_target(engine_rhi)
