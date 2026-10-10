#pragma once
#include "engine/core/Result.hpp"
struct GLFWwindow;
namespace Engine {
struct WindowExtent { int width; int height; };
// Owns native window; main-thread only, requires an active PlatformModule.
class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Result Open(int width, int height, const char* title);
    bool ShouldClose() const noexcept;
    void RequestClose() noexcept;
    WindowExtent FramebufferExtent() const noexcept;
    void PumpEvents(double timeoutSeconds) const noexcept;
    void PollEvents() const noexcept;
    void Resize(int width, int height) noexcept;
    void Minimize() noexcept;
    void Restore() noexcept;
    bool IsMinimized() const noexcept;
    // Borrowed GLFW handle for backend surface creation; valid until destruction.
    void* NativeHandle() const noexcept { return m_window; }
private:
    GLFWwindow* m_window = nullptr;
};
}
