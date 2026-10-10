#pragma once
#include "Components.hpp"
#include <filesystem>
namespace Engine::Sky {
struct CelestialState {Vec3 sun,moon;float moonPhase=0,daylight=0,starsVisibility=0;};
CelestialState EvaluateCelestials(const TimeOfDay&,const CelestialBody&,const CelestialBody&);
Result AdvanceTime(TimeOfDay&,double seconds);
struct WeatherPreset {std::string name;WeatherParameters parameters;};
Result LoadWeatherPresets(const std::filesystem::path&,std::vector<WeatherPreset>&);
class WeatherSystem {
public:
    Result Transition(WeatherState&,const WeatherParameters&,float duration);
    Result Update(WeatherState&,VolumetricClouds&,double seconds);
private:
    WeatherParameters m_start,m_target;double m_elapsed=0;float m_duration=0;bool m_active=false;
};
}
