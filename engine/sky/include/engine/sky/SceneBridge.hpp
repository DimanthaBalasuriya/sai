#pragma once
#include "Layers.hpp"
#include "engine/scene/Commands.hpp"
namespace Engine::Sky {
inline constexpr Scene::ComponentType kTime=static_cast<Scene::ComponentType>(0x534b5900),kAtmosphere=static_cast<Scene::ComponentType>(0x534b5901),kCelestial=static_cast<Scene::ComponentType>(0x534b5902),kStars=static_cast<Scene::ComponentType>(0x534b5903),kClouds=static_cast<Scene::ComponentType>(0x534b5904),kWeather=static_cast<Scene::ComponentType>(0x534b5905);
Result RegisterComponents();
Result EncodeComponent(Scene::ComponentType,const void*,Scene::ModuleComponent&);
Result DecodeComponent(const Scene::ModuleComponent&,void*);
Result ReadSettings(Scene::World&,Settings&,Guid& settingsEntity,Guid& sunEntity,Guid& moonEntity);
Result EnsureSettings(Scene::World&,Scene::CommandHistory&);
class SceneSystems {
public:Result Update(Scene::World&,double seconds,Settings&,CelestialState&);
private:WeatherSystem m_weather;WeatherParameters m_previousWeather;VolumetricClouds m_previousClouds;bool m_previous=false;
};
}
