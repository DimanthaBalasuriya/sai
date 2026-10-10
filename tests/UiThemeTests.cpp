#include "Theme.h"
#include "Fonts.h"
#include "UiAnim.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
namespace {void Check(bool v,const char* message) {if(!v) throw std::runtime_error(message);}}
int main() {
    try {
        EditorUi::ThemeTokens theme;Check(bool(EditorUi::LoadTheme(ENGINE_TEST_THEME,theme)),"default theme must load");
        ImGuiStyle style;EditorUi::ApplyTheme(style,theme,false);
        Check(std::abs(style.Colors[ImGuiCol_WindowBg].x-18.f/255)<.00001f,"UNORM color remains authored sRGB");
        EditorUi::ApplyTheme(style,theme,true,1.5f);Check(style.Colors[ImGuiCol_WindowBg].x<.01f,"sRGB target receives linear RGB");
        Check(style.Colors[ImGuiCol_DockingPreview].w==.4f,"alpha must not undergo gamma conversion");
        Check(style.WindowPadding.x==12,"DPI metrics");EditorUi::ApplyTheme(style,theme,true,1.25f);Check(style.WindowPadding.x==10,"DPI scaling must not accumulate");
        for(auto c:style.Colors) Check(std::isfinite(c.x)&&c.w>=0&&c.w<=1,"all style colors valid");
        auto path=std::filesystem::temp_directory_path()/"engine-ui-theme-tests.json";
        Check(bool(EditorUi::SaveTheme(path,theme)),"roundtrip save");EditorUi::ThemeTokens loaded;Check(bool(EditorUi::LoadTheme(path,loaded)),"roundtrip load");for(const auto& [name,c]:theme.colors) {auto v=loaded.colors.at(name);Check(v.x==c.x&&v.y==c.y&&v.z==c.z,"hex roundtrip");}
        {std::ofstream file(path);file<<"{\"version\":1,\"colors\":{\"bg-app\":\"#zz0000\"}}";}
        Check(!EditorUi::LoadTheme(path,loaded),"invalid color must fail");Check(loaded.metrics==theme.metrics,"failure must preserve previous theme");
        {std::ofstream file(path);file<<"{\"version\":99}";}Check(!EditorUi::LoadTheme(path,loaded),"unknown version must fail");std::filesystem::remove(path);
        EditorUi::UiAnim anim;anim.BeginFrame(.01f,1);float a=anim.Ease(7,1);Check(a>0&&a<1&&anim.NeedsRedraw(),"hover eases over time");
        for(int frame=2;frame<60;++frame) {anim.BeginFrame(.01f,frame);anim.Ease(7,1);}Check(!anim.NeedsRedraw(),"settled animation must not request redraw");
        anim.BeginFrame(.01f,60);Check(anim.Ease(7,0)<1&&anim.NeedsRedraw(),"hover exit fades");
        ImGui::CreateContext();EditorUi::Fonts fonts;
        for(float dpi:{1.f,1.25f,1.5f,1.f}) {Check(bool(fonts.Rebuild(ENGINE_TEST_FONTS,dpi,15,14)),"font rebuild");Check(std::abs(fonts.regular->FontSize-std::floor(15*dpi))<.001f,"font pixel scale");Check(fonts.regular->FindGlyphNoFallback(0xf047)!=nullptr,"merged toolbar icon");Check(fonts.mono->FindGlyphNoFallback('0')!=nullptr,"numeric font");}
        ImGui::DestroyContext();std::cout<<"Theme parsing, sRGB, DPI, fonts and animation passed\n";return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
