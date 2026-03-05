/**
 * @file car_basic_types.h
 * @brief Core basic type definitions for CAR CAE system
 * @version 20.0
 */

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <algorithm>
#include <cstdint>
#include <cstring>

// ========== Core Type Definitions ==========
using DBU = int64_t;           // Database Unit (design unit)
using ObjectId = uint64_t;     // Object global ID (64-bit, contains generation)
using StringId = uint32_t;     // String pool index (0 = empty string)
using LayerId = uint16_t;      // Layer ID
using ExprId = uint64_t;       // Expression ID (0 = literal)
using ShapeId = uint64_t;     // Shape ID

// ========== Global ID Generator ==========
inline ObjectId generate_object_id() {
    static std::atomic<ObjectId> next_id(1);
    return next_id++;
}

// ========== Parametric Value (DBUValue) ==========
struct DBUValue {
    double value = 0.0;
    ExprId expr_id = 0;

    constexpr DBUValue() = default;
    constexpr DBUValue(double v) : value(v), expr_id(0) {}
    constexpr DBUValue(ExprId eid) : value(0.0), expr_id(eid) {}

    bool is_parametric() const { return expr_id != 0; }
};

class ParamPool;
DBU resolve_dbu_value(const DBUValue& v, const ParamPool& pool);

// ========== Geometric Basic Types ==========
struct Point {
    DBUValue x, y;
    Point() = default;
    Point(DBUValue xv, DBUValue yv) : x(xv), y(yv) {}
    Point(double xv, double yv) : x(xv), y(yv) {}
};

struct Vector {
    DBUValue dx, dy;
    Vector() = default;
    Vector(DBUValue dxv, DBUValue dyv) : dx(dxv), dy(dyv) {}
};

// ========== Shape Types ==========
enum class ShapeType : uint8_t { NONE = 0, BOX = 1, CIRCLE = 2, POLYGON = 3, PATH = 4, ARC = 5 };

struct ShapeRef {
    ShapeId id = 0;
    ShapeType type = ShapeType::NONE;
    ShapeRef() = default;
    ShapeRef(ShapeId i, ShapeType t) : id(i), type(t) {}
    bool is_valid() const { return id != 0 && type != ShapeType::NONE; }
};

// ========== Geometric Shapes ==========
struct Box {
    DBUValue x1, y1, x2, y2;
    Box() = default;
    Box(DBU x1_, DBU y1_, DBU x2_, DBU y2_) : x1(x1_), y1(y1_), x2(x2_), y2(y2_) {}
};

struct Circle {
    DBUValue cx, cy, r;
    Circle() = default;
    Circle(DBU cx_, DBU cy_, DBU r_) : cx(cx_), cy(cy_), r(r_) {}
};

struct Polygon { std::vector<Point> vertices; };
struct Path { std::vector<Point> points; bool closed = false; };

struct Arc {
    Point center;
    DBUValue radius;
    DBUValue start_angle;
    DBUValue end_angle;
    bool clockwise = false;
};

// ========== Basic Enumerations ==========
enum class CapType : uint8_t { FLAT = 0, ROUND = 1, SQUARE = 2 };
enum class JoinType : uint8_t { MITER = 0, ROUND = 1, BEVEL = 2 };
enum class OperationType : uint8_t { ADD = 0, REMOVE = 1, REPLACE = 2 };
enum class ObjectType : uint8_t { UNKNOWN = 0, PADSTACK_DEF = 1, TRACE = 2, VIA = 3, COMPONENT = 4, NET = 5, LAYER = 6, SURFACE = 7, BONDWIRE = 8, TEXT = 9, BOARD = 10 };
enum class SegmentType : uint8_t { LINE = 0, ARC = 1 };

// ========== Segment Types ==========
struct LineSegment {
    Point start, end;
    DBUValue width;
    LineSegment() = default;
    LineSegment(Point s, Point e, DBUValue w) : start(s), end(e), width(w) {}
};

struct ArcSegment {
    Point start, end, center;
    DBUValue radius, start_angle, end_angle, width;
    bool clockwise = false;
    ArcSegment() = default;
};

// ========== Small Vector Optimization ==========
template<typename T, size_t N = 4>
class SmallVector {
public:
    SmallVector() = default;
    ~SmallVector() { clear(); }
    
    SmallVector(const SmallVector& o) {
        if (o.is_small()) { m_small = o.m_small; m_flag = o.m_flag; }
        else { m_ptr = new std::vector<T>(*o.m_ptr); m_flag = 1; }
    }
    
    SmallVector(SmallVector&& o) noexcept { m_small = o.m_small; m_flag = o.m_flag; o.m_flag = 0; }
    
    void push_back(const T& v) {
        if (is_small()) {
            if (m_small.size < N) m_small.data[m_small.size++] = v;
            else { upgrade_to_vector(); m_ptr->push_back(v); }
        } else m_ptr->push_back(v);
    }
    
    void push_back(T&& v) {
        if (is_small()) {
            if (m_small.size < N) m_small.data[m_small.size++] = std::move(v);
            else { upgrade_to_vector(); m_ptr->push_back(std::move(v)); }
        } else m_ptr->push_back(std::move(v);
    }
    
    size_t size() const { return is_small() ? m_small.size : m_ptr->size(); }
    T& operator[](size_t i) { return is_small() ? m_small.data[i] : (*m_ptr)[i]; }
    const T& operator[](size_t i) const { return is_small() ? m_small.data[i] : (*m_ptr)[i]; }
    void clear() { if (!is_small()) { delete m_ptr; m_ptr = nullptr; } m_small.size = 0; }
    bool empty() const { return size() == 0; }

private:
    bool is_small() const { return m_flag == 0; }
    void upgrade_to_vector() {
        auto* p = new std::vector<T>();
        for (size_t i = 0; i < m_small.size; ++i) p->push_back(std::move(m_small.data[i]));
        m_ptr = p; m_flag = 1;
    }
    struct SmallData { T data[N]; size_t size = 0; };
    union { SmallData m_small; std::vector<T>* m_ptr; };
    uint8_t m_flag = 0;
};

// ========== Bounding Box ==========
struct BBox {
    DBUValue x1, y1, x2, y2;
    BBox() = default;
    BBox(DBUValue a, DBUValue b, DBUValue c, DBUValue d) : x1(a), y1(b), x2(c), y2(d) {}
    DBU width() const { return x2.value - x1.value; }
    DBU height() const { return y2.value - y1.value; }
    bool contains(const Point& p) const { return p.x.value >= x1.value && p.x.value <= x2.value && p.y.value >= y1.value && p.y.value <= y2.value; }
    bool intersects(const BBox& o) const { return !(x2.value < o.x1.value || x1.value > o.x2.value || y2.value < o.y1.value || y1.value > o.y2.value); }
};

inline DBU resolve_dbu_value(const DBUValue& v, const ParamPool&) {
    if (!v.is_parametric()) return static_cast<DBU>(v.value);
    return static_cast<DBU>(v.value);
}
