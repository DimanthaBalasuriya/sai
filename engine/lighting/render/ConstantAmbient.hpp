#pragma once
#include "engine/lighting/Providers.hpp"
namespace Engine::Lighting {
class ConstantAmbient final:public IEnvironmentLighting {
public:Result Evaluate(Renderer::RenderGraph&,Extensions::FeatureFrame&,Renderer::RenderWorld& w)override{w.environment.diffuseCube={};w.environment.irradiance={};w.environment.irradianceAddress=0;w.environment.specular={};w.environment.brdf={};w.environment.intensity=w.settings.environmentIntensity;return Result::Success();}
};
}
