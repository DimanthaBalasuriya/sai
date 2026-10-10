// Matches the shared 64-byte Renderer::GpuLight ABI.
struct GpuLight {
    float3 position;float invRangeSq;
    float3 direction;uint type;
    float3 color;uint shadowIndex;
    float spotScale;float spotOffset;uint flags;float padding;
};
#include "DirectionalLight.slang"
#include "PointLight.slang"
#include "SpotLight.slang"
