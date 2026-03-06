/**
 * @file car_shape.h
 * @brief Shape storage and indirection handles
 */

#pragma once

#include "car_basic_types.h"
#include "car_reuse_vector.h"

inline ShapeId make_shape_id(ShapeType type, uint32_t idx, uint32_t gen) {
    return (static_cast<ShapeId>(type) << 56) | (static_cast<ShapeId>(gen) << 32) | idx;
}

inline ShapeType extract_type(ShapeId id) {
    return static_cast<ShapeType>(id >> 56);
}

inline uint32_t extract_idx(ShapeId id) {
    return static_cast<uint32_t>(id & 0xFFFFFFFFu);
}

inline uint32_t extract_gen(ShapeId id) {
    return static_cast<uint32_t>((id >> 32) & 0x00FFFFFFu);
}

class ShapeManager {
public:
    static ShapeManager& get_instance() {
        static ShapeManager inst;
        return inst;
    }

    ShapeId add(const Box& b) { return encode(ShapeType::BOX, m_boxes.add(b)); }
    ShapeId add(const Circle& c) { return encode(ShapeType::CIRCLE, m_circles.add(c)); }
    ShapeId add(const Polygon& p) { return encode(ShapeType::POLYGON, m_polygons.add(p)); }
    ShapeId add(const Path& p) { return encode(ShapeType::PATH, m_paths.add(p)); }
    ShapeId add(const Arc& a) { return encode(ShapeType::ARC, m_arcs.add(a)); }

    void remove(ShapeId id) {
        const ObjectId handle = to_handle(id);
        switch (extract_type(id)) {
            case ShapeType::BOX: m_boxes.remove(handle); break;
            case ShapeType::CIRCLE: m_circles.remove(handle); break;
            case ShapeType::POLYGON: m_polygons.remove(handle); break;
            case ShapeType::PATH: m_paths.remove(handle); break;
            case ShapeType::ARC: m_arcs.remove(handle); break;
            default: break;
        }
    }

    Box* get_box(ShapeId id) { return extract_type(id) == ShapeType::BOX ? m_boxes.get(to_handle(id)) : nullptr; }
    Circle* get_circle(ShapeId id) { return extract_type(id) == ShapeType::CIRCLE ? m_circles.get(to_handle(id)) : nullptr; }
    Polygon* get_polygon(ShapeId id) { return extract_type(id) == ShapeType::POLYGON ? m_polygons.get(to_handle(id)) : nullptr; }
    Path* get_path(ShapeId id) { return extract_type(id) == ShapeType::PATH ? m_paths.get(to_handle(id)) : nullptr; }
    Arc* get_arc(ShapeId id) { return extract_type(id) == ShapeType::ARC ? m_arcs.get(to_handle(id)) : nullptr; }

    ShapeType get_type(ShapeId id) const { return extract_type(id); }

    bool valid(ShapeId id) const {
        const ObjectId handle = to_handle(id);
        switch (extract_type(id)) {
            case ShapeType::BOX: return m_boxes.valid(handle);
            case ShapeType::CIRCLE: return m_circles.valid(handle);
            case ShapeType::POLYGON: return m_polygons.valid(handle);
            case ShapeType::PATH: return m_paths.valid(handle);
            case ShapeType::ARC: return m_arcs.valid(handle);
            default: return false;
        }
    }

    size_t size() const {
        return m_boxes.size() + m_circles.size() + m_polygons.size() + m_paths.size() + m_arcs.size();
    }

    void clear() {
        m_boxes.clear();
        m_circles.clear();
        m_polygons.clear();
        m_paths.clear();
        m_arcs.clear();
    }

private:
    static ShapeId encode(ShapeType type, ObjectId handle) {
        return make_shape_id(type, extract_idx(handle), extract_gen(handle));
    }

    static ObjectId to_handle(ShapeId id) {
        return (static_cast<ObjectId>(extract_gen(id)) << 32) | extract_idx(id);
    }

    ShapeManager() = default;

    ReuseVector<Box> m_boxes;
    ReuseVector<Circle> m_circles;
    ReuseVector<Polygon> m_polygons;
    ReuseVector<Path> m_paths;
    ReuseVector<Arc> m_arcs;
};
