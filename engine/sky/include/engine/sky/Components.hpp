#pragma once
#include "engine/scene/World.hpp"
namespace Engine::Sky {
struct TimeOfDay {
    float time=12,dayLength=600,speed=1;std::uint32_t paused=1;
    float day=0,latitude=35,longitude=0,dayOfYear=172;
};
struct SkyAtmosphere {
    float planetRadius=6360000,atmosphereHeight=100000;
    Vec3 rayleigh{5.802e-6f,13.558e-6f,33.1e-6f};float rayleighHeight=8000;
    Vec3 mie{3.996e-6f,3.996e-6f,3.996e-6f};float mieHeight=1200,mieAbsorption=4.4e-6f,mieAnisotropy=.8f;
    Vec3 ozone{.65e-6f,1.881e-6f,.085e-6f};float ozoneCenter=25000,ozoneWidth=15000;
    Vec3 groundAlbedo{.15f,.15f,.15f};float sunIntensity=20,exposureHint=1;
};
struct CelestialBody {
    std::uint32_t type=0,directionSource=0; // Sun/Moon; source: 0 time, 1 manual direction, 2 authored light transform.
    Vec3 direction{0,1,0},color{1,.97f,.92f};float angularSize=.533f,intensity=1;
    float phase=.5f,albedo=.12f;Guid light;
};
struct StarField {float density=.6f,brightness=1,magnitudeCutoff=6,twinkle=.15f,colorVariation=.35f,milkyWay=.15f;};
struct VolumetricClouds {
    float altitude=1500,thickness=3500,coverage=.35f,density=.6f,typeBlend=.3f;
    Vec3 wind{12,0,3};float shapeScale=.00035f,detailScale=.003f,shadowStrength=.6f;
    std::uint32_t quality=1;
};
struct WeatherParameters {
    float coverage=.35f,type=.3f,precipitation=0;Vec3 wind{12,0,3};float fogDensity=.000015f,wetness=0;
    bool operator==(const WeatherParameters&) const=default;
};
struct WeatherState {
    WeatherParameters current,target;float duration=10;std::uint32_t transitioning=0;
};
struct Settings {
    TimeOfDay time;SkyAtmosphere atmosphere;CelestialBody sun,moon;
    StarField stars;VolumetricClouds clouds;WeatherState weather;
    Settings(){moon.type=1;moon.angularSize=.518f;moon.color={.65f,.75f,1};moon.intensity=.02f;}
};
struct ComponentSchema {std::string_view name;std::size_t bytes;std::span<const Scene::FieldInfo> fields;};
std::span<const ComponentSchema> Schemas();
Result ReadField(const ComponentSchema&,const void*,std::string_view,Scene::FieldValue&);
Result WriteField(const ComponentSchema&,void*,std::string_view,const Scene::FieldValue&);
Result Validate(const Settings&);
struct QualitySettings {std::uint32_t transmittanceWidth,transmittanceHeight,multiSize,skyWidth,skyHeight,froxelWidth,froxelHeight,froxelDepth,cloudDivisor,cloudSteps,iblInterval;};
QualitySettings Quality(std::uint32_t tier);
}
