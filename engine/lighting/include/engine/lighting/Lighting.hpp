#pragma once
#include "engine/scene/Commands.hpp"
#include "engine/core/IModule.hpp"
#include <span>
namespace Engine::Rhi {class Device;}
namespace Engine::Extensions {class FeatureRegistry;}
namespace Engine::Lighting {
inline constexpr Scene::ComponentType kDirectionalLight=static_cast<Scene::ComponentType>(0x4c490000);
inline constexpr Scene::ComponentType kPointLight=static_cast<Scene::ComponentType>(0x4c490001);
inline constexpr Scene::ComponentType kSpotLight=static_cast<Scene::ComponentType>(0x4c490002);
inline constexpr Scene::ComponentType kRectLight=static_cast<Scene::ComponentType>(0x4c490003);
inline constexpr Scene::ComponentType kDiskLight=static_cast<Scene::ComponentType>(0x4c490004);
enum class IntensityUnit:std::uint32_t {Physical=0,Lumen=1,Unitless=2};
// Authoring values; transforms belong to the scene. Main-thread component registry.
struct CommonLight {
    Vec3 color{1,1,1};float temperature=6500;
    std::uint32_t useTemperature=0,unit=0,enabled=1,castShadows=1;
    std::uint32_t shadowResolution=1024,filterQuality=1;
    float depthBias=.0005f,normalBias=.02f;
};
struct DirectionalLight {
    CommonLight common;float illuminance=100000,angularDiameter=.533f;
    std::uint32_t cascadeCount=4;float maxShadowDistance=200,splitBlend=.5f,cascadeBlend=.1f;
};
struct PointLight {CommonLight common;float intensity=300,range=10,sourceRadius=.01f;};
struct SpotLight {CommonLight common;float intensity=300,range=10,sourceRadius=.01f,innerCone=.349066f,outerCone=.523599f;};
struct RectLight {CommonLight common;float intensity=1000,range=20,width=1,height=1;};
struct DiskLight {CommonLight common;float intensity=1000,range=20,radius=.5f;};
struct LightType {
    Scene::ComponentType component;std::uint32_t shaderId;const char* name;const char* shaderFunction;
    std::span<const Scene::FieldInfo> fields;
    Scene::RenderLightExtractor packer;Scene::ComponentValidator validator=nullptr;
};
std::span<const LightType> LightTypes();
const LightType* FindLightType(Scene::ComponentType) noexcept;
bool HasLight(const Scene::EntityDefinition&) noexcept;
Result RegisterComponents();
Result Encode(Scene::ComponentType,const void*,Scene::ModuleComponent&);
Result Decode(const Scene::ModuleComponent&,void*);
float PointLumensToCandela(float lumens) noexcept;
float SpotSolidAngle(float outerHalfAngle) noexcept;
float SpotLumensToCandela(float lumens,float outerHalfAngle) noexcept;
float PhysicalIntensity(Scene::ComponentType,float value,std::uint32_t unit,float outerHalfAngle=.523599f,float area=1) noexcept;
float EmittingArea(const Scene::World&,Guid,Scene::ComponentType);
Vec3 TemperatureTint(float kelvin) noexcept;
Renderer::RenderLight Pack(Scene::ComponentType,const CommonLight&,float intensity,float range,const Mat4&,Guid,float inner=0,float outer=0);
struct Preset {const char* name;float lux,ev100;};
std::span<const Preset> Presets();
Result SoloLight(Scene::World&,Scene::CommandHistory&,Guid);
Result ApplyPreset(Scene::World&,Scene::CommandHistory&,std::size_t);
Result ChangeIntensityUnit(Scene::World&,Scene::CommandHistory&,Guid,Scene::ComponentType,std::uint32_t);
// Module owns the GPU upload feature; device/registry must outlive it.
std::unique_ptr<IModule> MakeLightingModule(Extensions::FeatureRegistry&,Rhi::Device&,const std::filesystem::path& shaders);
}
