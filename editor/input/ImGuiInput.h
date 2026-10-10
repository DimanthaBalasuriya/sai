#pragma once
#include "Input.h"
#include <optional>
struct GLFWwindow;
namespace EditorInput {
class ImGuiInput {
public:
    ~ImGuiInput();void Attach(GLFWwindow*);
    void Detach();
    Frame Capture(bool flying);
    void CapturePointer(bool enabled);
    bool ItemClicked() const;bool ItemDoubleClicked() const;
    void Inject(std::string_view chord,bool down); // Normal ImGui events for deterministic smoke checks.
    // Captured mouse bypasses ImGui; smoke supplies deterministic pointer samples
    // at this boundary while still exercising native cursor capture/release.
    void OverridePointerForSmoke(Frame frame) {m_smokePointer=std::move(frame);}
private:
    GLFWwindow* m_window=nullptr;void* m_previousUser=nullptr;
    void (*m_previousScroll)(GLFWwindow*,double,double)=nullptr;
    std::array<bool,3> m_buttons{};double m_scroll=0,m_lastX=0,m_lastY=0;bool m_captured=false;
    std::optional<Frame> m_smokePointer;
};
}
