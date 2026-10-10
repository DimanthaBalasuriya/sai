#include "engine/lighting/Providers.hpp"
#include "engine/renderer/Exposure.hpp"
#include <glm/gtc/matrix_transform.hpp>
namespace Engine::Lighting {
class ExponentialHeightFog final:public Extensions::IRenderFeature,public ISceneFogProvider {
public:
    ExponentialHeightFog(Rhi::Device& d,std::filesystem::path p):m_pass(d),m_path(std::move(p)){}
    const char* Name()const noexcept override{return "Height Fog";}int Order()const noexcept override{return 20;}
    Result Initialize()override{auto r=RegisterSettings();return r?m_pass.Initialize(m_path,"heightfog",Rhi::Format::Rgba16Float):r;}
    Result AddPasses(Renderer::RenderGraph& g,Extensions::FeatureFrame& f)override{return Composite(g,f);}
    Result Composite(Renderer::RenderGraph& g,Extensions::FeatureFrame& f)override{
        if(!f.lit||!f.world->settings.fog||f.world->settings.density<=0)return Result::Success();
        auto& s=f.world->settings;auto data=f.data;auto& transform=f.world->camera.transform.values;data.inverseViewProjection=glm::translate(glm::mat4(1),glm::vec3(-transform[12],-transform[13],-transform[14]))*data.inverseViewProjection;data.parameters={s.density,s.heightFalloff,s.baseHeight,s.fogDistance};auto exposure=Renderer::ManualExposure(f.world->camera.ev100,f.world->camera.exposureCompensation);data.camera={s.fogColor.x*exposure,s.fogColor.y*exposure,s.fogColor.z*exposure,f.world->camera.transform.values[13]};
        auto output=Extensions::HdrImage(g,"Height fog composite",f.width,f.height);std::array inputs{f.color,f.depth};auto r=m_pass.Add(g,"Height Fog",inputs,output,f,data);if(r)f.color=output;return r;
    }
private:Extensions::FullscreenPass m_pass;std::filesystem::path m_path;
};
std::unique_ptr<IModule> MakeFogModule(Extensions::FeatureRegistry& r,Rhi::Device& d,const std::filesystem::path& p){return std::make_unique<Extensions::FeatureModule>(r,std::make_unique<ExponentialHeightFog>(d,p));}
}

