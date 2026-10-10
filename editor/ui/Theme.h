#pragma once
#include "engine/core/Result.hpp"
#include <imgui.h>
#include <filesystem>
#include <map>
#include <string>
namespace EditorUi {
struct ThemeTokens {
    std::map<std::string,ImVec4,std::less<>> colors;
    std::map<std::string,float,std::less<>> metrics;
};
Engine::Result LoadTheme(const std::filesystem::path&,ThemeTokens&);
Engine::Result SaveTheme(const std::filesystem::path&,const ThemeTokens&);
ImVec4 Color(const ThemeTokens&,std::string_view,bool linear);
void ApplyTheme(ImGuiStyle&,const ThemeTokens&,bool srgbTarget,float dpi=1);
}
