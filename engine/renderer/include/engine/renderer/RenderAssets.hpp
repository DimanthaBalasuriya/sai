#pragma once
#include "engine/core/Guid.hpp"
#include "engine/core/Math.hpp"
#include <string>
#include <vector>
namespace Engine::Renderer {
// Owned CPU assets. Importers populate these; renderer uploads copies to its device.
struct MeshVertex { float x=0,y=0,z=0,u=0,v=0,nx=0,ny=1,nz=0; };
struct MeshAsset { Guid id; std::string name; std::vector<MeshVertex> vertices; std::vector<std::uint32_t> indices; }; // Cluster data can extend this format.
struct TextureAsset { Guid id; std::string name; std::uint32_t width=0,height=0; bool srgb=true; std::vector<std::uint8_t> rgba; };
struct MaterialAsset {
    Guid id; std::string name;
    std::array<float,4> baseColor{1,1,1,1}; Vec3 emissive;
    float metallic=1,roughness=1,normalScale=1,occlusionStrength=1,alphaCutoff=0.5f;
    Guid baseColorTexture,metallicRoughnessTexture,normalTexture,occlusionTexture,emissiveTexture;
    std::uint32_t alphaMode=0; bool doubleSided=false; // Opaque=0, mask=1; blend is explicitly unsupported for now.
};
struct AssetBundle { std::vector<MeshAsset> meshes; std::vector<TextureAsset> textures; std::vector<MaterialAsset> materials; };
}
