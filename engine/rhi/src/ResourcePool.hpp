#pragma once
#include <memory>
#include <vector>
#include <cstdint>
namespace Engine::Rhi {
// Owns records; pointers survive slot-vector growth. Main-thread only.
template<class Record, class HandleType> class ResourcePool {
    struct Slot { std::uint32_t generation = 1; std::unique_ptr<Record> record; };
    std::vector<Slot> m_slots;
public:
    HandleType Insert(std::unique_ptr<Record> record) {
        for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
            if (!m_slots[i].record) { m_slots[i].record = std::move(record); return {i, m_slots[i].generation}; }
        }
        m_slots.push_back({1, std::move(record)});
        return {static_cast<std::uint32_t>(m_slots.size() - 1), 1};
    }
    Record* Get(HandleType handle) const noexcept {
        if (handle.slot >= m_slots.size()) return nullptr;
        const auto& slot = m_slots[handle.slot];
        return slot.generation == handle.generation ? slot.record.get() : nullptr;
    }
    bool Remove(HandleType handle) noexcept {
        if (!Get(handle)) return false;
        auto& slot = m_slots[handle.slot];
        slot.record.reset();
        if (++slot.generation == 0) ++slot.generation;
        return true;
    }
    template<class Function> void ForEach(Function&& function) {
        for (auto& slot : m_slots) if (slot.record) function(*slot.record);
    }
    void Clear() noexcept {
        for (std::uint32_t i = 0; i < m_slots.size(); ++i) if (m_slots[i].record) Remove({i, m_slots[i].generation});
    }
};
}
