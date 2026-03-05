/**
 * @file car_shape.h
 * @brief Shape system - Optimized using ReuseVector
 * @version 20.1
 */

#pragma once

#include <variant>
#include "car_basic_types.h"
#include "car_reuse_vector.h"

using ShapeId = uint64_t;

class ShapeManager {
public:
    static ShapeManager& get_instance() { static ShapeManager instance; return instance; }

    ShapeManager(const ShapeManager&) = delete;
    ShapeManager& operator=(const ShapeManager&) = delete;

    // Add shapes
    ShapeId add_box(const Box& b) { ShapeData d; d.type = ShapeType::BOX; d.box = b; return m_shapes.add(std::move(d)); }
    ShapeId add_circle(const Circle& c) { ShapeData d; d.type = ShapeType::CIRCLE; d.circle = c; return m_shapes.add(std::move(d)); }
    ShapeId add_polygon(const Polygon& p) { ShapeData d; d.type = ShapeType::POLYGON; d.polygon = p; return m_shapes.add(std::move(d)); }
    ShapeId add_path(const Path& p) { ShapeData d; d.type = ShapeType::PATH; d.path = p; return m_shapes.add(std::move(d)); }
    ShapeId add_arc(const Arc& a) { ShapeData d; d.type = ShapeType::ARC; d.arc = a; return m_shapes.add(std::move(d)); }

    void remove(ShapeId id) { m_shapes.remove(id); }
    ShapeType get_type(ShapeId id) const { const ShapeData* d = m_shapes.get(id); return d ? d->type : ShapeType::NONE; }
    
    Box* get_box(ShapeId id) { ShapeData* d = m_shapes.get(id); return (d && d->type == ShapeType::BOX) ? &d->box : nullptr; }
    Circle* get_circle(ShapeId id) { ShapeData* d = m_shapes.get(id); return (d && d->type == ShapeType::CIRCLE) ? &d->circle : nullptr; }
    Polygon* get_polygon(ShapeId id) { ShapeData* d = m_shapes.get(id); return (d && d->type == ShapeType::POLYGON) ? &d->polygon : nullptr; }
    Path* get_path(ShapeId id) { ShapeData* d = m_shapes.get(id); return (d && d->type == ShapeType::PATH) ? &d->path : nullptr; }
    Arc* get_arc(ShapeId id) { ShapeData* d = m_shapes.get(id); return (d && d->type == ShapeType::ARC) ? &d->arc : nullptr; }

    bool valid(ShapeId id) const { return m_shapes.valid(id); }
    std::pair<uint32_t, uint32_t> get_slot_info(ShapeId id) const { return m_shapes.get_slot_info(id); }
    void restore(ShapeId id, const ShapeData& data) { auto [idx, gen] = m_shapes.get_slot_info(id); if (idx != UINT32_MAX) m_shapes.restore(idx, data, gen); }
    size_t size() const { return m_shapes.size(); }
    void clear() { m_shapes.clear(); }

private:
    ShapeManager() = default;

    struct ShapeData {
        ShapeType type = ShapeType::NONE;
        Box box;
        Circle circle;
        Polygon polygon;
        Path path;
        Arc arc;
    };

    ReuseVector<ShapeData> m_shapes;
};

// Backward compatibility
template<typename T> using ShapeStore = ReuseVector<T>;
