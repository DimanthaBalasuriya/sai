#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <array>
namespace Engine::Lighting {
struct GpuLighting {
    glm::mat4 view{1};glm::vec4 camera{};glm::uvec4 grid{20,12,24,256};
    glm::vec4 depth{},screen{};
    std::uint64_t lights=0,bounds=0,cells=0,indices=0,counters=0;
    std::uint32_t localCount=0,directionalCount=0;std::uint64_t shadows=0;
    std::uint64_t areas=0,ltc=0;
    std::uint64_t bodyPadding=0;std::array<glm::vec4,4> bodyTransmission{};
    glm::vec4 atmosphere{};
};
static_assert(sizeof(GpuLighting)==288);
}
