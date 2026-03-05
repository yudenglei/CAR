/**
 * @file car_quadtree.h
 * @brief QuadTree and layer index helpers
 */

#pragma once

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "car_basic_types.h"

constexpr int QUADTREE_MAX_DEPTH = 8;
constexpr int QUADTREE_MAX_OBJECTS = 8;

struct QuadObject {
    ObjectId id = 0;
    BBox bbox{};
};

class QuadTree {
public:
    QuadTree() : QuadTree(BBox{}, 0) {}
    QuadTree(const BBox& bounds, int depth) : m_bounds(bounds), m_depth(depth) {}

    void clear() {
        m_objects.clear();
        m_children.clear();
    }

    bool insert(const QuadObject& obj) {
        if (!m_bounds.intersects(obj.bbox)) {
            return false;
        }

        if (m_children.empty()) {
            m_objects.push_back(obj);
            if (static_cast<int>(m_objects.size()) > QUADTREE_MAX_OBJECTS && m_depth < QUADTREE_MAX_DEPTH) {
                subdivide();
            }
            return true;
        }

        int idx = child_index_for(obj.bbox);
        if (idx >= 0) {
            return m_children[static_cast<size_t>(idx)]->insert(obj);
        }

        m_objects.push_back(obj);
        return true;
    }

    void query(const BBox& area, std::vector<ObjectId>& out) const {
        if (!m_bounds.intersects(area)) {
            return;
        }

        for (const auto& obj : m_objects) {
            if (obj.bbox.intersects(area)) {
                out.push_back(obj.id);
            }
        }

        for (const auto& child : m_children) {
            child->query(area, out);
        }
    }

private:
    int child_index_for(const BBox& bb) const {
        if (m_children.empty()) {
            return -1;
        }

        const DBU mid_x = static_cast<DBU>((m_bounds.x1.value + m_bounds.x2.value) * 0.5);
        const DBU mid_y = static_cast<DBU>((m_bounds.y1.value + m_bounds.y2.value) * 0.5);

        const bool in_left = bb.x2.value <= mid_x;
        const bool in_right = bb.x1.value >= mid_x;
        const bool in_bottom = bb.y2.value <= mid_y;
        const bool in_top = bb.y1.value >= mid_y;

        if (in_left && in_bottom) return 0;
        if (in_right && in_bottom) return 1;
        if (in_left && in_top) return 2;
        if (in_right && in_top) return 3;
        return -1;
    }

    void subdivide() {
        const DBU mid_x = static_cast<DBU>((m_bounds.x1.value + m_bounds.x2.value) * 0.5);
        const DBU mid_y = static_cast<DBU>((m_bounds.y1.value + m_bounds.y2.value) * 0.5);

        m_children.reserve(4);
        m_children.push_back(std::make_unique<QuadTree>(BBox(m_bounds.x1.value, m_bounds.y1.value, mid_x, mid_y), m_depth + 1));
        m_children.push_back(std::make_unique<QuadTree>(BBox(mid_x, m_bounds.y1.value, m_bounds.x2.value, mid_y), m_depth + 1));
        m_children.push_back(std::make_unique<QuadTree>(BBox(m_bounds.x1.value, mid_y, mid_x, m_bounds.y2.value), m_depth + 1));
        m_children.push_back(std::make_unique<QuadTree>(BBox(mid_x, mid_y, m_bounds.x2.value, m_bounds.y2.value), m_depth + 1));

        std::vector<QuadObject> remain;
        remain.reserve(m_objects.size());
        for (const auto& obj : m_objects) {
            int idx = child_index_for(obj.bbox);
            if (idx >= 0) {
                m_children[static_cast<size_t>(idx)]->insert(obj);
            } else {
                remain.push_back(obj);
            }
        }
        m_objects.swap(remain);
    }

    BBox m_bounds{};
    int m_depth = 0;
    std::vector<QuadObject> m_objects;
    std::vector<std::unique_ptr<QuadTree>> m_children;
};

class LayerIndex {
public:
    enum class Kind : uint8_t { TRACE, SURFACE, VIA };

    void add(LayerId layer, ObjectId id, Kind kind) { map_for(kind)[layer].push_back(id); }

    void remove(LayerId layer, ObjectId id, Kind kind) {
        auto& vec = map_for(kind)[layer];
        vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
    }

    const std::vector<ObjectId>& get(LayerId layer, Kind kind) const {
        static const std::vector<ObjectId> kEmpty;
        const auto& map = map_for(kind);
        auto it = map.find(layer);
        return it == map.end() ? kEmpty : it->second;
    }

private:
    using BucketMap = std::unordered_map<LayerId, std::vector<ObjectId>>;

    BucketMap& map_for(Kind kind) {
        if (kind == Kind::TRACE) return m_traces;
        if (kind == Kind::SURFACE) return m_surfaces;
        return m_vias;
    }

    const BucketMap& map_for(Kind kind) const {
        if (kind == Kind::TRACE) return m_traces;
        if (kind == Kind::SURFACE) return m_surfaces;
        return m_vias;
    }

    BucketMap m_traces;
    BucketMap m_surfaces;
    BucketMap m_vias;
};
