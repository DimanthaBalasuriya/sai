#include "engine/platform/PlatformModule.hpp"
#include "engine/core/Log.hpp"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
namespace Engine {
Result PlatformModule::OnStartup(EngineContext&) noexcept {
    glfwSetErrorCallback([](int, const char* message) { Log::Error(message); });
    if (!glfwInit()) return Result::Failure("GLFW initialization failed");
    m_initialized = true;
    Log::Info("Platform started (GLFW)");
    return Result::Success();
}
void PlatformModule::OnShutdown(EngineContext&) noexcept {
    if (m_initialized) {
        glfwTerminate();
        m_initialized = false;
        Log::Info("Platform stopped");
    }
}
}
