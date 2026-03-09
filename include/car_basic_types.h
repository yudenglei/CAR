/**
 * @file car_basic_types.h
 * @brief Core basic types for CAR
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

using DBU = int64_t;
using ObjectId = uint64_t;
using StringId = uint32_t;
using LayerId = uint16_t;
using ExprId = uint64_t;
using ShapeId = uint64_t;

inline ObjectId generate_object_id() {
    static std::atomic<ObjectId> next{1};
    return next++;
}

struct DBUValue {
    double value = 0.0;
    ExprId expr_id = 0;

    constexpr DBUValue() = default;
    constexpr DBUValue(double v) : value(v), expr_id(0) {}
    static constexpr DBUValue from_expr(ExprId id) { DBUValue v; v.expr_id = id; return v; }

    bool is_parametric() const { return expr_id != 0; }
};

struct Point {
    DBUValue x{};
    DBUValue y{};

    Point() = default;
    Point(DBUValue xv, DBUValue yv) : x(xv), y(yv) {}
    Point(DBU xv, DBU yv) : x(DBUValue(static_cast<double>(xv))), y(DBUValue(static_cast<double>(yv))) {}
};

struct Vector {
    DBUValue dx{};
    DBUValue dy{};

    Vector() = default;
    Vector(DBUValue xv, DBUValue yv) : dx(xv), dy(yv) {}
    Vector(DBU xv, DBU yv) : dx(DBUValue(static_cast<double>(xv))), dy(DBUValue(static_cast<double>(yv))) {}
};

enum class ShapeType : uint8_t { NONE = 0, BOX = 1, CIRCLE = 2, POLYGON = 3, PATH = 4, ARC = 5 };

struct ShapeRef {
    ShapeId id = 0;
    ShapeType type = ShapeType::NONE;

    ShapeRef() = default;
    ShapeRef(ShapeId shape_id, ShapeType t) : id(shape_id), type(t) {}
    bool is_valid() const { return id != 0 && type != ShapeType::NONE; }
};

struct Box {
    DBUValue x1{}, y1{}, x2{}, y2{};
    Box() = default;
    Box(DBUValue ax1, DBUValue ay1, DBUValue ax2, DBUValue ay2) : x1(ax1), y1(ay1), x2(ax2), y2(ay2) {}
    Box(DBU ax1, DBU ay1, DBU ax2, DBU ay2) : x1(DBUValue(static_cast<double>(ax1))), y1(DBUValue(static_cast<double>(ay1))), x2(DBUValue(static_cast<double>(ax2))), y2(DBUValue(static_cast<double>(ay2))) {}
};

struct Circle {
    DBUValue cx{}, cy{}, r{};
    Circle() = default;
    Circle(DBUValue x, DBUValue y, DBUValue radius) : cx(x), cy(y), r(radius) {}
    Circle(DBU x, DBU y, DBU radius) : cx(DBUValue(static_cast<double>(x))), cy(DBUValue(static_cast<double>(y))), r(DBUValue(static_cast<double>(radius))) {}
};

struct Polygon {
    std::vector<Point> vertices;
};

struct Path {
    std::vector<Point> points;
    bool closed = false;
};

struct Arc {
    Point center{};
    DBUValue radius{};
    DBUValue start_angle{};
    DBUValue end_angle{};
    bool clockwise = false;
};

enum class CapType : uint8_t { FLAT = 0, ROUND = 1, SQUARE = 2 };
enum class JoinType : uint8_t { MITER = 0, ROUND = 1, BEVEL = 2 };

struct LineSegment {
    Point start{};
    Point end{};
    DBUValue width{};

    LineSegment() = default;
    LineSegment(Point s, Point e, DBUValue w) : start(s), end(e), width(w) {}
    LineSegment(Point s, Point e, DBU w) : start(s), end(e), width(DBUValue(static_cast<double>(w))) {}
};

struct ArcSegment {
    Point start{};
    Point end{};
    Point center{};
    DBUValue radius{};
    DBUValue start_angle{};
    DBUValue end_angle{};
    DBUValue width{};
    bool clockwise = false;
};

struct BBox {
    DBUValue x1{}, y1{}, x2{}, y2{};

    BBox() = default;
    BBox(DBUValue ax1, DBUValue ay1, DBUValue ax2, DBUValue ay2) : x1(ax1), y1(ay1), x2(ax2), y2(ay2) {}
    BBox(DBU ax1, DBU ay1, DBU ax2, DBU ay2) : x1(DBUValue(static_cast<double>(ax1))), y1(DBUValue(static_cast<double>(ay1))), x2(DBUValue(static_cast<double>(ax2))), y2(DBUValue(static_cast<double>(ay2))) {}

    bool contains(const Point& p) const {
        return p.x.value >= x1.value && p.x.value <= x2.value && p.y.value >= y1.value && p.y.value <= y2.value;
    }

    bool intersects(const BBox& other) const {
        return !(x2.value < other.x1.value || x1.value > other.x2.value || y2.value < other.y1.value || y1.value > other.y2.value);
    }
};
