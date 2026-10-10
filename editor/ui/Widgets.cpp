#include "Widgets.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cstring>
namespace EditorUi {
namespace { ImVec4 Blend(ImVec4 a,ImVec4 b,float t) {return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};} }
bool Widgets::PropertyRow(const char* label,const std::function<bool()>& widget) {
    ImGui::PushID(label);bool changed=false;ImGuiLastItemData last;
    if(ImGui::BeginTable("property",2,ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("label",ImGuiTableColumnFlags_WidthFixed,std::min(110*dpi,ImGui::GetContentRegionAvail().x*.32f));ImGui::TableSetupColumn("value",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();ImGui::TextColored(Token("text-muted"),"%s",label);ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);
        changed=widget();last=GImGui->LastItemData;ImGui::EndTable();GImGui->LastItemData=last;
    }ImGui::PopID();return changed;
}
bool Widgets::DragVec3(const char* label,Engine::Vec3& v,float minimum,float maximum,Engine::Vec3 reset) {
    return PropertyRow(label,[&]{
        float* values[]{&v.x,&v.y,&v.z};float defaults[]{reset.x,reset.y,reset.z};const char* axes[]{"X","Y","Z"};const char* colors[]{"negative","positive","info"};
        const float available=ImGui::GetContentRegionAvail().x;
        ImGui::PushFont(fonts.mono);float minimumWidth=14*dpi+ImGui::CalcTextSize("-0.000").x+2*ImGui::GetStyle().FramePadding.x;ImGui::PopFont();
        bool stacked=available<minimumWidth*3+2*ImGui::GetStyle().ItemInnerSpacing.x;
        const float width=stacked?available:(available-2*ImGui::GetStyle().ItemInnerSpacing.x)/3;
        bool changed=false;ImGuiLastItemData active;bool haveActive=false;
        ImGui::PushFont(fonts.mono);
        for(int i=0;i<3;++i) {
            if(i&&!stacked) ImGui::SameLine(0,ImGui::GetStyle().ItemInnerSpacing.x);ImGui::PushID(i);
            ImGui::BeginGroup();ImGui::PushStyleColor(ImGuiCol_Text,Token(colors[i]));ImGui::Selectable(axes[i],false,ImGuiSelectableFlags_AllowDoubleClick,{14*dpi,ImGui::GetFrameHeight()});
            if(ImGui::IsItemHovered()&&ImGui::IsMouseDoubleClicked(0)) {*values[i]=std::clamp(defaults[i],minimum,maximum);changed=true;active=GImGui->LastItemData;haveActive=true;}
            Tooltip("Double-click to reset this axis");ImGui::PopStyleColor();ImGui::SameLine(0,0);ImGui::SetNextItemWidth(std::max(20*dpi,width-14*dpi));
            bool edited=ImGui::DragFloat("##axis",values[i],maximum>minimum&&maximum-minimum<.1f?(maximum-minimum)*.0001f:.02f,minimum,maximum,maximum-minimum<.1f?"%.6g":"%.3f");changed|=edited;
            if(edited||ImGui::IsItemActive()||ImGui::IsItemDeactivated()) {active=GImGui->LastItemData;haveActive=true;}
            ImGui::EndGroup();ImGui::PopID();
        }ImGui::PopFont();if(haveActive) GImGui->LastItemData=active;return changed;
    });
}
bool Widgets::ComponentHeader(const char* name,const char* icon,bool* enabled,const std::function<void()>& menu) {
    std::string label=std::string(icon)+"  "+name;auto p=ImGui::GetCursorScreenPos();
    float t=anim.Ease(ImGui::GetID(name)^0xcafeU,ImGui::IsMouseHoveringRect(p,{p.x+ImGui::GetContentRegionAvail().x,p.y+ImGui::GetFrameHeight()})?1.f:0.f,theme.metrics.at("hover-seconds"));
    auto bg=Blend(Token("info"),Token("border"),t);float alpha=theme.metrics.at("selected-alpha");bg.w=alpha+(1-alpha)*t;ImGui::PushStyleColor(ImGuiCol_Header,bg);ImGui::PushStyleColor(ImGuiCol_HeaderHovered,bg);
    bool open=ImGui::CollapsingHeader(label.c_str(),ImGuiTreeNodeFlags_DefaultOpen);ImGui::PopStyleColor(2);
    if(ImGui::BeginPopupContextItem()) {menu();ImGui::EndPopup();}
    if(enabled) {ImGui::PushID(name);ToggleSwitch("Enabled",enabled);ImGui::PopID();}return open;
}
bool Widgets::IconButton(const char* label,ImVec2 size) {
    auto id=ImGui::GetID(label);auto text=ImGui::CalcTextSize(label,nullptr,true);auto padding=ImGui::GetStyle().FramePadding;
    if(size.x<=0) size.x=text.x+padding.x*2;if(size.y<=0) size.y=text.y+padding.y*2;
    auto position=ImGui::GetCursorScreenPos();bool clicked=ImGui::InvisibleButton(label,size,ImGuiButtonFlags_EnableNav);
    float blend=anim.Ease(id,ImGui::IsItemActive()?2.f:ImGui::IsItemHovered()?1.f:0.f,theme.metrics.at("hover-seconds"));
    auto color=blend<=1?Blend(Token("btn"),Token("btn-hover"),blend):Blend(Token("btn-hover"),Token("btn-active"),blend-1);
    auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(position,{position.x+size.x,position.y+size.y},ImGui::GetColorU32(color),ImGui::GetStyle().FrameRounding);
    const char* end=ImGui::FindRenderedTextEnd(label);draw->AddText({position.x+padding.x,position.y+padding.y},ImGui::GetColorU32(ImGuiCol_Text),label,end);
    if(ImGui::IsItemFocused()) draw->AddRect(position,{position.x+size.x,position.y+size.y},ImGui::GetColorU32(Token("info")),3*dpi);
    return clicked;
}
bool Widgets::ToggleSwitch(const char* label,bool* value) {
    auto id=ImGui::GetID(label);float height=ImGui::GetFrameHeight(),width=height*1.7f;auto p=ImGui::GetCursorScreenPos();
    bool clicked=ImGui::InvisibleButton(label,{width+8*dpi+ImGui::CalcTextSize(label).x,height},ImGuiButtonFlags_EnableNav);if(clicked) *value=!*value;
    float t=anim.Ease(id,*value?1.f:0.f,theme.metrics.at("hover-seconds"));auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(p,{p.x+width,p.y+height},ImGui::GetColorU32(Blend(Token("scrollbar"),Token("info"),t)),height*.5f);
    draw->AddCircleFilled({p.x+height*.5f+t*(width-height),p.y+height*.5f},height*.5f-3*dpi,ImGui::GetColorU32(Token("text-primary")));
    draw->AddText({p.x+width+8*dpi,p.y+ImGui::GetStyle().FramePadding.y},ImGui::GetColorU32(ImGuiCol_Text),label);return clicked;
}
bool Widgets::SearchField(const char* label,char* buffer,std::size_t capacity) {
    ImGui::PushID(label);ImGui::SetNextItemWidth(std::max(20*dpi,ImGui::GetContentRegionAvail().x-30*dpi));bool changed=ImGui::InputTextWithHint("##search",label,buffer,capacity);
    ImGui::SameLine();if(IconButton("x##clear",{24*dpi,0})) {buffer[0]=0;changed=true;}Tooltip("Clear search");ImGui::PopID();return changed;
}
bool Widgets::TreeRow(const char* label,ImGuiTreeNodeFlags flags) {
    auto id=ImGui::GetID(label);auto p=ImGui::GetCursorScreenPos();float t=anim.Ease(id,ImGui::IsMouseHoveringRect(p,{p.x+ImGui::GetContentRegionAvail().x,p.y+ImGui::GetFrameHeight()})?1.f:0.f,theme.metrics.at("hover-seconds"));
    auto bg=Blend(Token("bg-surface"),Token("border"),t);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,bg);
    if(!(flags&ImGuiTreeNodeFlags_Selected)&&t>.001f) ImGui::GetWindowDrawList()->AddRectFilled(p,{p.x+ImGui::GetContentRegionAvail().x,p.y+ImGui::GetFrameHeight()},ImGui::GetColorU32(bg));
    bool open=ImGui::TreeNodeEx(label,flags);ImGui::PopStyleColor();return open;
}
void Widgets::StatusChip(const char* label,const char* token) {
    auto p=ImGui::GetCursorScreenPos();float height=ImGui::GetTextLineHeight();ImGui::GetWindowDrawList()->AddCircleFilled({p.x+4*dpi,p.y+height*.5f},3*dpi,ImGui::GetColorU32(Token(token)));ImGui::Dummy({12*dpi,height});ImGui::SameLine(0,0);ImGui::TextColored(Token("text-secondary"),"%s",label);
}
void Widgets::Tooltip(const char* text) {
    auto id=GImGui->LastItemData.ID;bool hovered=ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal);float t=anim.Ease(id^0x71abU,hovered?1.f:0.f);
    if(hovered) {ImGui::PushStyleVar(ImGuiStyleVar_Alpha,t);ImGui::BeginTooltip();ImGui::TextUnformatted(text);ImGui::EndTooltip();ImGui::PopStyleVar();}
}
void Widgets::PolishDrawData(ImDrawData* data) {
    // Dock tabs are rendered by ImGui itself. Recolor only their background vertices;
    // text, active overlines and unrelated geometry retain their own token colors.
    for(auto* window:GImGui->Windows) if(window->DockNode&&window->DockNode->HostWindow==window&&window->DockNode->TabBar) {
        auto* bar=window->DockNode->TabBar;
        bool focused=(bar->Flags&ImGuiTabBarFlags_IsFocused)!=0;
        for(auto& tab:bar->Tabs) {
            bool selected=bar->SelectedTabId==tab.ID;ImVec2 lo{bar->BarRect.Min.x+tab.Offset,bar->BarRect.Min.y},hi{lo.x+tab.Width,bar->BarRect.Max.y};
            bool hovered=ImGui::IsMouseHoveringRect(lo,hi,false);float t=anim.Ease(tab.ID^0x7abU,hovered?1.f:0.f,theme.metrics.at("hover-seconds"));
            auto base=focused?(selected?ImGuiCol_TabSelected:ImGuiCol_Tab):(selected?ImGuiCol_TabDimmedSelected:ImGuiCol_TabDimmed);auto color=ImGui::GetColorU32(Blend(ImGui::GetStyle().Colors[base],Token("border"),t));
            for(auto* list:data->CmdLists) if(list==window->DrawList) for(auto& v:list->VtxBuffer) if(v.pos.x>=lo.x&&v.pos.x<=hi.x&&v.pos.y>=lo.y&&v.pos.y<=hi.y) {
                auto rgb=v.col&0x00ffffffU;
                if(rgb==(ImGui::GetColorU32(base)&0x00ffffffU)||rgb==(ImGui::GetColorU32(ImGuiCol_TabHovered)&0x00ffffffU)||rgb==(ImGui::GetColorU32(ImGuiCol_TabDimmed)&0x00ffffffU)||rgb==(ImGui::GetColorU32(ImGuiCol_TabDimmedSelected)&0x00ffffffU)) v.col=(v.col&0xff000000U)|(color&0x00ffffffU);
                else if(!focused&&rgb==(ImGui::GetColorU32(ImGuiCol_Text)&0xffffffU)) v.col=(v.col&0xff000000U)|(ImGui::GetColorU32(Token("text-muted"))&0xffffffU);
            }
        }
    }
    for(auto* window:GImGui->Windows) if(window->Active&&(window->Flags&ImGuiWindowFlags_Popup)&&!(window->Flags&ImGuiWindowFlags_Tooltip)) {
        float t=anim.Ease(window->ID^0xfadeU,1.f); // Reset closed popup state below.
        for(auto* list:data->CmdLists) if(list==window->DrawList) for(auto& v:list->VtxBuffer) v.col=(v.col&0xffffffU)|(static_cast<ImU32>(float(v.col>>24)*t)<<24);
    } else if(window->Flags&ImGuiWindowFlags_Popup) anim.Ease(window->ID^0xfadeU,0.f);
}
}
