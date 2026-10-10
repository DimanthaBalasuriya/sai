#include "engine/core/EngineContext.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/ModuleRegistry.hpp"
#include "engine/core/Profile.hpp"
#include "engine/platform/PlatformModule.hpp"
#include "engine/platform/Window.hpp"
#include "engine/rhi/Device.hpp"
#include "engine/extensions/Builtins.hpp"
#include "engine/scene/SceneModule.hpp"
#include "engine/lighting/Lighting.hpp"
#include "DemoScene.hpp"
#include "LightingScenario.hpp"
#include <vector>
#include <algorithm>
#include <numeric>
#include <charconv>
#include <cmath>
#include <exception>
#include <filesystem>
#include <chrono>
#include <cstdlib>
#include <string_view>
#include <glm/gtc/packing.hpp>

#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
#include "engine/sky/GpuSky.hpp"
#endif
#ifdef _WIN32
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement=1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1;
}
#endif
int main(int argc, char** argv) {
    try {
        std::string gpuOverride;bool lightingGrid=false,evExplicit=false,lightingSmoke=false,advancedLighting=false,lightingCompare=false,orbitalSmoke=false;float ev100=15;
        int smokeFrames = 0;
        bool resizeSmoke = false;
        bool instanceDemo=false;bool skySmoke=false,skyBenchmark=false,skyTimeExplicit=false;float skyTime=12;int skyQuality=1,skyDebug=0;
        std::filesystem::path gltfPath;
        for (int argument=1;argument<argc;++argument) {
            std::string_view flag(argv[argument]);
            if(flag.starts_with("--gpu=")) gpuOverride=std::string(flag.substr(6));
            else if(flag=="--lighting-grid")lightingGrid=true;
            else if(flag=="--lighting-path-compare"){advancedLighting=true;lightingCompare=true;smokeFrames=10;}
            else if(flag=="--orbital-smoke"){orbitalSmoke=true;smokeFrames=150;}
            else if(flag=="--lighting-all-smoke"){advancedLighting=true;smokeFrames=150;}
            else if(flag=="--lighting-smoke"){lightingSmoke=true;smokeFrames=120;}
            else if(flag.starts_with("--ev100=")){evExplicit=true;auto value=flag.substr(8);auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),ev100);if(error!=std::errc{}||end!=value.data()+value.size()||!std::isfinite(ev100)||ev100 < -16||ev100>32)return 2;}
            else if(flag=="--sky-smoke"){skySmoke=true;smokeFrames=240;}
            else if(flag=="--sky-benchmark"){skyBenchmark=true;smokeFrames=300;}
            else if(flag=="--sky-time"&&argument+1<argc){skyTimeExplicit=true;skyTime=std::stof(argv[++argument]);}
            else if(flag=="--sky-quality"&&argument+1<argc)skyQuality=std::stoi(argv[++argument]);
            else if(flag=="--sky-debug"&&argument+1<argc)skyDebug=std::stoi(argv[++argument]);
            else if (flag=="--resize-smoke") { resizeSmoke=true;smokeFrames=120; }
            else if (flag=="--instances") instanceDemo=true;
            else if (flag=="--gltf"&&argument+1<argc) gltfPath=argv[++argument];
            else if (flag=="--smoke-frames"&&argument+1<argc) {
                std::string_view value(argv[++argument]);auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),smokeFrames);
                if (error!=std::errc{}||end!=value.data()+value.size()||smokeFrames<=0) return 2;
            } else { Engine::Log::Error("Usage: engine_runtime [--gltf path | --instances] [--smoke-frames N | --resize-smoke]"); return 2; }
        }
        if(lightingSmoke&&(lightingGrid||evExplicit||skySmoke||skyBenchmark||skyTimeExplicit||instanceDemo))return 2;
        Engine::EngineContext context;
        Engine::ModuleRegistry modules(context);
        auto result = modules.Add(std::make_unique<Engine::PlatformModule>());
        if (!result) { Engine::Log::Error(result.Error()); return 1; }
        auto sceneModule=std::make_unique<Engine::Scene::SceneModule>();
        auto& world=sceneModule->GetWorld();
        result=modules.Add(std::move(sceneModule));
        if (!result) { Engine::Log::Error(result.Error()); return 1; }
        result = modules.Startup();
        if (!result) { Engine::Log::Error(result.Error()); return 1; }
        {
            Engine::Window window;
            result = window.Open(lightingCompare?320:advancedLighting||skySmoke||skyBenchmark?1920:1280,lightingCompare?180:advancedLighting||skySmoke||skyBenchmark?1080:720,"Engine | F4 procedural sky");
            if (!result) { Engine::Log::Error(result.Error()); return 1; }
            Engine::Rhi::Device device;
            const auto executable = std::filesystem::absolute(argv[0]);
            result = device.Initialize(window,gpuOverride);
            if (!result) { Engine::Log::Error(result.Error()); return 1; }
            {
            Engine::Scene::CommandHistory history;
            Engine::Renderer::RenderWorld renderWorld;
            Engine::Extensions::FeaturePipeline renderer(device);
            result=Engine::Extensions::RegisterBuiltins(renderer,device,executable.parent_path()/"shaders");
            if (!result) { Engine::Log::Error(result.Error());return 1; }
            result = renderer.Initialize(executable.parent_path() / "shaders");
            if (!result) { Engine::Log::Error(result.Error()); return 1; }
            Engine::Assets::Registry registry(executable.parent_path()/"asset-cache");
            result=registry.Open();if (!result) { Engine::Log::Error(result.Error());return 1; }
            if (instanceDemo) result=BuildDemoScene(world,history);
            else {
                if (gltfPath.empty()) gltfPath=executable.parent_path()/(lightingGrid?"assets/lighting_grid.gltf":"assets/material_gallery.gltf");
                Engine::Assets::ImportedScene imported;result=registry.Import(gltfPath,imported);
                if (result) result=renderer.UploadAssets(imported.assets);
                if (result) result=BuildImportedScene(imported,world,history);
                Engine::Log::Info("Imported meshes: "+std::to_string(imported.assets.meshes.size())+", materials: "+std::to_string(imported.assets.materials.size())+", textures: "+std::to_string(imported.assets.textures.size()));
            }
            if (!result) { Engine::Log::Error(result.Error());return 1; }
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
            result=Engine::Sky::EnsureSettings(world,history);if(!result){Engine::Log::Error(result.Error());return 1;}
            Engine::Sky::SceneSystems skySystems;double skyElapsed=0;Engine::Sky::Settings skySettings;Engine::Guid skyConfig,skySun,skyMoon;
            result=Engine::Sky::ReadSettings(world,skySettings,skyConfig,skySun,skyMoon);if(!result)return 1;
            if(skySmoke||skyBenchmark||skyTimeExplicit){result=history.Execute(world,std::make_unique<Engine::Scene::SetFieldCommand>(skySun,Engine::Sky::kCelestial,"directionSource",std::uint32_t{0}));if(!result)return 1;}
            result=history.Execute(world,std::make_unique<Engine::Scene::SetFieldCommand>(skyConfig,Engine::Sky::kTime,"time",skyTime));if(!result)return 2;
            result=history.Execute(world,std::make_unique<Engine::Scene::SetFieldCommand>(skyConfig,Engine::Sky::kClouds,"quality",std::uint32_t(skyQuality)));if(!result)return 2;
            if(auto sky=Engine::Sky::FindSky(renderer.Features()))sky->DebugView(skyDebug);
            std::vector<Engine::Sky::WeatherPreset> skyPresets;result=Engine::Sky::LoadWeatherPresets(executable.parent_path()/"sky/weather-presets.json",skyPresets);if(!result)return 1;
            std::vector<double> skyTimings;

#endif
            if(lightingGrid)for(auto& entry:renderer.Features().Entries())if(std::string_view(entry.feature->Name())!="Tonemap")entry.enabled=false;
            if(lightingSmoke)for(auto& entry:renderer.Features().Entries())if(std::string_view(entry.feature->Name())!="Tonemap"&&std::string_view(entry.feature->Name())!="Lighting")entry.enabled=false;
            if(advancedLighting){result=BuildLightingScenario(world,history,executable.parent_path()/"assets/studio.hdr");if(!result){Engine::Log::Error(result.Error());return 1;}for(auto& entry:renderer.Features().Entries())if(std::string_view(entry.feature->Name())=="Procedural Sky"||std::string_view(entry.feature->Name())=="Bloom")entry.enabled=false;}
            std::vector<std::byte> loopPixels;double cullTotal=0;std::uint32_t cullSamples=0;std::uint64_t lightUploadBytes=0,lightPacks=0;std::vector<double> lightCpuTimes;
            Engine::Log::Info("Window opened. Escape or close the window to exit.");
            int frame = 0;
            const auto started = std::chrono::steady_clock::now();
            std::vector<double> frameTimes;
            while (!window.ShouldClose()) {
                const auto frameStarted=std::chrono::steady_clock::now();
                ENGINE_PROFILE_SCOPE("Runtime frame");
                if (resizeSmoke) {
                    if (frame == 10) window.Resize(800, 600);
                    if (frame == 25) window.Resize(1600, 900);
                    if (frame == 40) window.Resize(640, 480);
                    if (frame == 55) window.Minimize();
                    if (frame == 65) { window.Restore(); window.Resize(1024, 768); }
                    if (frame == 90) window.Resize(1280, 720);
                }
                window.PollEvents();
                if (window.IsMinimized()) window.PumpEvents(0.05);
                else {
                    if(lightingSmoke){
                        using namespace Engine;
                        using namespace Engine::Scene;
                        const Guid sun{1,3},camera{1,2};
                        if(frame==10)result=history.Execute(world,std::make_unique<SetFieldCommand>(sun,ComponentType::Transform,"rotation",Vec3{-.6f,.8f,0}));
                        else if(frame==20)result=history.Execute(world,std::make_unique<SetFieldCommand>(sun,Lighting::kDirectionalLight,"illuminance",20000.f));
                        else if(frame==30)result=Lighting::ApplyPreset(world,history,2);
                        else if(frame==40||frame==60||frame==80)result=history.Undo(world);
                        else if(frame==50)result=history.Execute(world,std::make_unique<SetFieldCommand>(sun,Lighting::kDirectionalLight,"enabled",std::uint32_t{0}));
                        else if(frame==70)result=Lighting::ChangeIntensityUnit(world,history,sun,Lighting::kDirectionalLight,2);
                        else if(frame==90)result=history.Execute(world,std::make_unique<SetFieldCommand>(camera,ComponentType::Camera,"ev100",16.f));
                        else if(frame==100){EntityDefinition point;point.id={20,1};point.name.value="L1 point authoring smoke";ModuleComponent component;Lighting::PointLight settings;settings.common.unit=1;settings.intensity=1500;result=Lighting::Encode(Lighting::kPointLight,&settings,component);if(result){point.extensions.push_back(std::move(component));result=history.Execute(world,std::make_unique<CreateEntityCommand>(point));}}
                        if(!result){Log::Error(result.Error());return 1;}
                    }
                    if (instanceDemo && frame==5) {
                        Engine::Scene::EntityDefinition added; added.id={3,1}; added.name.value="Added after startup"; added.mesh=Engine::Scene::MeshRenderer{};
                        added.transform.position={0,8,0}; added.transform.scale={3,3,3}; added.mesh->color={1,0.25f,0.08f};
                        result=history.Execute(world,std::make_unique<Engine::Scene::CreateEntityCommand>(added));
                        if (!result) { Engine::Log::Error(result.Error()); return 1; }
                    }
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
                    if(skySmoke){
                        if(frame==40||frame==80||frame==220){float hour=frame==40?6.f:frame==80?0.f:18.f;result=history.Execute(world,std::make_unique<Engine::Scene::SetFieldCommand>(skyConfig,Engine::Sky::kTime,"time",hour));if(!result)return 1;}
                        if(frame==100||frame==120||frame==140){auto quality=frame==100?2u:frame==120?0u:1u;result=history.Execute(world,std::make_unique<Engine::Scene::SetFieldCommand>(skyConfig,Engine::Sky::kClouds,"quality",quality));if(!result)return 1;}
                        if(frame==150){result=Engine::Sky::ApplyWeatherPreset(world,history,skyConfig,skyPresets.back().parameters,.2f);if(!result)return 1;}
                        if(auto sky=Engine::Sky::FindSky(renderer.Features())){
                            if(frame>=160&&frame<180)sky->DebugView((frame-160)/2+1);if(frame==180){sky->DebugView(0);sky->Layers().SetEnabled("Aurora",true);sky->Layers().SetPriority("Stars",25);}
                            if(frame==200){sky->Layers().SetEnabled("SkyboxCubemap",true);sky->Layers().SetEnabled("Atmosphere",false);sky->Layers().SetEnabled("Volumetric clouds",false);}
                            if(frame==220){sky->Layers().SetEnabled("SkyboxCubemap",false);sky->Layers().SetEnabled("Atmosphere",true);sky->Layers().SetEnabled("Volumetric clouds",true);}
                        }
                    }
                    if(auto sky=Engine::Sky::FindSky(renderer.Features())){Engine::Sky::Settings settings;Engine::Sky::CelestialState celestial;double dt=frameTimes.empty()?1.0/60:frameTimes.back()/1000;skyElapsed+=dt;result=skySystems.Update(world,dt,settings,celestial);if(!result){Engine::Log::Error(result.Error());return 1;}sky->Snapshot(settings,celestial,skyElapsed);}
#endif
                    if(advancedLighting){using namespace Engine;using namespace Scene;if(frame==30||frame==50)result=history.Execute(world,std::make_unique<SetFieldCommand>(Guid{30,1},Lighting::kEnvironmentSettings,"provider",std::uint32_t(frame==30?0:1)));if(frame==70)result=history.Execute(world,std::make_unique<SetFieldCommand>(Guid{32,2},ComponentType::Transform,"position",Vec3{-2,2,2}));if(frame>=100&&frame<113)renderer.SetDebugView(frame-100);if(frame==113)renderer.SetDebugView(0);if(!result){Engine::Log::Error(result.Error());return 1;}}
                    result=world.Extract(renderWorld); if (!result) { Engine::Log::Error(result.Error()); return 1; }
                    if(evExplicit||lightingGrid)renderWorld.camera.ev100=ev100;
                    if(lightingCompare){renderWorld.settings.autoExposure=0;renderWorld.modified=true;for(auto& entry:renderer.Features().Entries())if(std::string_view(entry.feature->Name())=="Lighting")*entry.feature->Parameters()[0].value=frame==4||frame==6?1.f:2.f;if(frame>=6){std::uint32_t modified=0;for(auto& light:renderWorld.lights)if(modified<300&&light.type==1&&light.modulePacked&&(light.gpu.flags&2)==0){light.range=100;light.gpu.invRangeSq=.0001f;++modified;}}}
                    if(lightingGrid){
                        // L0 reference is intentionally one fixed 100,000 lux light.
                        renderWorld.lights.resize(1);renderWorld.lights[0]={0,{}, {-.45f,-.6f,-1.f},{1,1,1},100000};
                        renderWorld.modified=true;
                    }
                    result = renderer.Render(renderWorld);
                    if (!result) { Engine::Log::Error(result.Error()); return 1; }
                    if(lightingCompare&&frame>=4&&frame<=7){std::vector<std::byte> pixels;result=device.ReadImage(renderer.LastGeometryImage(),pixels);if(!result){Engine::Log::Error(result.Error());return 1;}if(frame%2==0)loopPixels=std::move(pixels);else{double maximum=0,squared=0;auto a=reinterpret_cast<const std::uint16_t*>(loopPixels.data()),b=reinterpret_cast<const std::uint16_t*>(pixels.data());for(std::size_t i=0;i<pixels.size()/2;++i){float x=glm::unpackHalf1x16(a[i]),y=glm::unpackHalf1x16(b[i]);if(!std::isfinite(x)||!std::isfinite(y)){Engine::Log::Error("Nonfinite HDR output");return 1;}double delta=std::abs(x-y)/std::max(1.f,std::abs(x));maximum=std::max(maximum,delta);squared+=delta*delta;}Engine::Log::Info("GPU loop / cluster image comparison "+std::string(frame==5?"normal":"overflow")+": max relative error "+std::to_string(maximum)+", RMSE "+std::to_string(std::sqrt(squared/(pixels.size()/2))));if(maximum>.003){Engine::Log::Error("Loop / cluster image mismatch");return 1;}}}
                    if(advancedLighting){const auto& stats=renderer.LightingStatistics();if(lightingCompare&&frame==9&&stats.overflow==0){Engine::Log::Error("Overflow fixture failed to overflow");return 1;}if(!std::isfinite(stats.exposureEv)||stats.exposureEv<renderWorld.settings.minEv||stats.exposureEv>renderWorld.settings.maxEv){Engine::Log::Error("Exposure left its configured range");return 1;}if(!lightingCompare&&frame==99&&std::abs(stats.exposureEv-15)<.01){Engine::Log::Error("Auto exposure did not adapt");return 1;}if((frame<101||frame>=105)&&stats.localCount!=1011){Engine::Log::Error("Advanced local-light count mismatch");return 1;}if(frame==6&&stats.shadowTiles<47){Engine::Log::Error("Expected four cascades and all eight local shadow allocations");return 1;}if(frame>5&&frame<70&&stats.shadowPasses!=0){Engine::Log::Error("Static shadow cache unexpectedly redrew tiles");return 1;}if(frame==70&&stats.shadowPasses==0){Engine::Log::Error("Moved shadow light did not invalidate tiles");return 1;}if(frame>=10&&frame<100)for(auto& timing:device.PassTimings())if(timing.name=="Lighting light cull"){cullTotal+=timing.gpuMilliseconds;++cullSamples;}}
                    if(lightingSmoke){
                        const auto& stats=renderer.LightingStatistics();lightUploadBytes+=stats.uploadedBytes;lightPacks+=stats.packedLights;
                        // Both fenced frame buffers must receive each edit once; camera edits write no light data.
                        bool lightEdit=frame>=10&&frame<=81&&frame%10<=1;
                        if(frame>=5&&frame<100&&stats.uploadedBytes!=(lightEdit?64u:0u)){Engine::Log::Error("L1 dirty GPU upload byte count failed at frame "+std::to_string(frame)+": "+std::to_string(stats.uploadedBytes));return 1;}
                        if(frame==30&&renderWorld.camera.ev100!=5){Engine::Log::Error("L1 preset exposure was overwritten");return 1;}
                        if(frame==40&&renderWorld.camera.ev100!=15){Engine::Log::Error("L1 preset undo did not restore exposure");return 1;}
                        if(frame==100&&stats.localCount!=2){Engine::Log::Error("L1 structural light rebuild failed");return 1;}
                        if(frame>=5)for(const auto& timing:renderer.CpuPassTimes())if(timing.first=="Lighting buffers ready")lightCpuTimes.push_back(timing.second);
                    }
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
                    if(orbitalSmoke){auto sky=Engine::Sky::FindSky(renderer.Features());if(frame<70){static const float heights[]{0,1,100,10000,100000,1000000,40000000};sky->ForcedAltitude(heights[frame/10]);}else if(frame<120)sky->ForcedAltitude(50000+float(frame-70)*1400);else{sky->ForcedAltitude(frame<132?0.f:1000000.f);float angles[]{0,80,90,95,100,120};float zenith=angles[(frame-120)/2%6];result=history.Execute(world,std::make_unique<Engine::Scene::SetFieldCommand>(skySun,Engine::Scene::ComponentType::Transform,"rotation",Engine::Vec3{zenith*.0174532925f-1.57079633f,0,0}));}if(frame>=144){sky->ForcedAltitude(100000);history.Execute(world,std::make_unique<Engine::Scene::SetFieldCommand>(skySun,Engine::Scene::ComponentType::Transform,"rotation",Engine::Vec3{-.174533f,0,0}));}sky->DebugView(frame==145||frame==146||frame==147?13:frame==148?14:0);if(frame%10==0)Engine::Log::Info("Orbital sweep altitude m: "+std::to_string(sky->Altitude()));if(frame==149){auto error=sky->ReferenceError();if(!std::isfinite(error.x)||!std::isfinite(error.y)||!std::isfinite(error.z)){Engine::Log::Error("Nonfinite orbital reference error");return 1;}Engine::Log::Info("Orbital reference RMSE RGB: "+std::to_string(error.x)+" / "+std::to_string(error.y)+" / "+std::to_string(error.z));}}
                    if(frame>=15&&(skyBenchmark||(skySmoke&&frame<40))){double total=0;for(auto& t:device.PassTimings())if(t.name.starts_with("Sky "))total+=t.gpuMilliseconds;skyTimings.push_back(total);}
#endif
                    if (frame>=10) frameTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStarted).count());
                }
                ENGINE_PROFILE_FRAME();
                ++frame;
                if (smokeFrames > 0 && frame >= smokeFrames) window.RequestClose();
                if (smokeFrames > 0 && std::chrono::steady_clock::now() - started > std::chrono::seconds(30)) {
                    Engine::Log::Error("Smoke test timed out"); return 1;
                }
            }
            if(advancedLighting)Engine::Log::Info("L2-L8 smoke: 1000 clustered points, spot/point cached shadows, ambient/HDRI switching, fog, automatic exposure, rect/disk LTC; cull mean ms "+std::to_string(cullTotal/std::max(1u,cullSamples)));
            if(lightingSmoke){double mean=lightCpuTimes.empty()?0:std::accumulate(lightCpuTimes.begin(),lightCpuTimes.end(),0.0)/lightCpuTimes.size();Engine::Log::Info("L1 smoke passed: live sun rotation, physical intensity, presets and undo, enabled, units, camera-only edits, structural point creation; light uploads "+std::to_string(lightUploadBytes)+" bytes, packed "+std::to_string(lightPacks)+", readiness-pass CPU mean ms "+std::to_string(mean));}
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
            if(!skyTimings.empty()){std::sort(skyTimings.begin(),skyTimings.end());Engine::Log::Info("F4 1080p sky GPU ms: mean "+std::to_string(std::accumulate(skyTimings.begin(),skyTimings.end(),0.0)/skyTimings.size())+", p95 "+std::to_string(skyTimings[(skyTimings.size()*95+99)/100-1])+", samples "+std::to_string(skyTimings.size()));}
            if(skySmoke){Engine::Sky::Settings finalSettings;Engine::Guid a,b,c;result=Engine::Sky::ReadSettings(world,finalSettings,a,b,c);if(!result||finalSettings.weather.current.coverage<.8f){Engine::Log::Error("F4 weather transition failed");return 1;}Engine::Log::Info("F4 smoke passed: day/night, quality rebuilds, weather, debug views, layer ordering, aurora and cubemap fallback");}
#endif
            for (const auto& timing : device.PassTimings())
                Engine::Log::Info(timing.name + " GPU ms: " + std::to_string(timing.gpuMilliseconds));
            Engine::Log::Info("Scene entities: "+std::to_string(world.EntityCount())+", rendered instances: "+std::to_string(renderer.LastInstanceCount()));
            if (instanceDemo && smokeFrames>5 && renderer.LastInstanceCount()!=10001) { Engine::Log::Error("Added entity missing from Render World"); return 1; }
            if (!frameTimes.empty()) {
                std::sort(frameTimes.begin(),frameTimes.end());
                const auto average=std::accumulate(frameTimes.begin(),frameTimes.end(),0.0)/static_cast<double>(frameTimes.size());
                const auto percentile=[&](std::size_t percent) { return frameTimes[(frameTimes.size()*percent+99)/100-1]; };
                Engine::Log::Info("Frame work ms: mean "+std::to_string(average)+", p50 "+std::to_string(percentile(50))+", p95 "+std::to_string(percentile(95)));
            }
            }
            result = device.Shutdown();
            const auto statistics = device.Statistics();
            Engine::Log::Info("Presented frames: " + std::to_string(statistics.presentedFrames)
                + ", swapchain generations: " + std::to_string(statistics.swapchainGenerations)
                + ", validation warnings/errors: " + std::to_string(statistics.validationMessages)
                + ", GPU frame ms: " + std::to_string(statistics.gpuFrameMilliseconds));
            if (!result) { Engine::Log::Error(result.Error()); return 1; }
            if (smokeFrames > 0 && statistics.presentedFrames == 0) return 1;
            if (resizeSmoke && (statistics.swapchainGenerations < 6 || statistics.presentedFrames < 90)) {
                Engine::Log::Error("Resize smoke did not exercise enough recreations/presentations"); return 1;
            }
        }
        modules.Shutdown();
        Engine::Log::Info("Engine shutdown complete");
        Engine::Log::Flush();
        return 0;
    } catch (const std::exception& error) {
        Engine::Log::Error(error.what());
        return 1;
    }
}

