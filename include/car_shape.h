/**
 * @file car_shape.h
 * @brief Shape system - KLayout style
 * @version 20.2
 */

#pragma once

#include "car_basic_types.h"
#include "car_reuse_vector.h"

using ShapeId = uint64_t;

// ShapeId: [type:8][gen:16][idx:32]
inline ShapeId make_shape_id(ShapeType type, uint32_t idx, uint32_t gen) {
    return (static_cast<ShapeId>(type) << 48) | (static_cast<ShapeId>(gen) << 32) | idx;
}
inline ShapeType extract_type(ShapeId id) { return static_cast<ShapeType>(id >> 48); }
inline uint32_t extract_idx(ShapeId id) { return static_cast<uint32_t>(id & 0xFFFFFFFFu); }
inline uint32_t extract_gen(ShapeId id) { return static_cast<uint32_t>((id >> 32) & 0xFFFF); }

class ShapeManager {
public:
    static ShapeManager& get_instance() { static ShapeManager i; return i; }

    ShapeId add(const Box& b) { ObjectId h = m_boxes.add(b); return make_shape_id(ShapeType::BOX, extract_idx(h), extract_gen(h)); }
    ShapeId add(const Circle& c) { ObjectId h = m_circles.add(c); return make_shape_id(ShapeType::CIRCLE, extract_idx(h), extract_gen(h)); }
    ShapeId add(const Polygon& p) { ObjectId h = m_polygons.add(p); return make_shape_id(ShapeType::POLYGON, extract_idx(h), extract_gen(h)); }
    ShapeId add(const Path& p) { ObjectId h = m_paths.add(p); return make_shape_id(ShapeType::PATH, extract_idx(h), extract_gen(h)); }
    ShapeId add(const Arc& a) { ObjectId h = m_arcs.add(a); return make_shape_id(ShapeType::ARC, extract_idx(h), extract_gen(h)); }

    void remove(ShapeId id) {
        switch (extract_type(id)) {
            case ShapeType::BOX: m_boxes.remove(id); break;
            case ShapeType::CIRCLE: m_circles.remove(id); break;
            case ShapeType::POLYGON: m_polygons.remove(id); break;
            case ShapeType::PATH: m_paths.remove(id); break;
            case ShapeType::ARC: m_arcs.remove(id); break;
            default: break;
        }
    }

    Box* get_box(ShapeId id) { return extract_type(id) == ShapeType::BOX ? m_boxes.get(id) : nullptr; }
    Circle* get_circle(ShapeId id) { return extract_type(id) == ShapeType::CIRCLE ? m_circles.get(id) : nullptr; }
    Polygon* get_polygon(ShapeId id) { return extract_type(id) == ShapeType::POLYGON ? m_polygons.get(id) : nullptr; }
    Path* get_path(ShapeId id) { return extract_type(id) == ShapeType::PATH ? m_paths.get(id) : nullptr; }
    Arc* get_arc(ShapeId id) { return extract_type(id) == ShapeType::ARC ? m_arcs.get(id) : nullptr; }

    ShapeType get_type(ShapeId id) const { return extract_type(id); }

    bool valid(ShapeId id) const {
        switch (extract_type(id)) {
            case ShapeType::BOX: return m_boxes.valid(id);
            case ShapeType::CIRCLE: return m_circles.valid(id);
            case ShapeType::POLYGON: return m_polygons.valid(id);
            case ShapeType::PATH: return m_paths.valid(id);
            case ShapeType::ARC: return m_arcs.valid(id);
            default: return false;
        }
    }

    template<typename T> ReuseVector<T>* get_container();
    template<> ReuseVector<Box>* get_container<Box>() { return &m_boxes; }
    template<> ReuseVector<Circle>* get_container<Circle>() { return &m_circles; }
    template<> ReuseVector<Polygon>* get_container<Polygon>() { return &m_polygons; }
    template<> ReuseVector<Path>* get_container<Path>() { return &m_paths; }
    template<> ReuseVector<Arc>* get_container<Arc>() { return &m_arcs; }

    size_t size() const { return m_boxes.size() + m_circles.size() + m_polygons.size() + m_paths.size() + m_arcs.size(); }
    void clear() { m_boxes.clear(); m_circles.clear(); m_polygons.clear(); m_paths.clear(); m_arcs.clear(); }

private:
    ShapeManager() = default;
    ReuseVector<Box> m_boxes;
    ReuseVector<Circle> m_circles;
    ReuseVector<Polygon> m_polygons;
    ReuseVector<Path> m_paths;
    ReuseVector<Arc> m_arcs;
};
