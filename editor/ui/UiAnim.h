#pragma once
#include <imgui.h>
#include <unordered_map>
namespace EditorUi {
class UiAnim {
public:
    void BeginFrame(float delta,int frame);
    float Ease(ImGuiID id,float target,float seconds=.1f);
    bool NeedsRedraw() const {return m_busy;}
private:
    struct State { float value=0;int seen=0; };
    std::unordered_map<ImGuiID,State> m_states;
    float m_delta=0;int m_frame=0;bool m_busy=false;
};
}
