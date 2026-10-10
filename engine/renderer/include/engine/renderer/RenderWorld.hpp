#pragma once
#include "engine/core/Guid.hpp"
#include "engine/core/Math.hpp"
#include "engine/renderer/GpuLight.hpp"
#include <vector>
#include "engine/rhi/Resources.hpp"
namespace Engine::Renderer {
// Extracted value snapshot owns its data. Renderer never holds World/ECS pointers.
struct RenderInstance { Guid entity, mesh, material; Mat4 model, normal; Vec3 color{1,1,1}; };
struct LightShadowSettings {std::uint32_t resolution=1024,cascades=4,filter=1,pad=0;float distance=200,split=.5f,blend=.1f,depthBias=.0005f;float normalBias=.02f;std::array<float,3> padding{};};
struct RenderLight { std::uint32_t type=0; Vec3 position,direction{0,-1,0},color{1,1,1}; float intensity=1,range=25,spotAngle=0.6f; std::uint32_t identityPadding=0;Guid entity;bool modulePacked=false;std::array<std::byte,7> padding{};GpuLight gpu;LightShadowSettings shadow;Vec3 right{1,0,0};float width=1;Vec3 up{0,1,0};float height=1; };
struct RenderCamera { Mat4 transform; float verticalFov=1.0472f,nearPlane=0.1f,farPlane=1000; bool valid=false; float ev100=15,exposureCompensation=0; };
struct RenderLighting {
    Rhi::BufferHandle buffer;std::uint64_t address=0;
    std::uint32_t directionalCount=0,localCount=0;
    std::uint64_t uploadedBytes=0,packedLights=0,rebuilds=0;
    Rhi::BufferHandle frameBuffer,clusterBounds,clusterCells,clusterIndices,clusterCounters;
    std::uint64_t frameAddress=0;
    std::uint32_t clusterCount=0,overflow=0,maxPerCluster=0,visibleLocal=0;
    Rhi::BufferHandle areaBuffer,ltcBuffer;Rhi::ImageHandle shadowAtlas;Rhi::BufferHandle shadowBuffer;std::uint64_t shadowAddress=0;
    std::uint32_t shadowPasses=0,shadowTiles=0;
    float exposureEv=15,exposureTarget=15;std::array<float,256> histogram{};
};
struct RenderWorld {
    struct Settings {
        std::uint32_t environment=2;Vec3 ambient{.08f,.08f,.08f};float environmentIntensity=1;std::string hdri;
        std::uint32_t fog=0;float density=.001f,heightFalloff=.1f,baseHeight=0;Vec3 fogColor{.5f,.6f,.7f};float fogDistance=1000;
        std::uint32_t autoExposure=0;float minEv=-10,maxEv=20,speedUp=3,speedDown=1;
    } settings;
    RenderLighting lighting;
    struct Environment { Rhi::ImageHandle diffuseCube,shadow; Mat4 worldToShadow; float intensity=1,wetness=0;Rhi::BufferHandle irradiance;std::uint64_t irradianceAddress=0;std::array<Rhi::ImageHandle,5> specular{};Rhi::ImageHandle brdf;std::array<Vec3,4> bodyTransmission{{{1,1,1},{1,1,1},{1,1,1},{1,1,1}}};float spaceBlend=0; };
    Environment environment;
    Rhi::ImageHandle atmosphericTransmittance;Vec3 atmosphere{0,0,0};
    Guid source;
    std::vector<RenderInstance> instances;
    std::vector<RenderLight> lights;
    RenderCamera camera;
    std::uint64_t revision=0;
    // Optional incremental transfer metadata. Consumers may retain older owned
    // snapshots; baseRevision tells them when dirty indices can be applied.
    std::uint64_t baseRevision=0,instanceRevision=0;
    bool fullUpdate=true;
    bool modified=false; // A preview/filter changed owned values; extraction restores them.
    std::vector<std::uint32_t> changedInstances;
    std::vector<std::uint32_t> changedLights;
};
}
