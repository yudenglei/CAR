/**
 * @file car_database.h
 * @brief Main PCB database
 */

#pragma once

#include <memory>
#include "car_basic_types.h"
#include "car_string_pool.h"
#include "car_reuse_vector.h"
#include "car_shape.h"
#include "car_pcb_objects.h"
#include "car_transaction.h"
#include "car_quadtree.h"

class PCBDatabase {
public:
    static PCBDatabase& get_instance() { static PCBDatabase instance; return instance; }

    ReuseVector<PadstackDef> padstacks;
    ReuseVector<Trace> traces;
    ReuseVector<Via> vias;
    ReuseVector<Component> components;
    ReuseVector<Net> nets;
    ReuseVector<Layer> layers;
    ReuseVector<Surface> surfaces;
    ReuseVector<BondWire> bondwires;
    ReuseVector<Board> boards;

    TransactionManager transactions;
    LayerIndex layer_index;
    std::unique_ptr<QuadTree> quadtree;

    StringPool& strings = StringPool::get_instance();
    ShapeManager& shapes = ShapeManager::get_instance();

    ObjectId add_trace(LayerId layer, const Trace& t) {
        transactions.begin("Add trace");
        ObjectId h = traces.add(t);
        auto [idx, gen] = traces.get_slot_info(h);
        Change c(OperationType::REMOVE, ObjectType::TRACE, h, idx, gen);
        transactions.record(std::move(c));
        transactions.commit();
        layer_index.add(layer, h, "Trace");
        return h;
    }

    void update_trace(ObjectId handle, const Trace& new_trace) {
        transactions.begin("Update trace");
        auto [old_idx, old_gen] = traces.get_slot_info(handle);
        Change c(OperationType::REPLACE, ObjectType::TRACE, handle, old_idx, old_gen);
        const Trace* old_trace = traces.get(handle);
        if (old_trace) {
            size_t obj_size = sizeof(Trace) + old_trace->segments.size() * sizeof(LineSegment);
            if (obj_size > LARGE_OBJECT_THRESHOLD) {
                c.is_large_object = true;
                c.snapshot_id = SnapshotCache::get_instance().add_snapshot(Snapshot::create_large(std::make_shared<Trace>(*old_trace)));
            } else {
                c.snapshot = Snapshot::create_small(old_trace, sizeof(Trace));
            }
        }
        transactions.record(std::move(c));
        traces.remove(handle);
        traces.add(new_trace);
        transactions.commit();
    }

    void remove_trace(ObjectId handle) {
        transactions.begin("Remove trace");
        auto [idx, gen] = traces.get_slot_info(handle);
        Change c(OperationType::ADD, ObjectType::TRACE, handle, idx, gen);
        transactions.record(std::move(c));
        Trace* t = traces.get(handle);
        if (t) layer_index.remove(t->layer_id, handle, "Trace");
        traces.remove(handle);
        transactions.commit();
    }

    ObjectId add_via(const Via& v) {
        transactions.begin("Add via");
        ObjectId h = vias.add(v);
        auto [idx, gen] = vias.get_slot_info(h);
        Change c(OperationType::REMOVE, ObjectType::VIA, h, idx, gen);
        transactions.record(std::move(c));
        transactions.commit();
        layer_index.add(v.start_layer, h, "Via");
        return h;
    }

    ObjectId add_component(const Component& c) {
        transactions.begin("Add component");
        ObjectId h = components.add(c);
        auto [idx, gen] = components.get_slot_info(h);
        Change c2(OperationType::REMOVE, ObjectType::COMPONENT, h, idx, gen);
        transactions.record(std::move(c2));
        transactions.commit();
        return h;
    }

    ObjectId add_net(const Net& n) {
        transactions.begin("Add net");
        ObjectId h = nets.add(n);
        auto [idx, gen] = nets.get_slot_info(h);
        Change c(OperationType::REMOVE, ObjectType::NET, h, idx, gen);
        transactions.record(std::move(c));
        transactions.commit();
        return h;
    }

    ObjectId add_layer(const Layer& l) { return layers.add(l); }

    bool undo() { return transactions.undo(); }
    bool redo() { return transactions.redo(); }
    bool can_undo() const { return transactions.can_undo(); }
    bool can_redo() const { return transactions.can_redo(); }

    void init_quadtree(const BBox& bounds) { quadtree = std::make_unique<QuadTree>(bounds, 0); }

    void clear() {
        padstacks.clear(); traces.clear(); vias.clear(); components.clear();
        nets.clear(); layers.clear(); surfaces.clear(); bondwires.clear(); boards.clear();
        strings.clear();
        if (quadtree) quadtree->clear();
    }

private:
    PCBDatabase() { transactions.set_database(this); }
    PCBDatabase(const PCBDatabase&) = delete;
    PCBDatabase& operator=(const PCBDatabase&) = delete;
};
