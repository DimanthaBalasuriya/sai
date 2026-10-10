#include "engine/lighting/Lighting.hpp"
#include <algorithm>
namespace Engine::Lighting {
namespace {
class EditGroup final:public Scene::ICommand {
public:
    void Add(Guid entity,Scene::ComponentType type,const char* field,Scene::FieldValue value){m_edits.push_back(std::make_unique<Scene::SetFieldCommand>(entity,type,field,std::move(value)));}
    Result Do(Scene::World& world)override{
        for(std::size_t i=0;i<m_edits.size();++i){auto result=m_edits[i]->Do(world);if(!result){while(i) m_edits[--i]->Undo(world);return result;}}
        return Result::Success();
    }
    Result Undo(Scene::World& world)override{for(std::size_t i=m_edits.size();i;){auto result=m_edits[--i]->Undo(world);if(!result)return result;}return Result::Success();}
private:std::vector<std::unique_ptr<Scene::SetFieldCommand>> m_edits;
};
}
Result SoloLight(Scene::World& world,Scene::CommandHistory& history,Guid selected){auto group=std::make_unique<EditGroup>();std::vector<Guid> entities;for(auto& type:LightTypes()){auto r=world.EntitiesWithComponent(type.component,entities);if(!r)return r;for(auto id:entities)group->Add(id,type.component,"enabled",std::uint32_t(id==selected));}return history.Execute(world,std::move(group));}
std::span<const Preset> Presets(){
    // Coupled lux/EV100 pairs: daylight, diffuse overcast, room illuminance, moonlight.
    static constexpr Preset presets[]{{"Sunny day",100000,15},{"Overcast",20000,13},{"Indoor",50,5},{"Night",.3f,-2}};return presets;
}
Result ApplyPreset(Scene::World& world,Scene::CommandHistory& history,std::size_t index){
    if(index>=Presets().size())return Result::Failure("Unknown lighting preset");
    std::vector<Guid> cameras,suns,legacy;auto r=world.EntitiesWithComponent(Scene::ComponentType::Camera,cameras);if(!r)return r;
    r=world.EntitiesWithComponent(kDirectionalLight,suns);if(!r)return r;
    r=world.EntitiesWithComponent(Scene::ComponentType::Light,legacy);if(!r)return r;
    for(auto id:legacy){Scene::FieldValue type;r=world.ReadField(id,Scene::ComponentType::Light,"type",type);if(!r)return r;if(std::get<std::uint32_t>(type)==0)suns.push_back(id);}
    if(cameras.empty()||suns.empty())return Result::Failure("Lighting presets need a camera and a directional light");
    auto less=[](Guid a,Guid b){return a.high<b.high||(a.high==b.high&&a.low<b.low);};
    auto camera=*std::min_element(cameras.begin(),cameras.end(),less),sun=*std::min_element(suns.begin(),suns.end(),less);const auto& preset=Presets()[index];
    auto group=std::make_unique<EditGroup>();
    if(world.HasComponent(sun,kDirectionalLight)){
        group->Add(sun,kDirectionalLight,"unit",std::uint32_t{0});group->Add(sun,kDirectionalLight,"illuminance",preset.lux);
        group->Add(sun,kDirectionalLight,"enabled",std::uint32_t{1});group->Add(sun,kDirectionalLight,"useTemperature",std::uint32_t{0});group->Add(sun,kDirectionalLight,"color",Vec3{1,1,1});
    }else group->Add(sun,Scene::ComponentType::Light,"intensity",preset.lux);
    group->Add(camera,Scene::ComponentType::Camera,"ev100",preset.ev100);group->Add(camera,Scene::ComponentType::Camera,"exposureCompensation",0.f);
    return history.Execute(world,std::move(group));
}
Result ChangeIntensityUnit(Scene::World& world,Scene::CommandHistory& history,Guid entity,Scene::ComponentType type,std::uint32_t unit){
    if(!FindLightType(type)||unit>2||(type==kDirectionalLight&&unit==1))return Result::Failure("Invalid light intensity unit");
    auto field=type==kDirectionalLight?"illuminance":"intensity";Scene::FieldValue oldUnit,oldValue,angle;
    auto r=world.ReadField(entity,type,"unit",oldUnit);if(!r)return r;r=world.ReadField(entity,type,field,oldValue);if(!r)return r;
    float outer=.523599f;if(type==kSpotLight){r=world.ReadField(entity,type,"outerCone",angle);if(!r)return r;outer=std::get<float>(angle);}
    float physical=PhysicalIntensity(type,std::get<float>(oldValue),std::get<std::uint32_t>(oldUnit),outer,EmittingArea(world,entity,type)),converted=physical;
    if(unit==1&&(type==kRectLight||type==kDiskLight))converted=physical*3.14159265f*EmittingArea(world,entity,type);else if(unit==1)converted=physical*(type==kSpotLight?SpotSolidAngle(outer):12.566370614359f);
    if(unit==2)converted=physical/(type==kDirectionalLight?100000.f:type==kRectLight||type==kDiskLight?1000.f:300.f);
    auto group=std::make_unique<EditGroup>();group->Add(entity,type,field,converted);group->Add(entity,type,"unit",unit);return history.Execute(world,std::move(group));
}
}
