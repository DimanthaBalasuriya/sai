#include "engine/scene/Serialization.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace Engine::Scene {
namespace {
using Json=nlohmann::json;
Json Encode(const FieldValue& value) {
    return std::visit([](const auto& field) -> Json {
        using T=std::decay_t<decltype(field)>;
        if constexpr(std::is_same_v<T,Vec3>) return Json::array({field.x,field.y,field.z});
        else if constexpr(std::is_same_v<T,Guid>) return Json::array({field.high,field.low});
        else return field;
    },value);
}
FieldValue Decode(const Json& json,FieldType type) {
    switch(type) {
    case FieldType::String:return json.get<std::string>();
    case FieldType::Vector3:return Vec3{json.at(0).get<float>(),json.at(1).get<float>(),json.at(2).get<float>()};
    case FieldType::Guid:return Guid{json.at(0).get<std::uint64_t>(),json.at(1).get<std::uint64_t>()};
    case FieldType::Float:return json.get<float>();
    case FieldType::Unsigned:return json.get<std::uint32_t>();
    } throw std::runtime_error("Unknown reflected type");
}
void* Component(EntityDefinition& definition,ComponentType type) {
    switch(type) {
    case ComponentType::Name:return &definition.name;
    case ComponentType::Transform:return &definition.transform;
    case ComponentType::Hierarchy:return &definition.hierarchy;
    case ComponentType::MeshRenderer:if (!definition.mesh) definition.mesh.emplace();return &*definition.mesh;
    case ComponentType::Camera:if (!definition.camera) definition.camera.emplace();return &*definition.camera;
    case ComponentType::Light:if (!definition.light) definition.light.emplace();return &*definition.light;
    } return nullptr;
}
}
Result SaveScene(const World& world,const std::filesystem::path& path,std::span<const Guid> assetScenes) {
    try {
        Json json={{"version",1},{"assets",Json::array()},{"entities",Json::array()}};
        for (auto id:assetScenes) json["assets"].push_back(Json::array({id.high,id.low}));
        std::vector<Guid> entities;auto result=world.Entities(entities);if (!result) return result;
        for (auto id:entities) {
            Json entity={{"id",Json::array({id.high,id.low})},{"components",Json::object()}};
            for (const auto& component:world.Reflection()) if (world.HasComponent(id,component.type)) {
                Json fields=Json::object();for (const auto& field:component.fields) {
                    FieldValue value;result=world.ReadField(id,component.type,field.name,value);if (!result) return result;fields[std::string(field.name)]=Encode(value);
                } entity["components"][std::string(component.name)]=std::move(fields);
            } json["entities"].push_back(std::move(entity));
        }
        auto parent=path.parent_path();if (!parent.empty()) std::filesystem::create_directories(parent);
        auto temporary=path;temporary+=".engine-tmp";
        { std::ofstream stream(temporary,std::ios::binary);stream<<json.dump(2);stream.flush();if (!stream) return Result::Failure("Scene save failed"); }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) return Result::Failure("Scene file replacement failed");
#else
        std::filesystem::rename(temporary,path);
#endif
        return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result ReadScene(const std::filesystem::path& path,std::vector<EntityDefinition>& output,std::vector<Guid>& assetScenes) {
    try {
        std::ifstream stream(path);if (!stream) return Result::Failure("Cannot open scene");Json json;stream>>json;
        if (json.at("version")!=1) return Result::Failure("Unsupported scene version");
        World schema;std::vector<EntityDefinition> definitions;std::vector<Guid> assets;
        for (const auto& id:json.at("assets")) assets.push_back({id.at(0).get<std::uint64_t>(),id.at(1).get<std::uint64_t>()});
        for (const auto& entity:json.at("entities")) {
            EntityDefinition definition;definition.id={entity.at("id").at(0).get<std::uint64_t>(),entity.at("id").at(1).get<std::uint64_t>()};
            for (const auto& [name,fields]:entity.at("components").items()) {
                const ComponentInfo* info=nullptr;for (const auto& candidate:schema.Reflection()) if (candidate.name==name) info=&candidate;
                if (!info) return Result::Failure("Unknown reflected component: "+name);
                if(IsModuleComponent(info->type)){
                    ModuleComponent component;auto result=DefaultComponent(info->type,component);if(!result)return result;
                    for(const auto& [fieldName,encoded]:fields.items()){std::size_t index=0;for(;index<info->fields.size();++index)if(info->fields[index].name==fieldName)break;if(index==info->fields.size())return Result::Failure("Unknown module field: "+fieldName);component.fields[index]=Decode(encoded,info->fields[index].type);}
                    result=ValidateModuleComponent(component);if(!result)return result;definition.extensions.push_back(std::move(component));continue;
                }
                auto* component=Component(definition,info->type);
                for (const auto& [fieldName,encoded]:fields.items()) {
                    const FieldInfo* field=nullptr;for (const auto& candidate:info->fields) if (candidate.name==fieldName) field=&candidate;
                    if (!field) return Result::Failure("Unknown reflected field: "+fieldName);
                    auto value=Decode(encoded,field->type);auto* address=static_cast<std::byte*>(component)+field->offset;
                    std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;*reinterpret_cast<T*>(address)=v;},value);
                }
            } definitions.push_back(std::move(definition));
        }
        CommandHistory validation;auto result=validation.Execute(schema,std::make_unique<ReplaceSceneCommand>(definitions));if (!result) return result;
        output=std::move(definitions);assetScenes=std::move(assets);return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result LoadScene(World& world,CommandHistory& history,const std::filesystem::path& path,std::vector<Guid>& assetScenes) {
    std::vector<EntityDefinition> definitions;std::vector<Guid> assets;auto result=ReadScene(path,definitions,assets);if (!result) return result;
    result=history.Execute(world,std::make_unique<ReplaceSceneCommand>(std::move(definitions)));if (result) assetScenes=std::move(assets);return result;
}
}
