/**
 * @file car_database.h
 * @brief Main PCB database with lightweight containers and undo/redo
 */

#pragma once

#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "car_pcb_objects.h"
#include "car_quadtree.h"
#include "car_reuse_vector.h"
#include "car_string_pool.h"
#include "car_transaction.h"

class PCBDatabase {
public:
    static PCBDatabase& get_instance() {
        static PCBDatabase instance;
        return instance;
    }

    StringPool& strings = StringPool::get_instance();
    ShapeManager& shapes = ShapeManager::get_instance();

    ReuseVector<PadstackDef> padstacks;
    ReuseVector<Trace> traces;
    ReuseVector<Via> vias;
    ReuseVector<Component> components;
    ReuseVector<Net> nets;
    ReuseVector<Layer> layers;
    ReuseVector<LayerStack> layerstacks;
    ReuseVector<Surface> surfaces;
    ReuseVector<BondWire> bondwires;
    ReuseVector<Port> ports;
    ReuseVector<Symbol> symbols;
    ReuseVector<Board> boards;

    LayerIndex layer_index;
    TransactionManager transactions;
    std::unique_ptr<QuadTree> quadtree;

    ObjectId add_trace(LayerId layer, const Trace& trace) {
        ObjectId handle = traces.add(trace);
        layer_index.add(layer, handle, LayerIndex::Kind::TRACE);
        index_name(handle, trace.name_id);

        transactions.begin("add_trace");
        transactions.record({
            [this, layer, handle]() {
                if (const auto* t = traces.get(handle)) unindex_name(handle, t->name_id);
                traces.remove(handle);
                layer_index.remove(layer, handle, LayerIndex::Kind::TRACE);
            },
            [this, trace, layer, handle]() {
                traces.restore(handle, trace);
                layer_index.add(layer, handle, LayerIndex::Kind::TRACE);
                index_name(handle, trace.name_id);
            },
        });
        transactions.commit();

        return handle;
    }

    bool replace_trace(ObjectId handle, const Trace& next) {
        Trace* cur = traces.get(handle);
        if (!cur) {
            return false;
        }

        Trace prev = *cur;
        unindex_name(handle, prev.name_id);
        traces.replace(handle, next);
        index_name(handle, next.name_id);

        transactions.begin("replace_trace");
        transactions.record({
            [this, handle, prev]() {
                if (const auto* t = traces.get(handle)) unindex_name(handle, t->name_id);
                traces.replace(handle, prev);
                index_name(handle, prev.name_id);
            },
            [this, handle, next]() {
                if (const auto* t = traces.get(handle)) unindex_name(handle, t->name_id);
                traces.replace(handle, next);
                index_name(handle, next.name_id);
            },
        });
        transactions.commit();
        return true;
    }

    bool remove_trace(ObjectId handle) {
        Trace* t = traces.get(handle);
        if (!t) {
            return false;
        }
        Trace snapshot = *t;
        const LayerId layer = t->layer_id;
        unindex_name(handle, snapshot.name_id);
        traces.remove(handle);
        layer_index.remove(layer, handle, LayerIndex::Kind::TRACE);

        transactions.begin("remove_trace");
        transactions.record({
            [this, handle, snapshot, layer]() {
                traces.restore(handle, snapshot);
                layer_index.add(layer, handle, LayerIndex::Kind::TRACE);
                index_name(handle, snapshot.name_id);
            },
            [this, handle, layer, snapshot]() {
                if (const auto* cur = traces.get(handle)) unindex_name(handle, cur->name_id);
                traces.remove(handle);
                layer_index.remove(layer, handle, LayerIndex::Kind::TRACE);
            },
        });
        transactions.commit();
        return true;
    }

    ObjectId add_via(const Via& via) {
        ObjectId handle = vias.add(via);
        layer_index.add(via.start_layer, handle, LayerIndex::Kind::VIA);
        index_name(handle, via.name_id);
        return handle;
    }

    ObjectId add_layer(const Layer& layer) { return layers.add(layer); }

    std::vector<ObjectId> find_by_name_prefix(std::string_view prefix) const {
        std::vector<ObjectId> out;
        for (const auto& [id, sid] : m_name_by_object) {
            std::string_view s = strings.get(sid);
            if (!prefix.empty() && s.substr(0, prefix.size()) == prefix) {
                out.push_back(id);
            }
        }
        return out;
    }

    bool undo() { return transactions.undo(); }
    bool redo() { return transactions.redo(); }
    bool can_undo() const { return transactions.can_undo(); }
    bool can_redo() const { return transactions.can_redo(); }

    void init_quadtree(const BBox& bounds) { quadtree = std::make_unique<QuadTree>(bounds, 0); }

    void clear() {
        padstacks.clear();
        traces.clear();
        vias.clear();
        components.clear();
        nets.clear();
        layers.clear();
        layerstacks.clear();
        surfaces.clear();
        bondwires.clear();
        ports.clear();
        symbols.clear();
        boards.clear();

        m_name_by_object.clear();
        strings.clear();
        shapes.clear();
        if (quadtree) {
            quadtree->clear();
        }
    }

private:
    PCBDatabase() = default;

    void index_name(ObjectId id, StringId sid) { if (sid != 0) m_name_by_object[id] = sid; }
    void unindex_name(ObjectId id, StringId sid) {
        auto it = m_name_by_object.find(id);
        if (it != m_name_by_object.end() && (sid == 0 || it->second == sid)) m_name_by_object.erase(it);
    }

    std::unordered_map<ObjectId, StringId> m_name_by_object;
};
