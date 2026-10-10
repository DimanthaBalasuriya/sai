#include "Fonts.h"
#include <imgui.h>
namespace EditorUi {
Engine::Result Fonts::Rebuild(const std::filesystem::path& directory,float dpi,float size,float monoSize) {
    for(auto file:{"Inter.ttf","RobotoMono.ttf","Icons.ttf"}) if(!std::filesystem::exists(directory/file)) return Engine::Result::Failure("Missing bundled font: "+(directory/file).string());
    auto& io=ImGui::GetIO();io.Fonts->Clear();regular=io.Fonts->AddFontFromFileTTF((directory/"Inter.ttf").string().c_str(),size*dpi);
    ImFontConfig config;config.MergeMode=true;config.PixelSnapH=true;config.GlyphMinAdvanceX=size*dpi;
    static const ImWchar icons[]{0xf000,0xf8ff,0};
    io.Fonts->AddFontFromFileTTF((directory/"Icons.ttf").string().c_str(),size*dpi,&config,icons);
    mono=io.Fonts->AddFontFromFileTTF((directory/"RobotoMono.ttf").string().c_str(),monoSize*dpi);
    if(!regular||!mono||!io.Fonts->Build()) return Engine::Result::Failure("Font atlas build failed");
    io.FontDefault=regular;return Engine::Result::Success();
}
}
