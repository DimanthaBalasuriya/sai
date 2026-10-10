#include "engine/scene/Commands.hpp"
#include <exception>
#include <algorithm>
#include <map>
#include <random>
namespace Engine::Scene {
Result SetTransformCommand::Do(World& world) {
    if (!m_captured) { EntityDefinition definition;auto result=world.Definition(m_id,definition);if (!result) return result;m_before=definition.transform;m_captured=true; }
    return world.ApplyTransform(m_id,m_after);
}
bool SetTransformCommand::Merge(const ICommand& other) {
    auto* transform=dynamic_cast<const SetTransformCommand*>(&other);if (!transform||transform->m_id!=m_id) return false;m_after=transform->m_after;return true;
}
Result ReplaceSceneCommand::Do(World& world) {
    if (!m_captured) {
        std::vector<Guid> ids;auto result=world.Entities(ids);if (!result) return result;
        for (auto id:ids) { EntityDefinition definition;result=world.Definition(id,definition);if (!result) return result;m_before.push_back(std::move(definition)); }
        m_captured=true;
    }
    return world.Replace(m_after);
}
Result SetFieldCommand::Do(World& world) {
    if (!m_captured) { auto result=world.ReadField(m_id,m_component,m_field,m_before); if (!result) return result; m_captured=true; }
    return world.ApplyField(m_id,m_component,m_field,m_after);
}
Result SetFieldCommand::Undo(World& world) { return world.ApplyField(m_id,m_component,m_field,m_before); }
bool SetFieldCommand::Merge(const ICommand& other) {
    auto* edit=dynamic_cast<const SetFieldCommand*>(&other);
    if (!edit || edit->m_id!=m_id || edit->m_component!=m_component || edit->m_field!=m_field) return false;
    m_after=edit->m_after; return true;
}
Result ReparentCommand::Do(World& world) {
    if (!m_captured) { FieldValue value; auto result=world.ReadField(m_id,ComponentType::Hierarchy,"parent",value); if (!result) return result; m_before=std::get<Guid>(value); m_captured=true; }
    return world.Reparent(m_id,m_after);
}
Result CommandHistory::Execute(World& world,std::unique_ptr<ICommand> command,bool merge) {
    try {
        if (!command) return Result::Failure("Null command");
        if (m_commands.capacity()<m_cursor+1) m_commands.reserve(std::max(m_cursor+1,m_commands.capacity()*2));
        auto result=command->Do(world); if (!result) return result;
        m_commands.resize(m_cursor);
        if (merge && m_cursor && m_commands.back()->Merge(*command)) return Result::Success();
        m_commands.push_back(std::move(command)); ++m_cursor; return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result CommandHistory::Undo(World& world) {
    if (!m_cursor) return Result::Failure("Nothing to undo");
    try { auto result=m_commands[m_cursor-1]->Undo(world); if (result) --m_cursor; return result; }
    catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result CommandHistory::Redo(World& world) {
    if (m_cursor==m_commands.size()) return Result::Failure("Nothing to redo");
    try { auto result=m_commands[m_cursor]->Do(world); if (result) ++m_cursor; return result; }
    catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result EntityBatchCommand::Do(World& world){
    if(!m_captured){m_removed.clear();m_addedIds.clear();m_removed.reserve(m_remove.size());m_addedIds.reserve(m_added.size());for(auto id:m_remove){EntityDefinition d;auto result=world.Definition(id,d);if(!result)return result;m_removed.push_back(std::move(d));}for(auto& d:m_added)m_addedIds.push_back(d.id);m_captured=true;}
    return world.ApplyBatch(m_added,m_remove);
}
Result EntityBatchCommand::Undo(World& world){return world.ApplyBatch(m_removed,m_addedIds);}
Result InstantiatePrefab(World& world,CommandHistory& history,std::span<const EntityDefinition> prefab,std::vector<Guid>& roots){
    struct Less{bool operator()(Guid a,Guid b)const{return a.high<b.high||(a.high==b.high&&a.low<b.low);}};
    std::map<Guid,Guid,Less> remap;std::map<Guid,bool,Less> generated;std::mt19937_64 random(std::random_device{}());
    for(auto& d:prefab){if(!d.id||remap.contains(d.id))return Result::Failure("Invalid prefab identity");Guid id;do{id={random(),random()};}while(!id||world.Contains(id)||generated.contains(id));generated[id]=true;remap[d.id]=id;}
    std::vector<EntityDefinition> added;added.reserve(prefab.size());std::vector<Guid> nextRoots;
    for(auto d:prefab){d.id=remap.at(d.id);if(d.hierarchy.parent){auto parent=remap.find(d.hierarchy.parent);if(parent==remap.end())return Result::Failure("Prefab parent is missing");d.hierarchy.parent=parent->second;}else nextRoots.push_back(d.id);
        for(auto& extension:d.extensions)for(auto& field:extension.fields)if(auto id=std::get_if<Guid>(&field)){auto found=remap.find(*id);if(found!=remap.end())*id=found->second;}
        added.push_back(std::move(d));
    }
    auto result=history.Execute(world,std::make_unique<EntityBatchCommand>(std::move(added)));if(result)roots=std::move(nextRoots);return result;
}

}
