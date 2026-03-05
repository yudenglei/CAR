/**
 * @file car_shape.h
 * @brief Shape system with ID-based templating
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include "car_basic_types.h"

using ShapeId = uint64_t;

template<typename T>
class ShapeStore {
public:
    struct Slot { T data; uint32_t gen = 0; bool valid = false; };

    ShapeId add(const T& item) {
        uint32_t idx;
        if (!m_free.empty()) { idx = m_free.back(); m_free.pop_back(); }
        else { idx = static_cast<uint32_t>(m_slots.size()); m_slots.emplace_back(); }
        m_slots[idx].data = item;
        m_slots[idx].gen += 1;
        m_slots[idx].valid = true;
        return make_id(idx, m_slots[idx].gen);
    }

    void remove(ShapeId id) {
        auto [idx, gen] = unpack_id(id);
        if (idx < m_slots.size() && m_slots[idx].valid && m_slots[idx].gen == gen) {
            m_slots[idx].valid = false;
            m_slots[idx].gen += 1;
            m_free.push_back(idx);
        }
    }

    T* get(ShapeId id) {
        auto [idx, gen] = unpack_id(id);
        if (idx >= m_slots.size()) return nullptr;
        Slot& s = m_slots[idx];
        if (!s.valid || s.gen != gen) return nullptr;
        return &s.data;
    }

    const T* get(ShapeId id) const { return const_cast<ShapeStore*>(this)->get(id); }
    bool valid(ShapeId id) const { return get(id) != nullptr; }

    void restore(ShapeId id, const T& item) {
        auto [idx, gen] = unpack_id(id);
        if (idx >= m_slots.size()) m_slots.resize(idx + 1);
        m_slots[idx].data = item;
        m_slots[idx].gen = gen;
        m_slots[idx].valid = true;
        auto it = std::find(m_free.begin(), m_free.end(), idx);
        if (it != m_free.end()) m_free.erase(it);
    }

private:
    static ShapeId make_id(uint32_t idx, uint32_t gen) { return (static_cast<ShapeId>(gen) << 32) | idx; }
    static std::pair<uint32_t, uint32_t> unpack_id(ShapeId id) { return {static_cast<uint32_t>(id & 0xFFFFFFFFu), static_cast<uint32_t>(id >> 32)}; }
    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_free;
};

class ShapeManager {
public:
    static ShapeManager& get_instance() { static ShapeManager instance; return instance; }

    ShapeId add_box(const Box& b) { auto id = m_boxes.add(b); m_type_map[id] = ShapeType::BOX; return id; }
    ShapeId add_circle(const Circle& c) { auto id = m_circles.add(c); m_type_map[id] = ShapeType::CIRCLE; return id; }
    ShapeId add_polygon(const Polygon& p) { auto id = m_polygons.add(p); m_type_map[id] = ShapeType::POLYGON; return id; }
    ShapeId add_path(const Path& p) { auto id = m_paths.add(p); m_type_map[id] = ShapeType::PATH; return id; }
    ShapeId add_arc(const Arc& a) { auto id = m_arcs.add(a); m_type_map[id] = ShapeType::ARC; return id; }

    void remove(ShapeId id) {
        auto it = m_type_map.find(id);
        if (it == m_type_map.end()) return;
        switch (it->second) {
            case ShapeType::BOX: m_boxes.remove(id); break;
            case ShapeType::CIRCLE: m_circles.remove(id); break;
            case ShapeType::POLYGON: m_polygons.remove(id); break;
            case ShapeType::PATH: m_paths.remove(id); break;
            case ShapeType::ARC: m_arcs.remove(id); break;
            default: break;
        }
        m_type_map.erase(it);
    }

    ShapeType get_type(ShapeId id) const { auto it = m_type_map.find(id); return it != m_type_map.end() ? it->second : ShapeType::NONE; }
    Box* get_box(ShapeId id) { return m_boxes.get(id); }
    Circle* get_circle(ShapeId id) { return m_circles.get(id); }
    Polygon* get_polygon(ShapeId id) { return m_polygons.get(id); }
    Path* get_path(ShapeId id) { return m_paths.get(id); }
    Arc* get_arc(ShapeId id) { return m_arcs.get(id); }

    void restore_box(ShapeId id, const Box& b) { m_boxes.restore(id, b); m_type_map[id] = ShapeType::BOX; }
    void restore_circle(ShapeId id, const Circle& c) { m_circles.restore(id, c); m_type_map[id] = ShapeType::CIRCLE; }
    void restore_polygon(ShapeId id, const Polygon& p) { m_polygons.restore(id, p); m_type_map[id] = ShapeType::POLYGON; }
    void restore_path(ShapeId id, const Path& p) { m_paths.restore(id, p); m_type_map[id] = ShapeType::PATH; }
    void restore_arc(ShapeId id, const Arc& a) { m_arcs.restore(id, a); m_type_map[id] = ShapeType::ARC; }

private:
    ShapeManager() = default;
    ShapeStore<Box> m_boxes;
    ShapeStore<Circle> m_circles;
    ShapeStore<Polygon> m_polygons;
    ShapeStore<Path> m_paths;
    ShapeStore<Arc> m_arcs;
    std::unordered_map<ShapeId, ShapeType> m_type_map;
};
