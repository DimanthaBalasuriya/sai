#include "engine/extensions/Builtins.hpp"
#include "engine/lighting/Lighting.hpp"
#include "engine/lighting/Providers.hpp"
#include "../../lighting/render/ShadowMaps.hpp"
#ifdef ENGINE_FEATURE_SKYBOX
#include "engine/extensions/Skybox.hpp"
#endif
#ifdef ENGINE_FEATURE_POSTPROCESS
#include "engine/extensions/PostProcess.hpp"
#endif
#ifdef ENGINE_FEATURE_DEBUG_DRAW
#include "engine/extensions/DebugDraw.hpp"
#endif
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
#include "engine/sky/GpuSky.hpp"
#endif
namespace Engine::Extensions {
Result RegisterBuiltins(FeaturePipeline& pipeline,Rhi::Device& device,const std::filesystem::path& shaders) {
    auto directory=shaders/"extensions";Result result=Result::Success();
    (void)pipeline;(void)device;(void)directory;
    result=pipeline.AddModule(Lighting::MakeExposureModule(pipeline.Features(),device,directory));if(!result)return result;
    result=pipeline.AddModule(Lighting::MakeEnvironmentModule(pipeline.Features(),device,directory));if(!result)return result;
    result=pipeline.AddModule(Lighting::MakeFogModule(pipeline.Features(),device,directory));if(!result)return result;
    result=pipeline.AddModule(Lighting::MakeShadowModule(pipeline.Features(),device,directory));if(!result)return result;
    result=pipeline.AddModule(Lighting::MakeLightingModule(pipeline.Features(),device,directory));if(!result)return result;
#if defined(ENGINE_FEATURE_SKYBOX) && !defined(ENGINE_FEATURE_PROCEDURAL_SKY)
    result=pipeline.AddModule(MakeSkyboxModule(pipeline.Features(),device,directory));if(!result)return result;
#endif
#ifdef ENGINE_FEATURE_POSTPROCESS
    result=pipeline.AddModule(MakeBloomModule(pipeline.Features(),device,directory));if(!result)return result;
    result=pipeline.AddModule(MakeTonemapModule(pipeline.Features(),device,directory));if(!result)return result;
#endif
#ifdef ENGINE_FEATURE_DEBUG_DRAW
    result=pipeline.AddModule(MakeDebugDrawModule(pipeline.Features(),device,directory));if(!result)return result;
#endif
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
    result=pipeline.AddModule(Sky::MakeSkyModule(pipeline.Features(),device,directory));if(!result)return result;
#endif
    return result;
}
}
