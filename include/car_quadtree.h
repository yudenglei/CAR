/**
 * @file car_quadtree.h
 * @brief QuadTree spatial index for efficient queries
 */

#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include "car_basic_types.h"

using ObjectId = uint64_t;

constexpr int QUADTREE_MAX_DEPTH = 8;
constexpr int QUADTREE_MAX_OBJECTS = 4;

struct QuadObject {
    ObjectId id;
    BBox bbox;
};

/**
 * @class QuadTree
 * @brief QuadTree spatial index for 2D queries
 */
class QuadTree {
public:
    QuadTree() : QuadTree(BBox{}, 0) {}
    QuadTree(const BBox& bounds, int depth) : m_bounds(bounds), m_depth(depth) {}

    void clear() {
        m_objects.clear();
        m_children.clear();
    }

    bool insert(const QuadObject& obj) {
        if (!m_bounds.intersects(obj.bbox)) return false;
        
        if (m_children.empty()) {
            m_objects.push_back(obj);
            if (m_objects.size() > QUADTREE_MAX_OBJECTS && m_depth < QUADTREE_MAX_DEPTH) {
                subdivide();
            }
            return true;
        }

        for (auto& child : m_children) {
            if (child->insert(obj)) return true;
        }
        return false;
    }

    void query(const BBox& area, std::vector<ObjectId>& results) const {
        if (!m_bounds.intersects(area)) return;

        for (const auto& obj : m_objects) {
            if (area.intersects(obj.bbox)) {
                results.push_back(obj.id);
            }
        }

        for (const auto& child : m_children) {
            child->query(area, results);
        }
    }

    void query_point(const Point& pt, std::vector<ObjectId>& results) const {
        if (!m_bounds.contains(pt)) return;

        for (const auto& obj : m_objects) {
            if (obj.bbox.contains(pt)) {
                results.push_back(obj.id);
            }
        }

        for (const auto& child : m_children) {
            child->query_point(pt, results);
        }
    }

    bool remove(ObjectId id) {
        for (auto it = m_objects.begin(); it != m_objects.end(); ++it) {
            if (it->id == id) {
                m_objects.erase(it);
                return true;
            }
        }

        for (auto& child : m_children) {
            if (child->remove(id)) return true;
        }
        return false;
    }

private:
    void subdivide() {
        DBU mx = (m_bounds.x1.value + m_bounds.x2.value) / 2;
        DBU my = (m_bounds.y1.value + m_bounds.y2.value) / 2;

        m_children.push_back(std::make_unique<QuadTree>(BBox{m_bounds.x1, m_bounds.y1, mx, my}, m_depth + 1));
        m_children.push_back(std::make_unique<QuadTree>(BBox{mx, m_bounds.y1, m_bounds.x2, my}, m_depth + 1));
        m_children.push_back(std::make_unique<QuadTree>(BBox{m_bounds.x1, my, mx, m_bounds.y2}, m_depth + 1));
        m_children.push_back(std::make_unique<QuadTree>(BBox{mx, my, m_bounds.x2, m_bounds.y2}, m_depth + 1));

        for (auto& obj : m_objects) {
            bool inserted = false;
            for (auto& child : m_children) {
                if (child->insert(obj)) { inserted = true; break; }
            }
            if (!inserted) m_objects.push_back(obj);
        }
        m_objects.clear();
    }

    BBox m_bounds;
    int m_depth = 0;
    std::vector<QuadObject> m_objects;
    std::vector<std::unique_ptr<QuadTree>> m_children;
};

/**
 * @class LayerIndex
 * @brief Layer-based object index
 */
class LayerIndex {
public:
    void add(LayerId layer, ObjectId id, const std::string& type) {
        if (type == "Trace") m_traces[layer].push_back(id);
        else if (type == "Surface") m_surfaces[layer].push_back(id);
        else if (type == "Via") m_vias[layer].push_back(id);
    }

    void remove(LayerId layer, ObjectId id, const std::string& type) {
        auto remove_from = [&](auto& map, const char* t) {
            if (type != t) return;
            auto& vec = map[layer];
            vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
        };
        remove_from(m_traces, "Trace");
        remove_from(m_surfaces, "Surface");
        remove_from(m_vias, "Via");
    }

    const std::vector<ObjectId>& get(const std::string& type, LayerId layer) const {
        static const std::vector<ObjectId> empty;
        if (type == "Trace") { auto it = m_traces.find(layer); return it != m_traces.end() ? it->second : empty; }
        if (type == "Surface") { auto it = m_surfaces.find(layer); return it != m_surfaces.end() ? it->second : empty; }
        if (type == "Via") { auto it = m_vias.find(layer); return it != m_vias.end() ? it->second : empty; }
        return empty;
    }

private:
    std::unordered_map<LayerId, std::vector<ObjectId>> m_traces;
    std::unordered_map<LayerId, std::vector<ObjectId>> m_surfaces;
    std::unordered_map<LayerId, std::vector<ObjectId>> m_vias;
};

#include <unordered_map>
