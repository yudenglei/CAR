/**
 * @file car_reuse_vector.h
 * @brief ReuseVector with generation tracking
 */

#pragma once

#include <vector>
#include <algorithm>
#include <cstdint>

using ObjectId = uint64_t;

template<typename T>
class ReuseVector {
public:
    struct Slot {
        T data;
        uint32_t gen = 0;
        bool valid = false;
    };

    ObjectId add(const T& item) {
        uint32_t idx;
        if (!m_free.empty()) { idx = m_free.back(); m_free.pop_back(); }
        else { idx = static_cast<uint32_t>(m_slots.size()); m_slots.emplace_back(); }
        m_slots[idx].data = item;
        m_slots[idx].gen += 1;
        m_slots[idx].valid = true;
        return make_handle(idx, m_slots[idx].gen);
    }

    void remove(ObjectId handle) {
        auto [idx, gen] = unpack_handle(handle);
        if (idx < m_slots.size() && m_slots[idx].valid && m_slots[idx].gen == gen) {
            m_slots[idx].valid = false;
            m_slots[idx].gen += 1;
            m_free.push_back(idx);
        }
    }

    T* get(ObjectId handle) {
        auto [idx, gen] = unpack_handle(handle);
        if (idx >= m_slots.size()) return nullptr;
        Slot& s = m_slots[idx];
        if (!s.valid || s.gen != gen) return nullptr;
        return &s.data;
    }

    const T* get(ObjectId handle) const { return const_cast<ReuseVector*>(this)->get(handle); }
    bool valid(ObjectId handle) const { return get(handle) != nullptr; }

    std::pair<uint32_t, uint32_t> get_slot_info(ObjectId handle) const {
        auto [idx, gen] = unpack_handle(handle);
        if (idx < m_slots.size() && m_slots[idx].valid && m_slots[idx].gen == gen)
            return {idx, m_slots[idx].gen};
        return {UINT32_MAX, 0};
    }

    void restore(uint32_t idx, const T& item, uint32_t gen) {
        if (idx >= m_slots.size()) m_slots.resize(idx + 1);
        m_slots[idx].data = item;
        m_slots[idx].gen = gen;
        m_slots[idx].valid = true;
        auto it = std::find(m_free.begin(), m_free.end(), idx);
        if (it != m_free.end()) m_free.erase(it);
    }

    const std::vector<Slot>& slots() const { return m_slots; }
    size_t size() const { size_t c = 0; for (auto& s : m_slots) if (s.valid) c++; return c; }
    void clear() { m_slots.clear(); m_free.clear(); }

private:
    static ObjectId make_handle(uint32_t idx, uint32_t gen) { return (static_cast<ObjectId>(gen) << 32) | idx; }
    static std::pair<uint32_t, uint32_t> unpack_handle(ObjectId h) { return {static_cast<uint32_t>(h & 0xFFFFFFFFu), static_cast<uint32_t>(h >> 32)}; }
    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_free;
};
