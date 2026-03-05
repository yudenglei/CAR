/**
 * @file car_reuse_vector.h
 * @brief ReuseVector with generation tracking and in-place replacement
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

using ObjectId = uint64_t;

template <typename T>
class ReuseVector {
public:
    struct Slot {
        T data{};
        uint32_t gen = 0;
        bool valid = false;
    };

    ObjectId add(const T& item) {
        uint32_t idx = alloc_slot();
        Slot& s = m_slots[idx];
        s.data = item;
        s.valid = true;
        ++s.gen;
        return make_handle(idx, s.gen);
    }

    ObjectId add(T&& item) {
        uint32_t idx = alloc_slot();
        Slot& s = m_slots[idx];
        s.data = std::move(item);
        s.valid = true;
        ++s.gen;
        return make_handle(idx, s.gen);
    }

    bool replace(ObjectId handle, const T& item) {
        auto [idx, gen] = unpack_handle(handle);
        if (!is_valid_slot(idx, gen)) {
            return false;
        }
        m_slots[idx].data = item;
        return true;
    }

    bool remove(ObjectId handle) {
        auto [idx, gen] = unpack_handle(handle);
        if (!is_valid_slot(idx, gen)) {
            return false;
        }

        Slot& s = m_slots[idx];
        s.valid = false;
        ++s.gen;
        m_free.push_back(idx);
        return true;
    }

    T* get(ObjectId handle) {
        auto [idx, gen] = unpack_handle(handle);
        if (!is_valid_slot(idx, gen)) {
            return nullptr;
        }
        return &m_slots[idx].data;
    }

    const T* get(ObjectId handle) const {
        return const_cast<ReuseVector*>(this)->get(handle);
    }

    bool valid(ObjectId handle) const {
        return get(handle) != nullptr;
    }

    std::pair<uint32_t, uint32_t> get_slot_info(ObjectId handle) const {
        auto [idx, gen] = unpack_handle(handle);
        if (!is_valid_slot(idx, gen)) {
            return {UINT32_MAX, 0};
        }
        return {idx, gen};
    }

    bool restore(ObjectId handle, const T& item) {
        auto [idx, gen] = unpack_handle(handle);
        if (idx >= m_slots.size()) {
            m_slots.resize(idx + 1);
        }

        Slot& s = m_slots[idx];
        s.data = item;
        s.valid = true;
        s.gen = gen;

        auto it = std::find(m_free.begin(), m_free.end(), idx);
        if (it != m_free.end()) {
            m_free.erase(it);
        }
        return true;
    }

    const std::vector<Slot>& slots() const { return m_slots; }

    size_t size() const {
        size_t count = 0;
        for (const auto& s : m_slots) {
            count += s.valid ? 1 : 0;
        }
        return count;
    }

    void clear() {
        m_slots.clear();
        m_free.clear();
    }

private:
    uint32_t alloc_slot() {
        if (!m_free.empty()) {
            uint32_t idx = m_free.back();
            m_free.pop_back();
            return idx;
        }
        m_slots.emplace_back();
        return static_cast<uint32_t>(m_slots.size() - 1);
    }

    bool is_valid_slot(uint32_t idx, uint32_t gen) const {
        return idx < m_slots.size() && m_slots[idx].valid && m_slots[idx].gen == gen;
    }

    static ObjectId make_handle(uint32_t idx, uint32_t gen) {
        return (static_cast<ObjectId>(gen) << 32) | idx;
    }

    static std::pair<uint32_t, uint32_t> unpack_handle(ObjectId handle) {
        return {
            static_cast<uint32_t>(handle & 0xFFFFFFFFu),
            static_cast<uint32_t>(handle >> 32),
        };
    }

    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_free;
};
