#include "UiAnim.h"
#include <algorithm>
#include <cmath>
namespace EditorUi {
void UiAnim::BeginFrame(float delta,int frame) {m_delta=std::clamp(delta,0.f,.1f);m_frame=frame;m_busy=false;if(frame%120==0) std::erase_if(m_states,[&](const auto& s){return frame-s.second.seen>600;});}
float UiAnim::Ease(ImGuiID id,float target,float seconds) {
    auto& state=m_states[id];state.seen=m_frame;
    state.value+=(target-state.value)*(1-std::exp(-m_delta*4.6f/std::max(seconds,.001f)));
    if(std::abs(state.value-target)<.001f) state.value=target;else m_busy=true;
    return state.value;
}
}
