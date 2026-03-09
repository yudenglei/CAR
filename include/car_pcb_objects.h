/**
 * @file car_pcb_objects.h
 * @brief PCB object model (lightweight handles + pooled strings)
 */

#pragma once

#include <vector>

#include "car_basic_types.h"
#include "car_shape.h"

struct Pad {
    ShapeRef shape{};
    LayerId layer_id = 0;
    DBUValue rotation{};
    Vector offset{};
};

struct Drill {
    ShapeRef shape{};
    bool plated = true;
};

struct PadstackDef {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    LayerId from_layer = 0;
    LayerId to_layer = 0;
    std::vector<Pad> pads;
    std::vector<Drill> drills;
};

struct Pin {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    ObjectId component = 0;
    ObjectId padstack = 0;
    Point rel_pos{};
    ObjectId net = 0;
    int16_t rotation = 0;
    bool mirrored = false;
};

struct Via {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    Point position{};
    ObjectId padstack = 0;
    ObjectId net = 0;
    LayerId start_layer = 0;
    LayerId end_layer = 0;
};

struct Component {
    ObjectId id = generate_object_id();
    StringId refdes = 0;
    StringId desc_id = 0;
    StringId category_id = 0;
    Point position{};
    DBUValue rotation{};
    bool mirrored = false;
    std::vector<Pin> pins;
};

struct Trace {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    LayerId layer_id = 0;
    ObjectId net = 0;
    CapType start_cap = CapType::ROUND;
    CapType end_cap = CapType::ROUND;
    JoinType join = JoinType::MITER;
    std::vector<LineSegment> segments;
    std::vector<ArcSegment> arcs;
};

struct BondWire {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    Point start_point{};
    Point end_point{};
    DBUValue diameter{};
    LayerId start_layer = 0;
    LayerId end_layer = 0;
    ObjectId net = 0;
};

struct Port {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    Point position{};
    LayerId layer = 0;
    ObjectId net = 0;
};

struct Symbol {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    std::vector<ShapeRef> graphics;
};

struct Net {
    enum class NetClass : uint8_t { SIGNAL = 0, POWER = 1, GND = 2, DIFF = 3 };

    ObjectId id = generate_object_id();
    StringId name_id = 0;
    StringId desc_id = 0;
    std::vector<ObjectId> pins;
    std::vector<ObjectId> vias;
    std::vector<ObjectId> traces;
    NetClass net_class = NetClass::SIGNAL;
};

struct Layer {
    enum class Type : uint8_t { SIGNAL = 0, POWER = 1, DIELECTRIC = 2, MASK = 3, SILK = 4, PASTE = 5 };

    ObjectId id = generate_object_id();
    StringId name_id = 0;
    int32_t number = 0;
    Type type = Type::SIGNAL;
    DBUValue thickness{};
    StringId material_id = 0;
    bool visible = true;
};

struct LayerStack {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    std::vector<ObjectId> ordered_layers;
};

struct Surface {
    ObjectId id = generate_object_id();
    LayerId layer_id = 0;
    ObjectId net = 0;
    std::vector<Polygon> polygons;
    std::vector<Polygon> holes;
};

struct Parameter {
    StringId key = 0;
    StringId value = 0;
};

struct Board {
    ObjectId id = generate_object_id();
    StringId name_id = 0;
    ObjectId layer_stack_id = 0;
    Box outline{};
    std::vector<Parameter> parameters;
    std::vector<ObjectId> components;
    std::vector<ObjectId> nets;
    std::vector<ObjectId> traces;
    std::vector<ObjectId> vias;
};
