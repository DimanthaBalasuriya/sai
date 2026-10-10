#pragma once
#include <algorithm>
#include <cstdint>
namespace Engine::Lighting {
// Bound only index-list memory. Overflow keeps correctness through loop fallback.
inline std::uint32_t ClusterListCapacity(std::uint32_t clusters,std::uint32_t budgetMiB){if(!clusters)return 1;return std::uint32_t(std::clamp(std::uint64_t(budgetMiB)*1024*1024/(std::uint64_t(clusters)*4),std::uint64_t(1),std::uint64_t(256)));}
}
