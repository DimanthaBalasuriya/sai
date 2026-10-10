#pragma once
#include "engine/scene/Commands.hpp"
#include <filesystem>
namespace Engine::Scene {
// Text scene files are generated from reflected fields; World mutation uses a command.
// Main-thread only. Load parses and validates before replacing the active World.
Result SaveScene(const World& world,const std::filesystem::path& path,std::span<const Guid> assetScenes={});
Result ReadScene(const std::filesystem::path& path,std::vector<EntityDefinition>& output,std::vector<Guid>& assetScenes);
Result LoadScene(World& world,CommandHistory& history,const std::filesystem::path& path,std::vector<Guid>& assetScenes);
}
