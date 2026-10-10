#pragma once
#include "engine/core/Result.hpp"
#include "engine/renderer/RenderAssets.hpp"
#include "engine/scene/Commands.hpp"
#include <filesystem>
namespace Engine::Assets {
// Nodes keep local matrices and GUID hierarchy; each mesh primitive is an asset.
struct PrimitiveRef { Guid mesh,material; };
struct ImportedNode { Guid id,parent; std::string name; Mat4 local; std::vector<PrimitiveRef> primitives; };
struct ImportedScene { Guid id; std::string name; Renderer::AssetBundle assets; std::vector<ImportedNode> nodes; };
struct RegistryEntry { Guid id; std::string name,source; std::filesystem::path cooked; };
// Owns cooked files/catalog under root; synchronous main-thread import in M4.
class Registry {
public:
    explicit Registry(std::filesystem::path root):m_root(std::move(root)){}
    Result Open();
    Result Import(const std::filesystem::path& source,ImportedScene& output);
    Result Load(Guid id,ImportedScene& output) const;
    std::span<const RegistryEntry> Entries() const noexcept { return m_entries; }
private: std::filesystem::path m_root; std::vector<RegistryEntry> m_entries;
};
Result ImportGltf(const std::filesystem::path& path,Guid source,ImportedScene& output);
Result Instantiate(const ImportedScene& imported,Scene::World& world,Scene::CommandHistory& history);
}
