#include "engine/assets/Registry.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
namespace Engine::Assets {
Result Instantiate(const ImportedScene& imported,Scene::World& world,Scene::CommandHistory& history) {
    try {
        std::vector<std::unique_ptr<Scene::ICommand>> commands;
        for (const auto& node:imported.nodes) {
            Scene::EntityDefinition entity; entity.id=node.id; entity.name.value=node.name; entity.hierarchy.parent=node.parent;
            auto matrix=glm::make_mat4(node.local.values.data()); glm::vec3 scale,translation,skew; glm::vec4 perspective; glm::quat orientation;
            if (!glm::decompose(matrix,scale,orientation,translation,skew,perspective)||glm::length(skew)>0.0001f) return Result::Failure("Sheared or singular glTF node is unsupported");
            float z=0,y=0,x=0; glm::extractEulerAngleZYX(glm::mat4_cast(orientation),z,y,x);
            entity.transform.position={translation.x,translation.y,translation.z}; entity.transform.rotation={x,y,z}; entity.transform.scale={scale.x,scale.y,scale.z};
            commands.push_back(std::make_unique<Scene::CreateEntityCommand>(entity));
            for (std::size_t p=0;p<node.primitives.size();++p) {
                Scene::EntityDefinition primitive; primitive.id={node.id.high^0x5052494d49544956ull,node.id.low^(static_cast<std::uint64_t>(p+1)<<32)};
                primitive.name.value=node.name+" / primitive "+std::to_string(p); primitive.hierarchy.parent=node.id;
                primitive.mesh=Scene::MeshRenderer{node.primitives[p].mesh,node.primitives[p].material,{1,1,1}};
                commands.push_back(std::make_unique<Scene::CreateEntityCommand>(primitive));
            }
        }
        for (auto& command:commands) { auto result=history.Execute(world,std::move(command)); if (!result) return result; }
        return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
}
