/**
 * @file car_pcb_objects.h
 * @brief PCB object definitions
 */

#pragma once

#include <vector>
#include <cstdint>
#include "car_basic_types.h"
#include "car_reuse_vector.h"
#include "car_shape.h"

using ObjectId = uint64_t;
using StringId = uint32_t;
using LayerId = uint16_t;

inline ObjectId generate_object_id() { static std::atomic<uint64_t> next_id(1); return next_id++; }

// ========== Pad ==========
struct Pad {
    ShapeRef shape;
    LayerId layer_id;
    DBUValue rotation;
    Vector offset;
    Pad() = default;
    Pad(ShapeId shape_id, LayerId layer) : shape(shape_id, ShapeType::BOX), layer_id(layer) {}
};

// ========== Drill ==========
struct Drill {
    ShapeRef shape;
    bool plated = true;
    Drill() = default;
    Drill(ShapeId shape_id) : shape(shape_id, ShapeType::CIRCLE) {}
};

// ========== PadstackDef ==========
struct PadstackDef {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    LayerId layers_mask = 0;
    std::vector<Pad> pads;
    std::vector<Drill> drills;
    bool is_pin = true;
    bool is_via = false;
};

// ========== Pin ==========
struct Pin {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    ObjectId component = 0;
    ObjectId padstack = 0;
    Point rel_pos;
    ObjectId net = ~0ULL;
    int16_t rotation = 0;
    bool mirrored = false;
};

// ========== Component ==========
struct Component {
    ObjectId id = generate_object_id();
    StringId refdes = 0;
    StringId desc_id = 0;
    StringId category_id = 0;
    ObjectId footprint = 0;
    Point position;
    double rotation = 0.0;
    bool mirrored = false;
    std::vector<Pin> pins;
};

// ========== Trace ==========
struct Trace {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    LayerId layer_id = 0;
    ObjectId net = ~0ULL;
    CapType start_cap = CapType::ROUND;
    CapType end_cap = CapType::ROUND;
    JoinType join = JoinType::MITER;
    std::vector<LineSegment> segments;
    std::vector<ArcSegment> arcs;
    bool has_arc() const { return !arcs.empty(); }
};

// ========== Via ==========
struct Via {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    Point position;
    ObjectId padstack = 0;
    ObjectId net = ~0ULL;
    LayerId start_layer = 0;
    LayerId end_layer = 0;
};

// ========== BondWire ==========
struct BondWire {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    Point start_point;
    Point end_point;
    DBUValue diameter;
    LayerId start_layer = 0;
    LayerId end_layer = 0;
    ObjectId net = ~0ULL;
};

// ========== Net ==========
struct Net {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    StringId desc_id = 0;
    std::vector<ObjectId> pins;
    std::vector<ObjectId> vias;
    std::vector<ObjectId> traces;
    enum class NetClass : uint8_t { SIGNAL = 0, POWER = 1, GND = 2, DIFF = 3 };
    NetClass net_class = NetClass::SIGNAL;
};

// ========== Layer ==========
struct Layer {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    int32_t number = 0;
    enum class Type : uint8_t { SIGNAL = 0, POWER = 1, DIELECTRIC = 2, SOLDER_MASK = 3, PASTE = 4, SILKSCREEN = 5 };
    Type type = Type::SIGNAL;
    DBUValue thickness;
    StringId material_id = 0;
    uint32_t color = 0;
    bool visible = true;
};

// ========== Surface ==========
struct Surface {
    ObjectId id = generate_object_id();
    LayerId layer_id = 0;
    ObjectId net = 0;
    std::vector<Polygon> polygons;
    std::vector<Polygon> holes;
};

// ========== Board ==========
struct Board {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    ObjectId layer_stack_id = 0;
    Box outline;
    std::vector<ObjectId> components;
    std::vector<ObjectId> nets;
    std::vector<ObjectId> traces;
    std::vector<ObjectId> vias;
};

#include <atomic>
