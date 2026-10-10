#pragma once
#include "engine/core/Result.hpp"
#include "engine/renderer/RenderWorld.hpp"
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
namespace Engine::Scene {
// Components own values. Public API exposes no EnTT types; main-thread only.
struct Name { std::string value="Entity"; };
struct Transform { Vec3 position,rotation,scale{1,1,1}; }; // XYZ Euler radians; T*Rz*Ry*Rx*S.
struct Hierarchy { Guid parent; };
struct MeshRenderer { Guid mesh=kBuiltinCube,material=kBuiltinMaterial; Vec3 color{1,1,1}; };
struct Camera { float verticalFov=1.0472f,nearPlane=0.1f,farPlane=1000; float ev100=15,exposureCompensation=0; };
struct Light { std::uint32_t type=0; Vec3 color{1,1,1}; float intensity=1,range=25,spotAngle=0.6f; }; // 0 directional, 1 point, 2 spot.
enum class ComponentType : std::uint32_t { Name,Transform,Hierarchy,MeshRenderer,Camera,Light };
enum class FieldType { String,Vector3,Guid,Float,Unsigned };
using FieldValue=std::variant<std::string,Vec3,Guid,float,std::uint32_t>;
struct FieldInfo { std::string_view name; FieldType type; std::size_t offset; float minimum,maximum; };
struct ComponentInfo { ComponentType type; std::string_view name; std::span<const FieldInfo> fields; };
// Module schemas are registered at startup. The registry owns all names, fields
// and defaults for process lifetime; identities >=256 are reserved for modules.
struct ModuleComponent {ComponentType type;std::vector<FieldValue> fields;};
using ComponentValidator=Result(*)(const ModuleComponent&);
using RenderLightExtractor=Renderer::RenderLight(*)(const ModuleComponent&,const Mat4&,Guid);
using RenderSettingsExtractor=void(*)(const ModuleComponent&,Renderer::RenderWorld&);
Result RegisterRenderSettingsExtractor(ComponentType,RenderSettingsExtractor);
Result RegisterComponent(ComponentType,std::string_view,std::span<const FieldInfo>,std::span<const FieldValue> defaults,ComponentValidator validator=nullptr);
// Registered module packers receive only owned values and the computed transform.
Result RegisterRenderLightExtractor(ComponentType,RenderLightExtractor);
Result DefaultComponent(ComponentType,ModuleComponent&);
bool IsModuleComponent(ComponentType) noexcept;
Result ValidateModuleComponent(const ModuleComponent&);
struct EntityDefinition {
    Guid id; Name name; Transform transform; Hierarchy hierarchy;
    std::optional<MeshRenderer> mesh; std::optional<Camera> camera; std::optional<Light> light;
    std::vector<ModuleComponent> extensions;
};
class CreateEntityCommand;
class SetFieldCommand;
class ReparentCommand;
struct EntityStatistics {
    std::size_t dirty=0,updated=0,instances=0,staticEntities=0,dynamicEntities=0;
    double updateMs=0,extractMs=0;
    std::array<std::size_t,1025> depthHistogram{};
};
struct HierarchyEntry {Guid id,parent;std::string name;std::uint32_t depth=0,children=0;};
class World {
public:
    World(); ~World();
    World(const World&)=delete; World& operator=(const World&)=delete;
    bool Contains(Guid id) const noexcept;
    std::size_t EntityCount() const noexcept;
    std::uint64_t Revision() const noexcept;
    std::uint64_t HierarchyVersion() const noexcept;
    std::uint64_t ComponentVersion(Guid) const noexcept;
    const EntityStatistics& Statistics() const noexcept;
    Result Update();
    void BeginFrame() noexcept;
    Result HierarchyEntries(std::vector<HierarchyEntry>&) const;
    Result Descendants(std::span<const Guid>,std::vector<Guid>&) const;
    std::span<const ComponentInfo> Reflection() const noexcept;
    Result ReadField(Guid id,ComponentType component,std::string_view field,FieldValue& value) const;
    Result GlobalTransform(Guid id,Mat4& value);
    Result Extract(Renderer::RenderWorld& output);
    Result Entities(std::vector<Guid>& output) const;
    Result EntitiesWithComponent(ComponentType,std::vector<Guid>&) const;
    Result VisibleEntitiesWithoutMesh(std::vector<Guid>&);
    bool HasComponent(Guid id,ComponentType type) const noexcept;
    Result Definition(Guid id,EntityDefinition& output) const;
private:
    friend class CreateEntityCommand; friend class SetFieldCommand; friend class ReparentCommand;
    friend class ReplaceSceneCommand;
    friend class SetTransformCommand;
    friend class EntityBatchCommand;
    Result Create(const EntityDefinition& definition);
    Result Remove(Guid id);
    Result ApplyField(Guid id,ComponentType component,std::string_view field,const FieldValue& value);
    Result Reparent(Guid id,Guid parent);
    Result Replace(std::span<const EntityDefinition> entities);
    Result ApplyTransform(Guid id,const Transform& transform);
    Result ApplyBatch(std::span<const EntityDefinition>,std::span<const Guid>);
    struct Impl; std::unique_ptr<Impl> m_impl;
};
}
