#include "engine/lighting/Lighting.hpp"
#include "engine/scene/Commands.hpp"
#include <cmath>
#include "DemoScene.hpp"
#include <algorithm>
#include <limits>
// Runtime fixture owns edit policy; engine scene/renderer do not depend on it.
Engine::Result BuildDemoScene(Engine::Scene::World& world,Engine::Scene::CommandHistory& history) {
    using namespace Engine;
    using namespace Engine::Scene;
    const auto create=[&](EntityDefinition definition) { return history.Execute(world,std::make_unique<CreateEntityCommand>(std::move(definition))); };
    EntityDefinition root; root.id={1,1}; root.name.value="Cube grid";
    auto result=create(root); if (!result) return result;
    for (std::uint64_t z=0;z<100;++z) for (std::uint64_t x=0;x<100;++x) {
        EntityDefinition entity; entity.id={2,z*100+x+1}; entity.name.value="Cube "+std::to_string(z*100+x);
        entity.hierarchy.parent=root.id;
        entity.transform.position={(static_cast<float>(x)-49.5f)*1.5f,std::sin(static_cast<float>(x)*0.15f)*std::cos(static_cast<float>(z)*0.13f)*3,(static_cast<float>(z)-49.5f)*1.5f};
        entity.transform.scale={0.45f,0.45f,0.45f}; entity.mesh=MeshRenderer{};
        entity.mesh->color={0.4f+static_cast<float>(x)/165,0.45f+static_cast<float>(z)/180,0.8f};
        result=create(std::move(entity)); if (!result) return result;
    }
    EntityDefinition camera; camera.id={1,2}; camera.name.value="Main camera"; camera.camera=Camera{};
    camera.transform.position={0,105,160}; camera.transform.rotation={-0.58f,0,0}; result=create(camera); if (!result) return result;
    EntityDefinition sun; sun.id={1,3}; sun.name.value="Directional light"; Lighting::DirectionalLight sunlight;Scene::ModuleComponent sunComponent;result=Lighting::Encode(Lighting::kDirectionalLight,&sunlight,sunComponent);if(!result)return result;sun.extensions.push_back(std::move(sunComponent));
    sun.transform.rotation={-0.9f,-0.45f,0};  result=create(sun); if (!result) return result;
    EntityDefinition point; point.id={1,4}; point.name.value="Warm point light"; point.light=Light{};
    point.light->type=1; point.light->color={1,0.45f,0.12f}; point.light->intensity=8; point.light->range=110;
    point.transform.position={0,20,0}; result=create(point);
    history.Clear(); return result;
}
Engine::Result BuildImportedScene(const Engine::Assets::ImportedScene& scene,Engine::Scene::World& world,Engine::Scene::CommandHistory& history) {
    auto result=Engine::Assets::Instantiate(scene,world,history); if (!result) return result;
    Engine::Renderer::RenderWorld snapshot; result=world.Extract(snapshot); if (!result) return result;
    Engine::Vec3 minimum{1e30f,1e30f,1e30f},maximum{-1e30f,-1e30f,-1e30f};
    for (const auto& instance:snapshot.instances) for (const auto& mesh:scene.assets.meshes) if (mesh.id==instance.mesh) for (const auto& vertex:mesh.vertices) {
        const auto& m=instance.model.values;
        Engine::Vec3 p{m[0]*vertex.x+m[4]*vertex.y+m[8]*vertex.z+m[12],m[1]*vertex.x+m[5]*vertex.y+m[9]*vertex.z+m[13],m[2]*vertex.x+m[6]*vertex.y+m[10]*vertex.z+m[14]};
        minimum={std::min(minimum.x,p.x),std::min(minimum.y,p.y),std::min(minimum.z,p.z)};maximum={std::max(maximum.x,p.x),std::max(maximum.y,p.y),std::max(maximum.z,p.z)};
    }
    Engine::Vec3 center{(minimum.x+maximum.x)*0.5f,(minimum.y+maximum.y)*0.5f,(minimum.z+maximum.z)*0.5f};
    float radius=std::max({maximum.x-minimum.x,maximum.y-minimum.y,maximum.z-minimum.z,1.0f})*0.5f;
    const auto create=[&](Engine::Scene::EntityDefinition entity) { return history.Execute(world,std::make_unique<Engine::Scene::CreateEntityCommand>(std::move(entity))); };
    Engine::Scene::EntityDefinition camera; camera.id={1,2}; camera.name.value="Scene camera";camera.camera=Engine::Scene::Camera{};
    camera.transform.position={center.x,center.y+radius*0.65f,center.z+radius*2.3f};camera.transform.rotation={-0.2754f,0,0};camera.camera->farPlane=std::max(1000.0f,radius*10);
    result=create(camera); if (!result) return result;
    Engine::Scene::EntityDefinition sun;sun.id={1,3};sun.name.value="Sun";Engine::Lighting::DirectionalLight sunlight;Engine::Scene::ModuleComponent sunComponent;result=Engine::Lighting::Encode(Engine::Lighting::kDirectionalLight,&sunlight,sunComponent);if(!result)return result;sun.extensions.push_back(std::move(sunComponent));sun.transform.rotation={-0.8f,-0.5f,0};result=create(sun);if (!result) return result;
    Engine::Scene::EntityDefinition point;point.id={1,4};point.name.value="Fill light";point.light=Engine::Scene::Light{};point.light->type=1;point.light->intensity=8;point.light->range=radius*4;
    point.transform.position={center.x+radius*0.4f,center.y+radius*0.6f,center.z+radius};result=create(point);
    history.Clear();return result;
}
