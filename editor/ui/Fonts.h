#pragma once
#include "engine/core/Result.hpp"
#include <filesystem>
struct ImFont;
namespace EditorUi {
struct Fonts {
    ImFont* regular=nullptr;ImFont* mono=nullptr;
    Engine::Result Rebuild(const std::filesystem::path& directory,float dpi,float size,float monoSize);
};
}
