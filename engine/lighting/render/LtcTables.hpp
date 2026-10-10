#pragma once
#include <glm/glm.hpp>
#include <vector>
namespace Engine::Lighting {
struct LtcEntry {glm::vec4 matrix;glm::vec4 amplitude;};
std::vector<LtcEntry> LtcTables();
}
