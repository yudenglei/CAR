/**
 * @file car_shape_flags.h
 * @brief Simplified ShapeFlags - KLayout style simple bitmask
 * @version 4.0 - Simplified per KLayout design
 * 
 * Design philosophy from KLayout:
 * - Use simple uint32_t bitmask, no complex class hierarchy
 * - Flags are just integers that can be OR'd together
 * - No virtual functions, no heavy objects
 */

#pragma once

#include <cstdint>

namespace car {

// ============================================================================
// Simple ShapeFlags - Just uint32_t bitmask
// ============================================================================

using ShapeFlags = uint32_t;

// Flag values - simple bitmask like KLayout
namespace Flags {
    // State flags (commonly used)
    constexpr ShapeFlags None       = 0;
    constexpr ShapeFlags Selected   = 1 << 0;   // 0x01 - Object is selected
    constexpr ShapeFlags Locked     = 1 << 1;   // 0x02 - Object is locked
    constexpr ShapeFlags Hidden     = 1 << 2;   // 0x04 - Object is hidden
    constexpr ShapeFlags Valid      = 1 << 3;   // 0x08 - Object is valid
    constexpr ShapeFlags Modified  = 1 << 4;   // 0x10 - Object has been modified
    
    // Property flags
    constexpr ShapeFlags Filled     = 1 << 5;   // 0x20 - Shape is filled
    constexpr ShapeFlags Hole       = 1 << 6;   // 0x40 - Shape is a hole
    constexpr ShapeFlags Round      = 1 << 7;   // 0x80 - Has round corners
    
    // Type flags (for shape type identification)
    constexpr ShapeFlags Box        = 1 << 10;  // 0x400 - Box shape
    constexpr ShapeFlags Polygon    = 1 << 11;  // 0x800 - Polygon shape
    constexpr ShapeFlags Circle     = 1 << 12;  // 0x1000 - Circle shape
    constexpr ShapeFlags Ellipse    = 1 << 13;  // 0x2000 - Ellipse shape
    constexpr ShapeFlags Ring       = 1 << 14;  // 0x4000 - Ring shape
    constexpr ShapeFlags Edge       = 1 << 15;  // 0x8000 - Edge shape
    constexpr ShapeFlags Path       = 1 << 16;  // 0x10000 - Path shape
    constexpr ShapeFlags Arc        = 1 << 17;  // 0x20000 - Arc shape
    constexpr ShapeFlags Text       = 1 << 18;  // 0x40000 - Text shape
    constexpr ShapeFlags Via        = 1 << 19;  // 0x80000 - Via shape
    constexpr ShapeFlags Trace      = 1 << 20;  // 0x100000 - Trace shape
    constexpr ShapeFlags Component  = 1 << 21;  // 0x200000 - Component shape
    constexpr ShapeFlags Net        = 1 << 22;  // 0x400000 - Net shape
}

// ============================================================================
// Helper functions - Simple inline functions
// ============================================================================

inline ShapeFlags make_flags(ShapeFlags f) { return f; }

// Combine flags
inline ShapeFlags operator|(ShapeFlags a, ShapeFlags b) {
    return static_cast<ShapeFlags>(a | b);
}

inline ShapeFlags& operator|=(ShapeFlags& a, ShapeFlags b) {
    a = static_cast<ShapeFlags>(a | b);
    return a;
}

// Check flags
inline bool has_flag(ShapeFlags flags, ShapeFlags f) {
    return (flags & f) != 0;
}

inline bool has_any_flag(ShapeFlags flags, ShapeFlags f) {
    return (flags & f) != 0;
}

inline bool has_all_flags(ShapeFlags flags, ShapeFlags f) {
    return (flags & f) == f;
}

// Set/clear flags
inline ShapeFlags set_flag(ShapeFlags flags, ShapeFlags f) {
    return static_cast<ShapeFlags>(flags | f);
}

inline ShapeFlags clear_flag(ShapeFlags flags, ShapeFlags f) {
    return static_cast<ShapeFlags>(flags & ~f);
}

inline ShapeFlags toggle_flag(ShapeFlags flags, ShapeFlags f) {
    return static_cast<ShapeFlags>(flags ^ f);
}

// ============================================================================
// ShapeHeader - Simple header with flags
// 
// Just like KLayout: minimal overhead, embedded in shape structure
// ============================================================================

struct ShapeHeader {
    ShapeFlags flags = Flags::None;
    
    // Convenience methods
    bool is_selected() const   { return has_flag(flags, Flags::Selected); }
    bool is_locked() const     { return has_flag(flags, Flags::Locked); }
    bool is_hidden() const     { return has_flag(flags, Flags::Hidden); }
    bool is_valid() const      { return has_flag(flags, Flags::Valid); }
    bool is_modified() const   { return has_flag(flags, Flags::Modified); }
    bool is_filled() const     { return has_flag(flags, Flags::Filled); }
    bool is_hole() const       { return has_flag(flags, Flags::Hole); }
    bool is_round() const      { return has_flag(flags, Flags::Round); }
    
    void select()   { flags = set_flag(flags, Flags::Selected); }
    void deselect() { flags = clear_flag(flags, Flags::Selected); }
    void lock()     { flags = set_flag(flags, Flags::Locked); }
    void unlock()   { flags = clear_flag(flags, Flags::Locked); }
    void hide()     { flags = set_flag(flags, Flags::Hidden); }
    void show()     { flags = clear_flag(flags, Flags::Hidden); }
    void validate() { flags = set_flag(flags, Flags::Valid); }
    void invalidate(){ flags = clear_flag(flags, Flags::Valid); }
    void mark_modified() { flags = set_flag(flags, Flags::Modified); }
    void clear_modified() { flags = clear_flag(flags, Flags::Modified); }
    
    void set_flags(ShapeFlags f) { flags = f; }
    void add_flags(ShapeFlags f) { flags = set_flag(flags, f); }
    void remove_flags(ShapeFlags f) { flags = clear_flag(flags, f); }
    
    ShapeFlags get_flags() const { return flags; }
};

// ============================================================================
// Type Trait - Map shape types to flags
// ============================================================================

template<typename T>
struct ShapeTypeFlags {
    static constexpr ShapeFlags flags = 0;
};

// Specialize for each type
template<> struct ShapeTypeFlags<struct Box> {
    static constexpr ShapeFlags flags = Flags::Box | Flags::Filled;
};

template<> struct ShapeTypeFlags<struct Polygon> {
    static constexpr ShapeFlags flags = Flags::Polygon | Flags::Filled;
};

template<> struct ShapeTypeFlags<struct Circle> {
    static constexpr ShapeFlags flags = Flags::Circle | Flags::Filled;
};

template<> struct ShapeTypeFlags<struct Edge> {
    static constexpr ShapeFlags flags = Flags::Edge;
};

template<> struct ShapeTypeFlags<struct Path> {
    static constexpr ShapeFlags flags = Flags::Path;
};

template<> struct ShapeTypeFlags<struct Arc> {
    static constexpr ShapeFlags flags = Flags::Arc;
};

template<> struct ShapeTypeFlags<struct Text> {
    static constexpr ShapeFlags flags = Flags::Text;
};

template<> struct ShapeTypeFlags<struct Via> {
    static constexpr ShapeFlags flags = Flags::Via;
};

template<> struct ShapeTypeFlags<struct Trace> {
    static constexpr ShapeFlags flags = Flags::Trace;
};

template<> struct ShapeTypeFlags<struct Component> {
    static constexpr ShapeFlags flags = Flags::Component;
};

template<> struct ShapeTypeFlags<struct Net> {
    static constexpr ShapeFlags flags = Flags::Net;
};

} // namespace car
