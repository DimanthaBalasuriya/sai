#include "engine/scene/World.hpp"
#include "engine/scene/EntityFeatures.hpp"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include <cmath>
#include <exception>
#include "engine/core/Profile.hpp"
#include <chrono>
#include <random>
#include <limits>
#include <set>
namespace Engine::Scene {
namespace {
#define FIELD(T,F,K,MIN,MAX) FieldInfo{#F,FieldType::K,offsetof(T,F),MIN,MAX}
const std::array kName{FIELD(Name,value,String,0,0)};
const std::array kTransform{FIELD(Transform,position,Vector3,-1000000,1000000),FIELD(Transform,rotation,Vector3,-1000000,1000000),FIELD(Transform,scale,Vector3,0.0001f,1000000)};
const std::array kHierarchy{FIELD(Hierarchy,parent,Guid,0,0)};
const std::array kMesh{FIELD(MeshRenderer,mesh,Guid,0,0),FIELD(MeshRenderer,material,Guid,0,0),FIELD(MeshRenderer,color,Vector3,0,1)};
const std::array kCamera{FIELD(Camera,verticalFov,Float,0.01f,3.13f),FIELD(Camera,nearPlane,Float,0.001f,1000000),FIELD(Camera,farPlane,Float,0.002f,1000000),FIELD(Camera,ev100,Float,-16,32),FIELD(Camera,exposureCompensation,Float,-16,16)};
const std::array kLight{FIELD(Light,type,Unsigned,0,2),FIELD(Light,color,Vector3,0,1),FIELD(Light,intensity,Float,0,100000000),FIELD(Light,range,Float,0.001f,1000000),FIELD(Light,spotAngle,Float,0.01f,1.56f)};
const std::array kComponents{ComponentInfo{ComponentType::Name,"Name",kName},ComponentInfo{ComponentType::Transform,"Transform",kTransform},
    ComponentInfo{ComponentType::Hierarchy,"Hierarchy",kHierarchy},ComponentInfo{ComponentType::MeshRenderer,"MeshRenderer",kMesh},
    ComponentInfo{ComponentType::Camera,"Camera",kCamera},ComponentInfo{ComponentType::Light,"Light",kLight}};
#undef FIELD
struct RegisteredComponent {
    ComponentType type;std::string name;std::vector<std::string> names;
    std::vector<FieldInfo> fields;std::vector<FieldValue> defaults;
    ComponentValidator validator=nullptr;RenderLightExtractor lightExtractor=nullptr;
    RenderSettingsExtractor settingsExtractor=nullptr;
};
struct ComponentRegistry {
    std::vector<std::unique_ptr<RegisteredComponent>> modules;
    std::vector<ComponentInfo> reflection{kComponents.begin(),kComponents.end()};
};
ComponentRegistry& Registry(){static ComponentRegistry registry;return registry;}
const RegisteredComponent* Registered(ComponentType type){for(auto& entry:Registry().modules)if(entry->type==type)return entry.get();return nullptr;}
struct Hash { std::size_t operator()(Guid id) const noexcept { return std::hash<std::uint64_t>{}(id.high) ^ (std::hash<std::uint64_t>{}(id.low)<<1); } };
glm::vec3 Native(Vec3 value) { return {value.x,value.y,value.z}; }
Mat4 Store(const glm::mat4& value) { Mat4 output; std::copy_n(glm::value_ptr(value),16,output.values.begin()); return output; }
glm::mat4 Native(const Mat4& value) { return glm::make_mat4(value.values.data()); }
const FieldInfo* FindField(ComponentType component,std::string_view name) {
    for (const auto& info:Registry().reflection) if (info.type==component) for (const auto& field:info.fields) if (field.name==name) return &field;
    return nullptr;
}
bool ValidVector(Vec3 value,float minimum,float maximum) {
    return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z)&&value.x>=minimum&&value.y>=minimum&&value.z>=minimum&&value.x<=maximum&&value.y<=maximum&&value.z<=maximum;
}
}
bool IsModuleComponent(ComponentType type) noexcept{return static_cast<std::uint32_t>(type)>=256;}
Result ValidateModuleComponent(const ModuleComponent& component){auto schema=Registered(component.type);if(!schema||schema->fields.size()!=component.fields.size())return Result::Failure("Unknown module schema or field count");for(std::size_t i=0;i<component.fields.size();++i){const auto& field=schema->fields[i];const auto& value=component.fields[i];if(value.index()!=static_cast<std::size_t>(field.type))return Result::Failure("Module field type mismatch");if(field.type==FieldType::Float){float v=std::get<float>(value);if(!std::isfinite(v)||v<field.minimum||v>field.maximum)return Result::Failure("Module field outside range");}if(field.type==FieldType::Vector3&&!ValidVector(std::get<Vec3>(value),field.minimum,field.maximum))return Result::Failure("Module vector outside range");if(field.type==FieldType::Unsigned){auto v=std::get<std::uint32_t>(value);if(v<field.minimum||v>field.maximum)return Result::Failure("Module enum outside range");}}return schema->validator?schema->validator(component):Result::Success();}
Result DefaultComponent(ComponentType type,ModuleComponent& output){auto schema=Registered(type);if(!schema)return Result::Failure("Unknown module component");output={type,schema->defaults};return Result::Success();}
Result RegisterComponent(ComponentType type,std::string_view name,std::span<const FieldInfo> fields,std::span<const FieldValue> defaults,ComponentValidator validator){
    if(!IsModuleComponent(type)||name.empty()||fields.empty()||fields.size()!=defaults.size())return Result::Failure("Invalid module schema");
    for(const auto& info:Registry().reflection)if(info.type==type||info.name==name)return Result::Failure("Duplicate component schema");
    std::set<std::string_view> names;for(const auto& field:fields)if(field.name.empty()||!names.insert(field.name).second||field.type<FieldType::String||field.type>FieldType::Unsigned||!std::isfinite(field.minimum)||!std::isfinite(field.maximum)||field.minimum>field.maximum)return Result::Failure("Invalid module field schema");
    auto entry=std::make_unique<RegisteredComponent>();entry->type=type;entry->validator=validator;entry->name=name;entry->defaults.assign(defaults.begin(),defaults.end());entry->fields.assign(fields.begin(),fields.end());entry->names.reserve(fields.size());for(auto& field:fields)entry->names.emplace_back(field.name);for(std::size_t i=0;i<fields.size();++i)entry->fields[i].name=entry->names[i];
    auto& registry=Registry();registry.modules.push_back(std::move(entry));auto result=ValidateModuleComponent({type,{defaults.begin(),defaults.end()}});if(!result){registry.modules.pop_back();return result;}auto& registered=*registry.modules.back();registry.reflection.push_back({type,registered.name,registered.fields});return Result::Success();
}
Result RegisterRenderSettingsExtractor(ComponentType type,RenderSettingsExtractor extractor){
    if(!extractor)return Result::Failure("Missing settings extractor");
    for(auto& schema:Registry().modules)if(schema->type==type){if(schema->settingsExtractor&&schema->settingsExtractor!=extractor)return Result::Failure("Settings extractor collision");schema->settingsExtractor=extractor;return Result::Success();}
    return Result::Failure("Settings extractor requires a registered schema");
}
Result RegisterRenderLightExtractor(ComponentType type,RenderLightExtractor extractor){
    if(!extractor)return Result::Failure("Missing light extractor");
    for(auto& schema:Registry().modules)if(schema->type==type){if(schema->lightExtractor&&schema->lightExtractor!=extractor)return Result::Failure("Light extractor collision");schema->lightExtractor=extractor;return Result::Success();}
    return Result::Failure("Light extractor requires a registered schema");
}
struct World::Impl {
    Impl() { std::random_device random; source={static_cast<std::uint64_t>(random())<<32|random(),static_cast<std::uint64_t>(random())<<32|random()}; }
    Guid source;
    struct Node {
        Guid id; Mat4 global;
        entt::entity parent=entt::null,first=entt::null,next=entt::null,previous=entt::null;
        std::uint32_t depth=0,children=0,instance=UINT32_MAX,light=UINT32_MAX;
        std::uint64_t version=1;bool dirty=false,renderDirty=false,visible=true,selfVisible=true,isStatic=false;
        Vec3 meshColor{1,1,1},lightColor{1,1,1};float lightIntensity=1;
    };
    struct PackedModuleLight {Renderer::RenderLight value;};
    struct ModuleComponents {std::vector<ModuleComponent> values;std::vector<std::size_t> indices;};
    entt::registry registry;
    std::unordered_map<Guid,entt::entity,Hash> entities;
    std::unordered_map<ComponentType,std::vector<entt::entity>> modulePools;
    std::uint64_t revision=1,computedRevision=0,extractedRevision=0;
    Renderer::RenderWorld extracted;
    std::uint64_t hierarchyVersion=1;
    bool topologyDirty=true,cameraDirty=true;
    EntityStatistics statistics;
    std::vector<entt::entity> dirtyNodes,sortedNodes,renderDirty,walk;
    void Reserve() {
        auto n=entities.size()+1;if(dirtyNodes.capacity()>=n)return;
        auto capacity=std::max<std::size_t>(64,n*2);
        dirtyNodes.reserve(capacity);sortedNodes.reserve(capacity);renderDirty.reserve(capacity);walk.reserve(capacity);
    }
    void RenderDirty(entt::entity entity){auto& node=registry.get<Node>(entity);if(!node.renderDirty){node.renderDirty=true;renderDirty.push_back(entity);}if(registry.all_of<Camera>(entity))cameraDirty=true;}
    void Dirty(entt::entity entity) {
        walk.clear();walk.push_back(entity);
        while(!walk.empty()){auto current=walk.back();walk.pop_back();auto& node=registry.get<Node>(current);if(node.dirty)continue;node.dirty=true;dirtyNodes.push_back(current);RenderDirty(current);
            for(auto child=node.first;child!=entt::null;child=registry.get<Node>(child).next)walk.push_back(child);
        }
    }
    void Unlink(entt::entity entity){auto& n=registry.get<Node>(entity);if(n.parent!=entt::null){auto& parent=registry.get<Node>(n.parent);--parent.children;if(parent.first==entity)parent.first=n.next;}if(n.previous!=entt::null)registry.get<Node>(n.previous).next=n.next;if(n.next!=entt::null)registry.get<Node>(n.next).previous=n.previous;n.parent=n.next=n.previous=entt::null;}
    void Link(entt::entity entity,entt::entity parent){auto& n=registry.get<Node>(entity);n.parent=parent;if(parent!=entt::null){auto& p=registry.get<Node>(parent);n.next=p.first;if(p.first!=entt::null)registry.get<Node>(p.first).previous=entity;p.first=entity;++p.children;}}

    entt::entity Find(Guid id) const { auto found=entities.find(id); return found==entities.end()?entt::null:found->second; }
    void* Component(entt::entity entity,ComponentType type) {
        if (entity==entt::null) return nullptr;
        if(IsModuleComponent(type)){auto* components=registry.try_get<ModuleComponents>(entity);if(components)for(auto& component:components->values)if(component.type==type)return &component;return nullptr;}
        switch(type) {
        case ComponentType::Name:return registry.try_get<Name>(entity);
        case ComponentType::Transform:return registry.try_get<Transform>(entity);
        case ComponentType::Hierarchy:return registry.try_get<Hierarchy>(entity);
        case ComponentType::MeshRenderer:return registry.try_get<MeshRenderer>(entity);
        case ComponentType::Camera:return registry.try_get<Camera>(entity);
        case ComponentType::Light:return registry.try_get<Light>(entity);
        } return nullptr;
    }
    Result UpdateTransforms() {
        ENGINE_PROFILE_SCOPE("Scene.TransformUpdate");
        auto start=std::chrono::steady_clock::now();
        if(dirtyNodes.empty()){computedRevision=revision;return Result::Success();}
        std::array<std::size_t,1025> counts{};
        for(auto entity:dirtyNodes)if(registry.valid(entity))++counts[registry.get<Node>(entity).depth];
        std::size_t total=0;for(auto& count:counts){auto n=count;count=total;total+=n;}
        sortedNodes.resize(total);
        for(auto entity:dirtyNodes)if(registry.valid(entity))sortedNodes[counts[registry.get<Node>(entity).depth]++]=entity;
        for(auto entity:sortedNodes){auto& node=registry.get<Node>(entity);const auto& transform=registry.get<Transform>(entity);
            auto* parent=node.parent==entt::null?nullptr:&registry.get<Node>(node.parent);
            auto* flags=static_cast<ModuleComponent*>(Component(entity,kEntityFlags));
            auto flag=[&](std::size_t index,bool fallback){return flags?std::get<std::uint32_t>(flags->fields[index])!=0:fallback;};
            node.selfVisible=parent&&flag(2,false)?parent->selfVisible:flag(0,true);
            bool visible=node.selfVisible&&(!parent||parent->visible);
            if(node.visible!=visible){node.visible=visible;topologyDirty=true;cameraDirty=true;}
            bool isStatic=parent&&flag(3,true)?parent->isStatic:flag(1,false);
            // A locally static node remains static even under a dynamic parent.
            isStatic=isStatic||flag(1,false);
            if(node.isStatic!=isStatic){if(isStatic)++statistics.staticEntities;else --statistics.staticEntities;node.isStatic=isStatic;}
            auto* inheritance=static_cast<ModuleComponent*>(Component(entity,kPropertyInheritance));
            auto inherit=[&](std::size_t index){return parent&&inheritance&&std::get<std::uint32_t>(inheritance->fields[index])!=0;};
            auto* mesh=registry.try_get<MeshRenderer>(entity);auto* light=registry.try_get<Light>(entity);
            node.meshColor=inherit(0)?parent->meshColor:mesh?mesh->color:Vec3{1,1,1};
            node.lightColor=inherit(1)?parent->lightColor:light?light->color:Vec3{1,1,1};
            node.lightIntensity=inherit(2)?parent->lightIntensity:light?light->intensity:1;
            auto local=glm::translate(glm::mat4(1),Native(transform.position));
            local=glm::rotate(local,transform.rotation.z,glm::vec3(0,0,1));
            local=glm::rotate(local,transform.rotation.y,glm::vec3(0,1,0));
            local=glm::rotate(local,transform.rotation.x,glm::vec3(1,0,0));
            local=glm::scale(local,Native(transform.scale));
            node.global=Store(node.parent==entt::null?local:Native(registry.get<Node>(node.parent).global)*local);
            if(auto* packed=registry.try_get<PackedModuleLight>(entity)){
                for(const auto& component:registry.get<ModuleComponents>(entity).values)if(auto* schema=Registered(component.type);schema->lightExtractor){packed->value=schema->lightExtractor(component,node.global,node.id);break;}
                node.lightColor=inherit(1)?parent->lightColor:packed->value.color;node.lightIntensity=inherit(2)?parent->lightIntensity:packed->value.intensity;
                auto& light=packed->value;light.color=node.lightColor;light.intensity=node.lightIntensity;light.gpu.color={light.color.x*light.intensity,light.color.y*light.intensity,light.color.z*light.intensity};
            }
node.dirty=false;
        }
        statistics.dirty+=total;statistics.updated+=total;dirtyNodes.clear();computedRevision=revision;
        statistics.updateMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return Result::Success();
    }

};
World::World():m_impl(std::make_unique<Impl>()){}
World::~World()=default;
Result World::ApplyTransform(Guid id,const Transform& transform) {
    auto entity=m_impl->Find(id);if (entity==entt::null||!ValidVector(transform.position,-1000000,1000000)||!ValidVector(transform.rotation,-1000000,1000000)||!ValidVector(transform.scale,0.0001f,1000000)) return Result::Failure("Invalid entity or transform");
    m_impl->registry.get<Transform>(entity)=transform;m_impl->Dirty(entity);m_impl->registry.get<Impl::Node>(entity).version=m_impl->revision+1;++m_impl->revision;return Result::Success();
}
bool World::HasComponent(Guid id,ComponentType type) const noexcept { return m_impl->Component(m_impl->Find(id),type)!=nullptr; }
Result World::Entities(std::vector<Guid>& output) const {
    try {output.clear();output.reserve(m_impl->entities.size());for(const auto& [id,entity]:m_impl->entities)output.push_back(id);
        std::sort(output.begin(),output.end(),[](Guid a,Guid b){return a.high<b.high||(a.high==b.high&&a.low<b.low);});return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}

Result World::EntitiesWithComponent(ComponentType type,std::vector<Guid>& output)const{
    try{output.clear();if(IsModuleComponent(type)){auto found=m_impl->modulePools.find(type);if(found!=m_impl->modulePools.end()){output.reserve(found->second.size());for(auto e:found->second)output.push_back(m_impl->registry.get<Impl::Node>(e).id);}}
        else for(auto e:m_impl->registry.view<Impl::Node>())if(m_impl->Component(e,type))output.push_back(m_impl->registry.get<Impl::Node>(e).id);
        std::sort(output.begin(),output.end(),[](Guid a,Guid b){return a.high<b.high||(a.high==b.high&&a.low<b.low);});return Result::Success();
    }catch(const std::exception& e){return Result::Failure(e.what());}
}
Result World::Definition(Guid id,EntityDefinition& output) const {
    try { auto entity=m_impl->Find(id);if (entity==entt::null) return Result::Failure("Entity does not exist");
        EntityDefinition definition;definition.id=id;definition.name=m_impl->registry.get<Name>(entity);definition.transform=m_impl->registry.get<Transform>(entity);definition.hierarchy=m_impl->registry.get<Hierarchy>(entity);
        if (auto* component=m_impl->registry.try_get<MeshRenderer>(entity)) definition.mesh=*component;
        if (auto* component=m_impl->registry.try_get<Camera>(entity)) definition.camera=*component;
        if (auto* component=m_impl->registry.try_get<Light>(entity)) definition.light=*component;
        if(auto* component=m_impl->registry.try_get<Impl::ModuleComponents>(entity))definition.extensions=component->values;
        output=std::move(definition);return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result World::Replace(std::span<const EntityDefinition> definitions) {
    try {
        World replacement;std::vector<bool> created(definitions.size());std::size_t remaining=definitions.size();
        while (remaining) {
            bool progress=false;
            for (std::size_t i=0;i<definitions.size();++i) if (!created[i]&&(!definitions[i].hierarchy.parent||replacement.Contains(definitions[i].hierarchy.parent))) {
                auto result=replacement.Create(definitions[i]);if (!result) return result;created[i]=true;--remaining;progress=true;
            }
            if (!progress) return Result::Failure("Scene contains missing parents or a hierarchy cycle");
        }
        auto result=replacement.m_impl->UpdateTransforms();if (!result) return result;
        replacement.m_impl->revision=m_impl->revision+1;replacement.m_impl->hierarchyVersion=m_impl->hierarchyVersion+1;for(auto e:replacement.m_impl->registry.view<Impl::Node>())replacement.m_impl->registry.get<Impl::Node>(e).version=replacement.m_impl->revision;m_impl.swap(replacement.m_impl);return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
bool World::Contains(Guid id) const noexcept { return m_impl->Find(id)!=entt::null; }
std::size_t World::EntityCount() const noexcept { return m_impl->entities.size(); }
std::uint64_t World::Revision() const noexcept { return m_impl->revision; }
std::span<const ComponentInfo> World::Reflection() const noexcept { return Registry().reflection; }
Result World::Create(const EntityDefinition& definition) {
    try {
        if (!definition.id || Contains(definition.id) || (definition.hierarchy.parent&&!Contains(definition.hierarchy.parent))) return Result::Failure("Invalid entity identity or parent");
        if (!ValidVector(definition.transform.position,-1000000,1000000)||!ValidVector(definition.transform.rotation,-1000000,1000000)||!ValidVector(definition.transform.scale,0.0001f,1000000)) return Result::Failure("Invalid transform");
        if (definition.camera && (!std::isfinite(definition.camera->verticalFov)||!std::isfinite(definition.camera->nearPlane)||!std::isfinite(definition.camera->farPlane)||definition.camera->verticalFov<0.01f||definition.camera->verticalFov>3.13f||definition.camera->nearPlane<0.001f||definition.camera->farPlane>1000000||definition.camera->farPlane<=definition.camera->nearPlane||!std::isfinite(definition.camera->ev100)||definition.camera->ev100 < -16||definition.camera->ev100 > 32||!std::isfinite(definition.camera->exposureCompensation)||std::abs(definition.camera->exposureCompensation)>16)) return Result::Failure("Invalid camera");
        if (definition.light && (definition.light->type>2 || !ValidVector(definition.light->color,0,1) || !std::isfinite(definition.light->intensity) || definition.light->intensity<0 || definition.light->intensity>100000000 || !std::isfinite(definition.light->range) || definition.light->range<0.001f || definition.light->range>1000000 || !std::isfinite(definition.light->spotAngle) || definition.light->spotAngle<0.01f || definition.light->spotAngle>1.56f)) return Result::Failure("Invalid light");
        if (definition.mesh && (!definition.mesh->mesh || !definition.mesh->material || !ValidVector(definition.mesh->color,0,1))) return Result::Failure("Invalid mesh reference or color");
        std::set<ComponentType> types;for(const auto& component:definition.extensions){if(!types.insert(component.type).second)return Result::Failure("Duplicate module component on entity");auto result=ValidateModuleComponent(component);if(!result)return result;}
        std::size_t lightCount=definition.light?1:0;
        for(const auto& component:definition.extensions)if(Registered(component.type)->lightExtractor)++lightCount;
        if(lightCount>1)return Result::Failure("An entity can have one registered or legacy light component");
        auto parent=m_impl->Find(definition.hierarchy.parent);
        auto depth=parent==entt::null?0:m_impl->registry.get<Impl::Node>(parent).depth+1;
        if(depth>1024)return Result::Failure("Hierarchy depth limit");
        m_impl->Reserve();auto entity=m_impl->registry.create();
        try {
            m_impl->registry.emplace<Name>(entity,definition.name); m_impl->registry.emplace<Transform>(entity,definition.transform);
            m_impl->registry.emplace<Hierarchy>(entity,definition.hierarchy); m_impl->registry.emplace<Impl::Node>(entity,definition.id);
            if (definition.mesh) m_impl->registry.emplace<MeshRenderer>(entity,*definition.mesh);
            if (definition.camera) m_impl->registry.emplace<Camera>(entity,*definition.camera);
            if (definition.light) m_impl->registry.emplace<Light>(entity,*definition.light);
            else if(lightCount)m_impl->registry.emplace<Impl::PackedModuleLight>(entity);
            if(!definition.extensions.empty()){auto& components=m_impl->registry.emplace<Impl::ModuleComponents>(entity,definition.extensions);components.indices.reserve(components.values.size());for(auto& c:components.values){auto& pool=m_impl->modulePools[c.type];components.indices.push_back(pool.size());try{pool.push_back(entity);}catch(...){components.indices.pop_back();throw;}}}
            m_impl->entities.emplace(definition.id,entity);
        }catch(...){if(auto* components=m_impl->registry.try_get<Impl::ModuleComponents>(entity))for(std::size_t i=0;i<components->indices.size();++i)m_impl->modulePools[components->values[i].type].pop_back();m_impl->registry.destroy(entity);throw;}
        auto& node=m_impl->registry.get<Impl::Node>(entity);node.version=m_impl->revision+1;node.depth=depth;m_impl->Link(entity,parent);++m_impl->statistics.depthHistogram[depth];
        m_impl->Dirty(entity);m_impl->topologyDirty=true;++m_impl->hierarchyVersion;
        ++m_impl->revision; return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result World::Remove(Guid id) {
    auto entity=m_impl->Find(id); if (entity==entt::null) return Result::Failure("Entity does not exist");
    auto& node=m_impl->registry.get<Impl::Node>(entity);if(node.children)return Result::Failure("Remove children before their parent");
    --m_impl->statistics.depthHistogram[node.depth];if(node.isStatic)--m_impl->statistics.staticEntities;m_impl->Unlink(entity);m_impl->topologyDirty=true;m_impl->cameraDirty=true;++m_impl->hierarchyVersion;
    if(auto* components=m_impl->registry.try_get<Impl::ModuleComponents>(entity))for(std::size_t i=0;i<components->values.size();++i){auto type=components->values[i].type;auto& pool=m_impl->modulePools[type];auto index=components->indices[i];auto moved=pool.back();pool[index]=moved;pool.pop_back();if(moved!=entity){auto& other=m_impl->registry.get<Impl::ModuleComponents>(moved);for(std::size_t j=0;j<other.values.size();++j)if(other.values[j].type==type){other.indices[j]=index;break;}}}
    m_impl->registry.destroy(entity); m_impl->entities.erase(id); ++m_impl->revision; return Result::Success();
}
Result World::ReadField(Guid id,ComponentType component,std::string_view name,FieldValue& value) const {
    try {
        const auto* field=FindField(component,name); auto* object=m_impl->Component(m_impl->Find(id),component);
        if (!field||!object) return Result::Failure("Unknown entity, component or field");
        if(IsModuleComponent(component)){auto schema=Registered(component);value=static_cast<const ModuleComponent*>(object)->fields[static_cast<std::size_t>(field-schema->fields.data())];return Result::Success();}
        auto* address=static_cast<const std::byte*>(object)+field->offset;
        switch(field->type) {
        case FieldType::String:value=*reinterpret_cast<const std::string*>(address);break;
        case FieldType::Vector3:value=*reinterpret_cast<const Vec3*>(address);break;
        case FieldType::Guid:value=*reinterpret_cast<const Guid*>(address);break;
        case FieldType::Float:value=*reinterpret_cast<const float*>(address);break;
        case FieldType::Unsigned:value=*reinterpret_cast<const std::uint32_t*>(address);break;
        } return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result World::ApplyField(Guid id,ComponentType component,std::string_view name,const FieldValue& value) {
    try {
        const auto* field=FindField(component,name); auto entity=m_impl->Find(id); auto* object=m_impl->Component(entity,component);
        if (!field||!object||value.index()!=static_cast<std::size_t>(field->type)) return Result::Failure("Field type or component mismatch");
        if(IsModuleComponent(component)){auto next=*static_cast<ModuleComponent*>(object);auto schema=Registered(component);auto index=static_cast<std::size_t>(field-schema->fields.data());next.fields[index]=value;auto result=ValidateModuleComponent(next);if(!result)return result;if(next.fields==static_cast<ModuleComponent*>(object)->fields)return Result::Success();*static_cast<ModuleComponent*>(object)=std::move(next);if(component==kEntityFlags||component==kPropertyInheritance||schema->lightExtractor)m_impl->Dirty(entity);m_impl->registry.get<Impl::Node>(entity).version=m_impl->revision+1;++m_impl->revision;return Result::Success();}
        if (component==ComponentType::Hierarchy) return Reparent(id,std::get<Guid>(value));
        if (field->type==FieldType::Float) { auto v=std::get<float>(value); if (!std::isfinite(v)||v<field->minimum||v>field->maximum) return Result::Failure("Field outside reflected range"); }
        if (field->type==FieldType::Vector3&&!ValidVector(std::get<Vec3>(value),field->minimum,field->maximum)) return Result::Failure("Vector outside reflected range");
        if (field->type==FieldType::Unsigned&&std::get<std::uint32_t>(value)>field->maximum) return Result::Failure("Enum outside reflected range");
        if (component==ComponentType::Camera) {
            auto camera=*static_cast<Camera*>(object);
            *reinterpret_cast<float*>(reinterpret_cast<std::byte*>(&camera)+field->offset)=std::get<float>(value);
            if (camera.farPlane<=camera.nearPlane) return Result::Failure("Camera far plane must exceed near plane");
        }
        auto* address=static_cast<std::byte*>(object)+field->offset;
        std::visit([&](const auto& v) { using T=std::decay_t<decltype(v)>; *reinterpret_cast<T*>(address)=v; },value);
        if (component==ComponentType::Transform) m_impl->Dirty(entity);
        else if(component==ComponentType::MeshRenderer||component==ComponentType::Light)m_impl->Dirty(entity);
        else if(component==ComponentType::Camera)m_impl->RenderDirty(entity);
        if(component==ComponentType::Name)++m_impl->hierarchyVersion;
        m_impl->registry.get<Impl::Node>(entity).version=m_impl->revision+1;
        ++m_impl->revision; return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result World::Reparent(Guid id,Guid parent) {
    auto entity=m_impl->Find(id); if (entity==entt::null||(parent&&!Contains(parent))) return Result::Failure("Invalid parent or child");
    auto parentEntity=m_impl->Find(parent);auto& node=m_impl->registry.get<Impl::Node>(entity);
    if(node.parent==parentEntity)return Result::Success();
    for(auto ancestor=parentEntity;ancestor!=entt::null;ancestor=m_impl->registry.get<Impl::Node>(ancestor).parent)if(ancestor==entity)return Result::Failure("Hierarchy cycle");
    auto newDepth=parentEntity==entt::null?0:m_impl->registry.get<Impl::Node>(parentEntity).depth+1;
    auto delta=static_cast<int>(newDepth)-static_cast<int>(node.depth);
    m_impl->walk.clear();m_impl->walk.push_back(entity);
    for(std::size_t i=0;i<m_impl->walk.size();++i){auto& n=m_impl->registry.get<Impl::Node>(m_impl->walk[i]);if(static_cast<int>(n.depth)+delta>1024)return Result::Failure("Hierarchy depth limit");for(auto child=n.first;child!=entt::null;child=m_impl->registry.get<Impl::Node>(child).next)m_impl->walk.push_back(child);}
    for(auto current:m_impl->walk){auto& n=m_impl->registry.get<Impl::Node>(current);--m_impl->statistics.depthHistogram[n.depth];n.depth=static_cast<std::uint32_t>(static_cast<int>(n.depth)+delta);++m_impl->statistics.depthHistogram[n.depth];}
    m_impl->Unlink(entity);m_impl->Link(entity,parentEntity);
    m_impl->registry.get<Hierarchy>(entity).parent=parent;m_impl->Dirty(entity);node.version=m_impl->revision+1;++m_impl->hierarchyVersion;++m_impl->revision;return Result::Success();
}
Result World::GlobalTransform(Guid id,Mat4& value) {
    try { auto entity=m_impl->Find(id); if (entity==entt::null) return Result::Failure("Entity does not exist");
        auto result=m_impl->UpdateTransforms(); if (!result) return result; value=m_impl->registry.get<Impl::Node>(entity).global; return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result World::Extract(Renderer::RenderWorld& output) {
    ENGINE_PROFILE_SCOPE("Scene.Extract");
    try {
        auto start=std::chrono::steady_clock::now();if(!m_impl->dirtyNodes.empty()){auto result=m_impl->UpdateTransforms();if(!result)return result;}
        auto& snapshot=m_impl->extracted;
        if(m_impl->extractedRevision!=m_impl->revision){
            auto retainedHdri=std::move(snapshot.settings.hdri);snapshot.settings={};snapshot.settings.hdri=std::move(retainedHdri);snapshot.settings.hdri.clear();
            for(const auto& [type,pool]:m_impl->modulePools)if(auto* schema=Registered(type);schema&&schema->settingsExtractor){entt::entity chosen=entt::null;Guid selected{UINT64_MAX,UINT64_MAX};for(auto entity:pool){auto& node=m_impl->registry.get<Impl::Node>(entity);if(node.visible&&(node.id.high<selected.high||(node.id.high==selected.high&&node.id.low<selected.low))){chosen=entity;selected=node.id;}}if(chosen!=entt::null)for(auto& component:m_impl->registry.get<Impl::ModuleComponents>(chosen).values)if(component.type==type)schema->settingsExtractor(component,snapshot);}
            snapshot.baseRevision=m_impl->extractedRevision;snapshot.source=m_impl->source;snapshot.revision=m_impl->revision;snapshot.fullUpdate=m_impl->topologyDirty;snapshot.changedInstances.clear();snapshot.changedLights.clear();
            auto meshValue=[&](entt::entity entity){auto& node=m_impl->registry.get<Impl::Node>(entity);const auto& mesh=m_impl->registry.get<MeshRenderer>(entity);return Renderer::RenderInstance{node.id,mesh.mesh,mesh.material,node.global,Store(glm::transpose(glm::inverse(Native(node.global)))),node.meshColor};};
            auto lightValue=[&](entt::entity entity){
                auto& node=m_impl->registry.get<Impl::Node>(entity);
                if(auto* packed=m_impl->registry.try_get<Impl::PackedModuleLight>(entity))return packed->value;
                const auto& light=m_impl->registry.get<Light>(entity);auto global=Native(node.global);auto direction=glm::normalize(glm::vec3(global*glm::vec4(0,0,-1,0)));
                Renderer::RenderLight result{light.type,{global[3].x,global[3].y,global[3].z},{direction.x,direction.y,direction.z},node.lightColor,node.lightIntensity,light.range,light.spotAngle};result.entity=node.id;return result;
            };
            if(m_impl->topologyDirty){snapshot.instances.clear();snapshot.lights.clear();snapshot.instances.reserve(EntityCount());snapshot.changedInstances.reserve(EntityCount());snapshot.changedLights.reserve(EntityCount());
                for(auto entity:m_impl->registry.view<Impl::Node,MeshRenderer>()){auto& node=m_impl->registry.get<Impl::Node>(entity);node.instance=UINT32_MAX;if(node.visible){node.instance=static_cast<std::uint32_t>(snapshot.instances.size());snapshot.instances.push_back(meshValue(entity));}}
                for(auto entity:m_impl->registry.view<Impl::Node,Light>()){auto& node=m_impl->registry.get<Impl::Node>(entity);node.light=UINT32_MAX;if(node.visible){node.light=static_cast<std::uint32_t>(snapshot.lights.size());snapshot.lights.push_back(lightValue(entity));}}
                for(const auto& [type,pool]:m_impl->modulePools)if(auto* schema=Registered(type);schema&&schema->lightExtractor)for(auto entity:pool){
                    auto& node=m_impl->registry.get<Impl::Node>(entity);node.light=UINT32_MAX;
                    if(node.visible){node.light=static_cast<std::uint32_t>(snapshot.lights.size());snapshot.lights.push_back(lightValue(entity));}
                }
                snapshot.instanceRevision=m_impl->revision;
            }else{
                for(auto entity:m_impl->renderDirty)if(m_impl->registry.valid(entity)){auto& node=m_impl->registry.get<Impl::Node>(entity);
                    if(node.instance!=UINT32_MAX){snapshot.instances[node.instance]=meshValue(entity);snapshot.changedInstances.push_back(node.instance);snapshot.instanceRevision=m_impl->revision;}
                    if(node.light!=UINT32_MAX){snapshot.lights[node.light]=lightValue(entity);snapshot.changedLights.push_back(node.light);}
                }
            }
            if(m_impl->cameraDirty){snapshot.camera={};Guid selected{UINT64_MAX,UINT64_MAX};for(auto entity:m_impl->registry.view<Impl::Node,Camera>()){auto& node=m_impl->registry.get<Impl::Node>(entity);if(!node.visible||node.id.high>selected.high||(node.id.high==selected.high&&node.id.low>=selected.low))continue;selected=node.id;auto& camera=m_impl->registry.get<Camera>(entity);snapshot.camera={node.global,camera.verticalFov,camera.nearPlane,camera.farPlane,true,camera.ev100,camera.exposureCompensation};}}
            for(auto entity:m_impl->renderDirty)if(m_impl->registry.valid(entity))m_impl->registry.get<Impl::Node>(entity).renderDirty=false;
            m_impl->renderDirty.clear();m_impl->topologyDirty=false;m_impl->cameraDirty=false;m_impl->extractedRevision=m_impl->revision;
        }
        if(output.modified||output.source!=snapshot.source||output.revision!=snapshot.revision){
            if(!output.modified&&output.source==snapshot.source&&output.revision==snapshot.baseRevision&&!snapshot.fullUpdate&&output.instances.size()==snapshot.instances.size()&&output.lights.size()==snapshot.lights.size()){
                for(auto index:snapshot.changedInstances)output.instances[index]=snapshot.instances[index];for(auto index:snapshot.changedLights)output.lights[index]=snapshot.lights[index];output.camera=snapshot.camera;output.settings=snapshot.settings;output.revision=snapshot.revision;output.baseRevision=snapshot.baseRevision;output.instanceRevision=snapshot.instanceRevision;output.fullUpdate=false;output.changedInstances=snapshot.changedInstances;output.changedLights=snapshot.changedLights;
            }else{output=snapshot;output.changedInstances.reserve(output.instances.size());output.changedLights.reserve(output.lights.size());}
        }
        m_impl->statistics.instances=snapshot.instances.size();m_impl->statistics.dynamicEntities=EntityCount()-m_impl->statistics.staticEntities;m_impl->statistics.extractMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return Result::Success();
    }catch(const std::exception& error){return Result::Failure(error.what());}
}
void World::BeginFrame()noexcept{m_impl->statistics.dirty=m_impl->statistics.updated=0;m_impl->statistics.updateMs=m_impl->statistics.extractMs=0;}
Result World::VisibleEntitiesWithoutMesh(std::vector<Guid>& output){auto r=m_impl->UpdateTransforms();if(!r)return r;output.clear();for(auto entity:m_impl->registry.view<Impl::Node>(entt::exclude<MeshRenderer>)){auto& node=m_impl->registry.get<Impl::Node>(entity);if(node.visible)output.push_back(node.id);}return Result::Success();}
Result World::Update(){BeginFrame();return m_impl->UpdateTransforms();}
std::uint64_t World::HierarchyVersion()const noexcept{return m_impl->hierarchyVersion;}
std::uint64_t World::ComponentVersion(Guid id)const noexcept{auto e=m_impl->Find(id);return e==entt::null?0:m_impl->registry.get<Impl::Node>(e).version;}
const EntityStatistics& World::Statistics()const noexcept{return m_impl->statistics;}
Result World::HierarchyEntries(std::vector<HierarchyEntry>& output)const{
    try{output.clear();output.reserve(EntityCount());for(auto e:m_impl->registry.view<Impl::Node>()){auto& n=m_impl->registry.get<Impl::Node>(e);output.push_back({n.id,m_impl->registry.get<Hierarchy>(e).parent,m_impl->registry.get<Name>(e).value,n.depth,n.children});}return Result::Success();}catch(const std::exception& e){return Result::Failure(e.what());}
}
Result World::Descendants(std::span<const Guid> roots,std::vector<Guid>& output)const{
    try{std::vector<entt::entity> queue;std::unordered_map<Guid,bool,Hash> seen;queue.reserve(roots.size());for(auto id:roots){auto e=m_impl->Find(id);if(e!=entt::null)queue.push_back(e);}output.clear();for(std::size_t i=0;i<queue.size();++i){auto& n=m_impl->registry.get<Impl::Node>(queue[i]);if(!seen.emplace(n.id,true).second)continue;output.push_back(n.id);for(auto child=n.first;child!=entt::null;child=m_impl->registry.get<Impl::Node>(child).next)queue.push_back(child);}return Result::Success();}catch(const std::exception& e){return Result::Failure(e.what());}
}
Result World::ApplyBatch(std::span<const EntityDefinition> added,std::span<const Guid> removed){
    try{
        std::unordered_map<Guid,bool,Hash> removals;
        std::vector<entt::entity> removalOrder;removalOrder.reserve(removed.size());
        for(auto id:removed){auto e=m_impl->Find(id);if(e==entt::null||!removals.emplace(id,true).second)return Result::Failure("Invalid batch removal");removalOrder.push_back(e);}
        for(auto e:removalOrder)for(auto child=m_impl->registry.get<Impl::Node>(e).first;child!=entt::null;child=m_impl->registry.get<Impl::Node>(child).next)if(!removals.contains(m_impl->registry.get<Impl::Node>(child).id))return Result::Failure("Batch removal must include descendants");
        std::unordered_map<Guid,std::size_t,Hash> indices;indices.reserve(added.size());
        for(std::size_t i=0;i<added.size();++i)if(Contains(added[i].id)||!indices.emplace(added[i].id,i).second)return Result::Failure("Duplicate batch identity");
        std::vector<std::vector<std::size_t>> children(added.size());std::vector<std::size_t> order;order.reserve(added.size());
        World validation;
        for(std::size_t i=0;i<added.size();++i){auto parent=added[i].hierarchy.parent;auto found=indices.find(parent);if(found!=indices.end())children[found->second].push_back(i);else{
            if(parent){if(!Contains(parent)||removals.contains(parent))return Result::Failure("Missing batch parent");if(!validation.Contains(parent)){EntityDefinition stub;stub.id=parent;auto result=validation.Create(stub);if(!result)return result;validation.m_impl->registry.get<Impl::Node>(validation.m_impl->Find(parent)).depth=m_impl->registry.get<Impl::Node>(m_impl->Find(parent)).depth;}}
            order.push_back(i);
        }}
        for(std::size_t i=0;i<order.size();++i){auto index=order[i];auto result=validation.Create(added[index]);if(!result)return result;order.insert(order.end(),children[index].begin(),children[index].end());}
        if(order.size()!=added.size())return Result::Failure("Batch hierarchy cycle");
        std::sort(removalOrder.begin(),removalOrder.end(),[&](auto a,auto b){return m_impl->registry.get<Impl::Node>(a).depth>m_impl->registry.get<Impl::Node>(b).depth;});
        std::vector<Guid> created;created.reserve(order.size());
        for(auto index:order){auto result=Create(added[index]);if(!result){for(auto it=created.rbegin();it!=created.rend();++it)Remove(*it);return result;}created.push_back(added[index].id);}
        for(auto e:removalOrder){auto result=Remove(m_impl->registry.get<Impl::Node>(e).id);if(!result)return result;}
        return Result::Success();
    }catch(const std::exception& e){return Result::Failure(e.what());}
}

}
