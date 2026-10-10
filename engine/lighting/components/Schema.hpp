#pragma once
#include "engine/lighting/Lighting.hpp"
#include <cstddef>
namespace Engine::Lighting {
#define LF(T,F,K,LO,HI) Scene::FieldInfo{#F,Scene::FieldType::K,offsetof(T,F),LO,HI}
#define LC(T,F,K,LO,HI) Scene::FieldInfo{#F,Scene::FieldType::K,offsetof(T,common)+offsetof(CommonLight,F),LO,HI}
#define COMMON_FIELDS(T) LC(T,color,Vector3,0,1),LC(T,temperature,Float,1000,40000),LC(T,useTemperature,Unsigned,0,1),LC(T,unit,Unsigned,0,2),LC(T,enabled,Unsigned,0,1),LC(T,castShadows,Unsigned,0,1),LC(T,shadowResolution,Unsigned,128,4096),LC(T,filterQuality,Unsigned,0,3),LC(T,depthBias,Float,0,.1f),LC(T,normalBias,Float,0,10)
const LightType& DirectionalType();
const LightType& PointType();
const LightType& SpotType();
const LightType& RectType();
const LightType& DiskType();
}
