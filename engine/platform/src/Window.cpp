#include "engine/platform/Window.hpp"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
namespace Engine {
Window::~Window() { if (m_window) glfwDestroyWindow(m_window); }
Result Window::Open(int width, int height, const char* title) {
    if (m_window) return Result::Failure("Window is already open");
    if (width <= 0 || height <= 0 || !title) return Result::Failure("Invalid window description");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) return Result::Failure("Window creation failed");
    glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int, int action, int) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GLFW_TRUE);
    });
    return Result::Success();
}
bool Window::ShouldClose() const noexcept { return !m_window || glfwWindowShouldClose(m_window); }
void Window::RequestClose() noexcept { if (m_window) glfwSetWindowShouldClose(m_window, GLFW_TRUE); }
WindowExtent Window::FramebufferExtent() const noexcept {
    WindowExtent extent{};
    if (m_window) glfwGetFramebufferSize(m_window, &extent.width, &extent.height);
    return extent;
}
void Window::PumpEvents(double timeoutSeconds) const noexcept { glfwWaitEventsTimeout(timeoutSeconds); }
void Window::PollEvents() const noexcept { glfwPollEvents(); }
void Window::Resize(int width, int height) noexcept { if (m_window) glfwSetWindowSize(m_window, width, height); }
void Window::Minimize() noexcept { if (m_window) glfwIconifyWindow(m_window); }
void Window::Restore() noexcept { if (m_window) glfwRestoreWindow(m_window); }
bool Window::IsMinimized() const noexcept { return m_window && glfwGetWindowAttrib(m_window, GLFW_ICONIFIED); }
}
