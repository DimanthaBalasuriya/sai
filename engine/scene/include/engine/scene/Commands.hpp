#pragma once
#include "engine/scene/World.hpp"
#include <vector>
namespace Engine::Scene {
// Commands own edit values; history owns commands. Main-thread only.
class ICommand {
public:
    virtual ~ICommand()=default;
    virtual Result Do(World& world)=0;
    virtual Result Undo(World& world)=0;
    virtual bool Merge(const ICommand&) { return false; }
};
class CreateEntityCommand final : public ICommand {
public:
    explicit CreateEntityCommand(EntityDefinition definition):m_definition(std::move(definition)){}
    Result Do(World& world) override { return world.Create(m_definition); }
    Result Undo(World& world) override { return world.Remove(m_definition.id); }
private: EntityDefinition m_definition;
};
class SetFieldCommand final : public ICommand {
public:
    SetFieldCommand(Guid id,ComponentType component,std::string field,FieldValue value):m_id(id),m_component(component),m_field(std::move(field)),m_after(std::move(value)){}
    Result Do(World& world) override;
    Result Undo(World& world) override;
    bool Merge(const ICommand& other) override;
private:
    Guid m_id; ComponentType m_component; std::string m_field; FieldValue m_before,m_after; bool m_captured=false;
};
class ReparentCommand final : public ICommand {
public:
    ReparentCommand(Guid id,Guid parent):m_id(id),m_after(parent){}
    Result Do(World& world) override;
    Result Undo(World& world) override { return world.Reparent(m_id,m_before); }
private: Guid m_id,m_before,m_after; bool m_captured=false;
};
class CommandHistory {
public:
    Result Execute(World& world,std::unique_ptr<ICommand> command,bool merge=false);
    Result Undo(World& world);
    Result Redo(World& world);
    void Clear() noexcept { m_commands.clear(); m_cursor=0; }
    bool CanUndo() const noexcept { return m_cursor>0; }
    bool CanRedo() const noexcept { return m_cursor<m_commands.size(); }
private: std::vector<std::unique_ptr<ICommand>> m_commands; std::size_t m_cursor=0;
};
class ReplaceSceneCommand final : public ICommand {
public:
    explicit ReplaceSceneCommand(std::vector<EntityDefinition> definitions):m_after(std::move(definitions)){}
    Result Do(World& world) override;
    Result Undo(World& world) override { return world.Replace(m_before); }
private: std::vector<EntityDefinition> m_before,m_after; bool m_captured=false;
};
class SetTransformCommand final : public ICommand {
public:
    SetTransformCommand(Guid id,Transform transform):m_id(id),m_after(transform){}
    Result Do(World& world) override;
    Result Undo(World& world) override { return world.ApplyTransform(m_id,m_before); }
    bool Merge(const ICommand& other) override;
private: Guid m_id;Transform m_before,m_after;bool m_captured=false;
};
class EntityBatchCommand final : public ICommand {
public:
    EntityBatchCommand(std::vector<EntityDefinition> added,std::vector<Guid> removed={}):m_added(std::move(added)),m_remove(std::move(removed)){}
    Result Do(World&) override;
    Result Undo(World&) override;
private:
    std::vector<EntityDefinition> m_added,m_removed;
    std::vector<Guid> m_remove,m_addedIds;
    bool m_captured=false;
};
Result InstantiatePrefab(World&,CommandHistory&,std::span<const EntityDefinition>,std::vector<Guid>& roots);
}
