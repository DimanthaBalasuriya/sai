#include "EditorApp.hpp"
#include <imgui.h>
#ifdef ENGINE_FEATURE_PROCEDURAL_SKY
void EditorApp::SkyPanel(){
 using namespace Engine;auto sky=Sky::FindSky(m_pipeline.Features());if(!sky)return;
 Sky::Settings settings;Guid config,sun,moon;auto r=Sky::ReadSettings(m_world,settings,config,sun,moon);if(!r){ImGui::TextWrapped("%s",r.Error().c_str());return;}
 if(!config){if(ImGui::Button("Add sky settings"))Report(Sky::EnsureSettings(m_world,m_history));return;}
 ImGui::Text("Atmosphere: %s | altitude %.1f km",sky->SpaceBlend()<=0?"Ground":sky->SpaceBlend()>=1?"Space":"Blend",sky->Altitude()/1000);float forced=sky->ForcedAltitude();bool force=forced>=0;if(ImGui::Checkbox("Force altitude",&force))sky->ForcedAltitude(force?0.f:-1.f);if(force){forced=std::max(0.f,forced);if(ImGui::SliderFloat("Altitude (meters)",&forced,0,40000000,"%.0f",ImGuiSliderFlags_Logarithmic))sky->ForcedAltitude(forced);}
 auto error=sky->ReferenceError();if(sky->DebugView()==13)ImGui::Text("Reference RMSE RGB: %.6f / %.6f / %.6f",error.x,error.y,error.z);auto field=[&](Scene::ComponentType type,const char* name,Scene::FieldValue value){Report(m_history.Execute(m_world,std::make_unique<Scene::SetFieldCommand>(config,type,name,std::move(value)),ImGui::IsItemActive()&&!ImGui::IsItemActivated()));};
 if(ImGui::Button("Inspect sky components")){m_selected=config;m_selection.Click(config,false,false);}ImGui::SameLine();
 if(ImGui::Button("Sun")){m_selected=sun;m_selection.Click(sun,false,false);}ImGui::SameLine();if(ImGui::Button("Moon")){m_selected=moon;m_selection.Click(moon,false,false);}
 int directionSource=static_cast<int>(settings.sun.directionSource);const char* sources[]{"Time of day","Manual celestial direction","Scene light transform"};
 if(ImGui::Combo("Sun direction source",&directionSource,sources,3)&&sun)Report(m_history.Execute(m_world,std::make_unique<Scene::SetFieldCommand>(sun,Sky::kCelestial,"directionSource",std::uint32_t(directionSource))));
 float time=settings.time.time;if(ImGui::SliderFloat("Time of day",&time,0,23.999f,"%.2f h"))field(Sky::kTime,"time",time);
 bool paused=settings.time.paused!=0;if(ImGui::Checkbox("Pause clock",&paused))field(Sky::kTime,"paused",std::uint32_t(paused));
 int quality=static_cast<int>(settings.clouds.quality);const char* qualities[]{"Low","Medium","High"};if(ImGui::Combo("Quality",&quality,qualities,3))field(Sky::kClouds,"quality",std::uint32_t(quality));
 float coverage=settings.clouds.coverage;if(ImGui::SliderFloat("Cloud coverage",&coverage,0,1))field(Sky::kClouds,"coverage",coverage);
 int debug=static_cast<int>(sky->DebugView());const char* views[]{"Composite","Transmittance","Multiple scattering","Sky view","Cloud density","Cloud steps","Stars","Celestial discs","Aerial froxels","Weather map","Cloud shadow","Planet visibility","Sun transmission","Reference difference","Reference (128 samples)"};if(ImGui::Combo("Debug view",&debug,views,15))sky->DebugView(debug);
 ImGui::SeparatorText("Weather");static std::vector<Sky::WeatherPreset> presets;static bool loaded=false;
 if(!loaded){loaded=true;Report(Sky::LoadWeatherPresets(m_directory/"sky/weather-presets.json",presets));}
 static float duration=10;ImGui::SliderFloat("Transition seconds",&duration,0,120);
 for(auto& preset:presets){if(ImGui::Button(preset.name.c_str()))Report(Sky::ApplyWeatherPreset(m_world,m_history,config,preset.parameters,duration));ImGui::SameLine();}ImGui::NewLine();
 ImGui::Text("Fog %.6f  Wetness %.2f  Rain %.2f",settings.weather.current.fogDensity,settings.weather.current.wetness,settings.weather.current.precipitation);
 ImGui::SeparatorText("Sky layers");
 for(auto& entry:sky->Layers().Entries()){ImGui::PushID(entry.layer->Name());bool enabled=entry.enabled;if(ImGui::Checkbox(entry.layer->Name(),&enabled))sky->Layers().SetEnabled(entry.layer->Name(),enabled);ImGui::SameLine();ImGui::SetNextItemWidth(70);int priority=entry.priority;if(ImGui::DragInt("Order",&priority,1,-100,100))Report(sky->Layers().SetPriority(entry.layer->Name(),priority));ImGui::PopID();}
 if(ImGui::Button("Use cubemap fallback")){sky->Layers().SetEnabled("SkyboxCubemap",true);sky->Layers().SetEnabled("Atmosphere",false);}ImGui::SameLine();if(ImGui::Button("Use procedural sky")){sky->Layers().SetEnabled("SkyboxCubemap",false);sky->Layers().SetEnabled("Atmosphere",true);}
}
#endif
