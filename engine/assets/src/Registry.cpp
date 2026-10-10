#include "engine/assets/Registry.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <random>
#include <cstring>
namespace Engine {
void to_json(nlohmann::json& json,const Guid& id) { json=nlohmann::json::array({id.high,id.low}); }
void from_json(const nlohmann::json& json,Guid& id) { id={json.at(0).get<std::uint64_t>(),json.at(1).get<std::uint64_t>()}; }
void to_json(nlohmann::json& json,const Vec3& value) { json=nlohmann::json::array({value.x,value.y,value.z}); }
void from_json(const nlohmann::json& json,Vec3& value) { value={json.at(0).get<float>(),json.at(1).get<float>(),json.at(2).get<float>()}; }
namespace Renderer {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MaterialAsset,id,name,baseColor,emissive,metallic,roughness,normalScale,occlusionStrength,alphaCutoff,baseColorTexture,metallicRoughnessTexture,normalTexture,occlusionTexture,emissiveTexture,alphaMode,doubleSided)
}
namespace Assets {
namespace {
using Json=nlohmann::json;
Guid NewGuid() { std::random_device random; return {static_cast<std::uint64_t>(random())<<32|random(),static_cast<std::uint64_t>(random())<<32|random()}; }
std::string Key(Guid id) { return std::to_string(id.high)+"-"+std::to_string(id.low); }
template<class T> Json Pack(const std::vector<T>& values) {
    const auto* begin=reinterpret_cast<const std::uint8_t*>(values.data());
    return Json::binary(std::vector<std::uint8_t>(begin,begin+values.size()*sizeof(T)));
}
template<class T> std::vector<T> Unpack(const Json& json) {
    const auto& bytes=json.get_binary(); if (bytes.size()%sizeof(T)||bytes.size()>512*1024*1024) throw std::runtime_error("Invalid cooked buffer");
    std::vector<T> values(bytes.size()/sizeof(T)); if (!bytes.empty()) std::memcpy(values.data(),bytes.data(),bytes.size()); return values;
}
Json Cook(const ImportedScene& scene) {
    Json json={{"version",1},{"id",scene.id},{"name",scene.name},{"meshes",Json::array()},{"textures",Json::array()},{"materials",scene.assets.materials},{"nodes",Json::array()}};
    for (const auto& mesh:scene.assets.meshes) json["meshes"].push_back({{"id",mesh.id},{"name",mesh.name},{"vertices",Pack(mesh.vertices)},{"indices",Pack(mesh.indices)}});
    for (const auto& texture:scene.assets.textures) json["textures"].push_back({{"id",texture.id},{"name",texture.name},{"width",texture.width},{"height",texture.height},{"srgb",texture.srgb},{"rgba",Pack(texture.rgba)}});
    for (const auto& node:scene.nodes) {
        Json primitives=Json::array(); for (const auto& primitive:node.primitives) primitives.push_back({{"mesh",primitive.mesh},{"material",primitive.material}});
        json["nodes"].push_back({{"id",node.id},{"parent",node.parent},{"name",node.name},{"local",node.local.values},{"primitives",primitives}});
    } return json;
}
ImportedScene Uncook(const Json& json) {
    if (json.at("version")!=1) throw std::runtime_error("Unsupported cooked asset version");
    ImportedScene scene; scene.id=json.at("id").get<Guid>(); scene.name=json.at("name").get<std::string>();
    scene.assets.materials=json.at("materials").get<std::vector<Renderer::MaterialAsset>>();
    for (const auto& mesh:json.at("meshes")) scene.assets.meshes.push_back({mesh.at("id").get<Guid>(),mesh.at("name").get<std::string>(),Unpack<Renderer::MeshVertex>(mesh.at("vertices")),Unpack<std::uint32_t>(mesh.at("indices"))});
    for (const auto& texture:json.at("textures")) scene.assets.textures.push_back({texture.at("id").get<Guid>(),texture.at("name").get<std::string>(),texture.at("width").get<std::uint32_t>(),texture.at("height").get<std::uint32_t>(),texture.at("srgb").get<bool>(),Unpack<std::uint8_t>(texture.at("rgba"))});
    for (const auto& value:json.at("nodes")) {
        ImportedNode node; node.id=value.at("id").get<Guid>(); node.parent=value.at("parent").get<Guid>(); node.name=value.at("name").get<std::string>(); node.local.values=value.at("local").get<std::array<float,16>>();
        for (const auto& primitive:value.at("primitives")) node.primitives.push_back({primitive.at("mesh").get<Guid>(),primitive.at("material").get<Guid>()});
        scene.nodes.push_back(std::move(node));
    } return scene;
}
}
Result Registry::Open() {
    try {
        std::filesystem::create_directories(m_root); auto file=m_root/"registry.json";
        m_entries.clear(); if (!std::filesystem::exists(file)) return Result::Success();
        std::ifstream stream(file); Json json; stream>>json; if (json.at("version")!=1) return Result::Failure("Unknown asset registry version");
        for (const auto& entry:json.at("scenes")) m_entries.push_back({entry.at("id").get<Guid>(),entry.at("name").get<std::string>(),entry.at("source").get<std::string>(),m_root/(Key(entry.at("id").get<Guid>())+".cook")});
        return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result Registry::Import(const std::filesystem::path& path,ImportedScene& output) {
    try {
        const auto source=std::filesystem::weakly_canonical(path).string(); Guid id{};
        for (const auto& entry:m_entries) if (entry.source==source) id=entry.id;
        if (!id) id=NewGuid();
        ImportedScene scene; auto result=ImportGltf(path,id,scene); if (!result) return result;
        auto cooked=m_root/(Key(id)+".cook"); auto bytes=Json::to_cbor(Cook(scene));
        { std::ofstream stream(cooked,std::ios::binary|std::ios::trunc); stream.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())); if (!stream) return Result::Failure("Cannot write cooked assets"); }
        auto entries=m_entries; bool replaced=false;
        for (auto& entry:entries) if (entry.id==id) { entry={id,scene.name,source,cooked}; replaced=true; }
        if (!replaced) entries.push_back({id,scene.name,source,cooked});
        Json catalog={{"version",1},{"scenes",Json::array()}};
        for (const auto& entry:entries) catalog["scenes"].push_back({{"id",entry.id},{"name",entry.name},{"source",entry.source}});
        { std::ofstream stream(m_root/"registry.json"); stream<<catalog.dump(2); if (!stream) return Result::Failure("Cannot write asset registry"); }
        m_entries=std::move(entries); output=std::move(scene); return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result Registry::Load(Guid id,ImportedScene& output) const {
    try {
        for (const auto& entry:m_entries) if (entry.id==id) {
            std::ifstream stream(entry.cooked,std::ios::binary|std::ios::ate); if (!stream) return Result::Failure("Cooked asset missing");
            auto size=stream.tellg(); if (size<=0||size>1024ll*1024*1024) return Result::Failure("Cooked asset size invalid");
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size)); stream.seekg(0); stream.read(reinterpret_cast<char*>(bytes.data()),size); if (!stream) return Result::Failure("Cooked asset read failed");
            auto scene=Uncook(Json::from_cbor(bytes)); if (scene.id!=id) return Result::Failure("Cooked asset identity mismatch"); output=std::move(scene); return Result::Success();
        } return Result::Failure("Asset GUID not registered");
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
}
}
