#pragma once
#include "Fonts.h"
#include "Theme.h"
#include "UiAnim.h"
#include "engine/core/Math.hpp"
#include <functional>
namespace EditorUi {
class Widgets {
public:
    Widgets(ThemeTokens& t,Fonts& f,UiAnim& a):theme(t),fonts(f),anim(a){}
    ThemeTokens& theme;Fonts& fonts;UiAnim& anim;bool linear=false;float dpi=1;
    ImVec4 Token(const char* name) const {return Color(theme,name,linear);}
    bool PropertyRow(const char*,const std::function<bool()>&);
    bool DragVec3(const char*,Engine::Vec3&,float minimum,float maximum,Engine::Vec3 reset={});
    bool ComponentHeader(const char* name,const char* icon,bool* enabled,const std::function<void()>& menu);
    bool IconButton(const char* label,ImVec2 size={});
    bool ToggleSwitch(const char* label,bool* value);
    bool SearchField(const char* label,char* buffer,std::size_t capacity);
    bool TreeRow(const char* label,ImGuiTreeNodeFlags flags);
    void StatusChip(const char* label,const char* token);
    void Tooltip(const char* text);
    void PolishDrawData(ImDrawData* data);
};
}
