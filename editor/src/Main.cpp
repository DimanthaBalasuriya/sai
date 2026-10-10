#include "EditorApp.hpp"
#include "engine/core/EngineContext.hpp"
#include "engine/core/ModuleRegistry.hpp"
#include "engine/core/Log.hpp"
#include "engine/platform/PlatformModule.hpp"
#include "engine/scene/SceneModule.hpp"
#include "engine/assets/AssetsModule.hpp"
#include <charconv>
#include <fstream>
#ifdef _WIN32
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement=1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1;
}
#endif
int main(int argc,char** argv) {
    try {
        std::string gpuOverride;bool gpuExplicit=false;
        int frames=0;bool entityBenchmark=false,exercise=false,themePanel=false,uiSmoke=false,inputSmoke=false;float dpi=0;std::filesystem::path source,theme,bindings;std::string panel;
        for (int i=1;i<argc;++i) {
            std::string_view flag(argv[i]);
            if(flag.starts_with("--gpu=")){gpuOverride=std::string(flag.substr(6));gpuExplicit=true;}
            else if (flag=="--smoke") { exercise=true;frames=180; }
            else if(flag=="--ui-smoke") {exercise=true;uiSmoke=true;frames=240;}
            else if(flag=="--entity-benchmark"){entityBenchmark=true;frames=240;}
            else if(flag=="--input-smoke") {exercise=true;inputSmoke=true;frames=360;}
            else if(flag=="--bindings"&&i+1<argc) bindings=argv[++i];
            else if (flag=="--gltf"&&i+1<argc) source=argv[++i];
            else if (flag=="--theme"&&i+1<argc) theme=argv[++i];
            else if (flag=="--theme-panel") themePanel=true;
            else if(flag=="--panel"&&i+1<argc) panel=argv[++i];
            else if (flag=="--ui-dpi"&&i+1<argc) {std::string_view value(argv[++i]);auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),dpi);if(error!=std::errc{}||end!=value.data()+value.size()||!(dpi>=.5f&&dpi<=4)) return 2;}
            else if (flag=="--smoke-frames"&&i+1<argc) { std::string_view value(argv[++i]);auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),frames);if (error!=std::errc{}||end!=value.data()+value.size()||frames<1) return 2; }
            else return 2;
        }
        const auto directory=std::filesystem::absolute(argv[0]).parent_path();
        if(!gpuExplicit){std::ifstream gpuSetting(directory/"gpu.txt");if(gpuSetting)std::getline(gpuSetting,gpuOverride);}
        Engine::EngineContext context;Engine::ModuleRegistry modules(context);
        auto result=modules.Add(std::make_unique<Engine::PlatformModule>());if (!result) return 1;
        auto scene=std::make_unique<Engine::Scene::SceneModule>();auto& world=scene->GetWorld();result=modules.Add(std::move(scene));if (!result) return 1;
        auto assets=std::make_unique<Engine::Assets::AssetsModule>(directory/"asset-cache");auto& registry=assets->GetRegistry();result=modules.Add(std::move(assets));if (!result) return 1;
        result=modules.Startup();if (!result) { Engine::Log::Error(result.Error());return 1; }
        {
            Engine::Window window;result=window.Open(1600,1000,"Engine Editor | Live Scene");if (!result) return 1;
            Engine::Rhi::Device device;result=device.Initialize(window,gpuOverride);if (!result) { Engine::Log::Error(result.Error());return 1; }
            {
                EditorApp editor(device,window,world,registry,directory);editor.ConfigureUi(theme,dpi,themePanel,uiSmoke,panel);editor.ConfigureInput(bindings,inputSmoke);editor.ConfigureEntityBenchmark(entityBenchmark);result=editor.Initialize(source,frames>0);
                if (result) result=editor.Run(frames,exercise);
                if (!result) { Engine::Log::Error(result.Error());return 1; }
            }
            result=device.Shutdown();auto statistics=device.Statistics();
            Engine::Log::Info("Editor presented frames: "+std::to_string(statistics.presentedFrames)+", generations: "+std::to_string(statistics.swapchainGenerations)+", validation warnings/errors: "+std::to_string(statistics.validationMessages));
            if (!result) { Engine::Log::Error(result.Error());return 1; }
        }
        modules.Shutdown();return 0;
    } catch (const std::exception& error) { Engine::Log::Error(error.what());return 1; }
}
