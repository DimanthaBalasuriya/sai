#include "Theme.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>
#include <charconv>
#include <cstdio>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
namespace EditorUi {
namespace {
constexpr const char* colors[]{"bg-app","bg-surface","bg-raised","border","scrollbar","btn","btn-hover","btn-active","text-primary","text-secondary","text-muted","text-dim","positive","info","negative","warning","solana","scrollbar-hover","scrollbar-active","border-light","info-hover"};
constexpr const char* metrics[]{"window-padding-x","window-padding-y","frame-padding-x","frame-padding-y","item-spacing-x","item-spacing-y","inner-spacing-x","inner-spacing-y","indent","window-rounding","frame-rounding","popup-rounding","scrollbar-rounding","grab-rounding","tab-rounding","window-border","frame-border","popup-border","scrollbar-size","grab-min","dock-separator","font-size","mono-size","hover-seconds","tooltip-delay","selected-alpha","active-alpha","dock-preview-alpha","resize-hover-alpha","resize-active-alpha","text-selection-alpha","table-alt-alpha","modal-alpha"};
float Linear(float v) { return v<=0.04045f?v/12.92f:std::pow((v+0.055f)/1.055f,2.4f); }
}
Engine::Result LoadTheme(const std::filesystem::path& path,ThemeTokens& output) {
    try {
        std::ifstream file(path);if (!file) return Engine::Result::Failure("Cannot open theme: "+path.string());
        nlohmann::json json;file>>json;if(json.at("version")!=1) return Engine::Result::Failure("Unsupported theme version");
        ThemeTokens next;
        for(auto name:colors) {
            auto hex=json.at("colors").at(name).get<std::string>();unsigned value=0;
            if(hex.size()!=7||hex[0]!='#') return Engine::Result::Failure("Expected #RRGGBB for theme token "+std::string(name));
            auto [end,error]=std::from_chars(hex.data()+1,hex.data()+7,value,16);
            if(error!=std::errc{}||end!=hex.data()+7) return Engine::Result::Failure("Invalid hex theme token "+std::string(name));
            next.colors[name]={float((value>>16)&255)/255,float((value>>8)&255)/255,float(value&255)/255,1};
        }
        for(auto name:metrics) { auto v=json.at("metrics").at(name).get<float>();if(!std::isfinite(v)||v<0||v>128||(std::string_view(name).ends_with("-alpha")&&v>1)) return Engine::Result::Failure("Invalid theme metric "+std::string(name));next.metrics[name]=v; }
        if(next.metrics.at("font-size")<8||next.metrics.at("mono-size")<8||next.metrics.at("hover-seconds")<=0||next.metrics.at("tooltip-delay")>5) return Engine::Result::Failure("Invalid font or animation metric");
        output=std::move(next);return Engine::Result::Success();
    }catch(const std::exception& error) { return Engine::Result::Failure(std::string("Theme: ")+error.what()); }
}
Engine::Result SaveTheme(const std::filesystem::path& path,const ThemeTokens& theme) {
    try {
        nlohmann::json json;json["version"]=1;
        for(const auto& [name,v]:theme.colors) { char hex[8];std::snprintf(hex,sizeof(hex),"#%02x%02x%02x",int(std::round(v.x*255)),int(std::round(v.y*255)),int(std::round(v.z*255)));json["colors"][name]=hex; }
        json["metrics"]=theme.metrics;
        auto temp=path;temp+=".tmp";{std::ofstream file(temp);file<<json.dump(2)<<'\n';if(!file) return Engine::Result::Failure("Cannot write theme");}
#ifdef _WIN32
        if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) return Engine::Result::Failure("Cannot replace theme file");
#else
        std::filesystem::rename(temp,path);
#endif
        return Engine::Result::Success();
    }catch(const std::exception& error) { return Engine::Result::Failure(error.what()); }
}
ImVec4 Color(const ThemeTokens& t,std::string_view name,bool linear) { auto c=t.colors.at(std::string(name));if(linear) {c.x=Linear(c.x);c.y=Linear(c.y);c.z=Linear(c.z);}return c; }
void ApplyTheme(ImGuiStyle& s,const ThemeTokens& t,bool srgb,float dpi) {
    s=ImGuiStyle{}; // Always scale an unscaled baseline, never the previous frame's metrics.
    auto c=[&](const char* name,float alpha=1.f){auto v=Color(t,name,srgb);v.w=alpha;return v;};
    auto m=[&](const char* name){return t.metrics.at(name);};
    for(auto& color:s.Colors) color=c("text-muted"); // New enum entries also receive a theme token.
    auto set=[&](ImGuiCol id,const char* token,float alpha=1.f){s.Colors[id]=c(token,alpha);};
    set(ImGuiCol_Text,"text-primary");set(ImGuiCol_TextDisabled,"text-dim");
    set(ImGuiCol_WindowBg,"bg-surface");set(ImGuiCol_ChildBg,"bg-surface");set(ImGuiCol_PopupBg,"bg-raised");set(ImGuiCol_MenuBarBg,"bg-raised");
    set(ImGuiCol_Border,"border");set(ImGuiCol_BorderShadow,"border",0);
    set(ImGuiCol_FrameBg,"bg-app");set(ImGuiCol_FrameBgHovered,"bg-raised");set(ImGuiCol_FrameBgActive,"border");
    set(ImGuiCol_TitleBg,"bg-app");set(ImGuiCol_TitleBgActive,"bg-raised");set(ImGuiCol_TitleBgCollapsed,"bg-app");
    set(ImGuiCol_ScrollbarBg,"bg-app",0);set(ImGuiCol_ScrollbarGrab,"scrollbar");set(ImGuiCol_ScrollbarGrabHovered,"scrollbar-hover");set(ImGuiCol_ScrollbarGrabActive,"scrollbar-active");
    set(ImGuiCol_CheckMark,"info");set(ImGuiCol_SliderGrab,"info");set(ImGuiCol_SliderGrabActive,"info-hover");
    set(ImGuiCol_Button,"btn");set(ImGuiCol_ButtonHovered,"btn-hover");set(ImGuiCol_ButtonActive,"btn-active");
    set(ImGuiCol_Header,"info",m("selected-alpha"));set(ImGuiCol_HeaderHovered,"border");set(ImGuiCol_HeaderActive,"info",m("active-alpha"));
    set(ImGuiCol_Separator,"border");set(ImGuiCol_SeparatorHovered,"info");set(ImGuiCol_SeparatorActive,"info");
    set(ImGuiCol_ResizeGrip,"info",0);set(ImGuiCol_ResizeGripHovered,"info",m("resize-hover-alpha"));set(ImGuiCol_ResizeGripActive,"info",m("resize-active-alpha"));
    set(ImGuiCol_Tab,"bg-app");set(ImGuiCol_TabHovered,"border");set(ImGuiCol_TabSelected,"bg-surface");set(ImGuiCol_TabSelectedOverline,"info");
    set(ImGuiCol_TabDimmed,"bg-app");set(ImGuiCol_TabDimmedSelected,"bg-app");set(ImGuiCol_TabDimmedSelectedOverline,"text-muted");
    set(ImGuiCol_DockingPreview,"info",m("dock-preview-alpha"));set(ImGuiCol_DockingEmptyBg,"bg-app");
    set(ImGuiCol_PlotLines,"info");set(ImGuiCol_PlotLinesHovered,"info-hover");set(ImGuiCol_PlotHistogram,"positive");set(ImGuiCol_PlotHistogramHovered,"positive");
    set(ImGuiCol_TableHeaderBg,"bg-raised");set(ImGuiCol_TableBorderStrong,"border");set(ImGuiCol_TableBorderLight,"border-light");set(ImGuiCol_TableRowBg,"bg-app",0);set(ImGuiCol_TableRowBgAlt,"text-primary",m("table-alt-alpha"));
    set(ImGuiCol_TextLink,"info");set(ImGuiCol_TextSelectedBg,"info",m("text-selection-alpha"));set(ImGuiCol_DragDropTarget,"warning");set(ImGuiCol_NavCursor,"info");set(ImGuiCol_NavWindowingHighlight,"info");
    s.Colors[ImGuiCol_ModalWindowDimBg]=s.Colors[ImGuiCol_NavWindowingDimBg]={0,0,0,m("modal-alpha")};
    s.WindowPadding={m("window-padding-x"),m("window-padding-y")};s.FramePadding={m("frame-padding-x"),m("frame-padding-y")};s.ItemSpacing={m("item-spacing-x"),m("item-spacing-y")};s.ItemInnerSpacing={m("inner-spacing-x"),m("inner-spacing-y")};s.IndentSpacing=m("indent");
    s.WindowRounding=m("window-rounding");s.FrameRounding=m("frame-rounding");s.PopupRounding=m("popup-rounding");s.ScrollbarRounding=m("scrollbar-rounding");s.GrabRounding=m("grab-rounding");s.TabRounding=m("tab-rounding");
    s.WindowBorderSize=m("window-border");s.FrameBorderSize=m("frame-border");s.PopupBorderSize=m("popup-border");s.ScrollbarSize=m("scrollbar-size");s.GrabMinSize=m("grab-min");s.DockingSeparatorSize=m("dock-separator");s.WindowMenuButtonPosition=ImGuiDir_Right;s.WindowTitleAlign={0,.5f};s.HoverDelayNormal=m("tooltip-delay");s.ScaleAllSizes(dpi);
}
}
