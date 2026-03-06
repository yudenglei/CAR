/**
 * @file car_usage_example.h
 * @brief Usage examples - Simplified API v3.1
 * @version 3.1
 * 
 * Key improvements:
 * 1. No need to call get_container - auto-gets internally
 * 2. Batch operations support
 * 3. TypeTrait handles type ID mapping
 */

#pragma once

#include "car_transaction_v3.h"
#include "car_pcb_objects.h"

namespace car {

// ============================================================================
// Type Trait Specializations
// ============================================================================

// Define type mappings - like KLayout's internal type system
template<> struct TypeTrait<Trace> { 
    static constexpr uint16_t id = ObjectTypeId::Trace; 
};

template<> struct TypeTrait<Via> { 
    static constexpr uint16_t id = ObjectTypeId::Via; 
};

template<> struct TypeTrait<Component> { 
    static constexpr uint16_t id = ObjectTypeId::Component; 
};

template<> struct TypeTrait<Net> { 
    static constexpr uint16_t id = ObjectTypeId::Net; 
};

template<> struct TypeTrait<Layer> { 
    static constexpr uint16_t id = ObjectTypeId::Layer; 
};

// ============================================================================
// Example: PCBDatabase using new simplified API
// ============================================================================

/**
 * @brief Simple PCBDatabase - Just use TransactionManager directly!
 * 
 * NO explicit get_container calls! NO registration! NO callbacks!
 * Just use the template methods - container is auto-created.
 */
class PCBDatabase {
public:
    // Storage
    ReuseVector<Trace> traces;
    ReuseVector<Via> vias;
    ReuseVector<Component> components;
    ReuseVector<Net> nets;
    ReuseVector<Layer> layers;
    
    // Transaction manager - just use it directly!
    TransactionManager transactions;
    
    // Layer index (manual, simple)
    std::unordered_map<LayerId, std::vector<ObjectId>> layer_index;
    
    // =========================================================================
    // SIMPLE API: No get_container needed!
    // =========================================================================
    
    /**
     * @brief Add single trace - Simple!
     * 
     * Usage:
     *   ObjectId id = db.add_trace(layer_id, trace_obj);
     * 
     * Internally:
     *   1. Auto-gets container (like KLayout get_layer)
     *   2. Adds object to storage
     *   3. Records change for undo
     */
    ObjectId add_trace(LayerId layer, const Trace& trace) {
        // No get_container call needed! Auto-created internally.
        ObjectId id = transactions.add(traces, trace);
        layer_index[layer].push_back(id);
        return id;
    }
    
    /**
     * @brief Add single via - Same pattern
     */
    ObjectId add_via(LayerId layer, const Via& via) {
        // Auto-gets container internally
        ObjectId id = transactions.add(vias, via);
        layer_index[layer].push_back(id);
        return id;
    }
    
    // =========================================================================
    // BATCH OPERATIONS: Add multiple objects in ONE transaction!
    // 
    // Like KLayout's batch operations - huge performance improvement
    // =========================================================================
    
    /**
     * @brief Add multiple traces - Batch operation
     * 
     * Usage:
     *   std::vector<Trace> traces = {trace1, trace2, trace3, ...};
     *   auto ids = db.add_traces(layer_id, traces);
     * 
     * Creates ONE transaction for ALL traces (not one per trace!)
     * Undo/redo works on the entire batch.
     */
    std::vector<ObjectId> add_traces(LayerId layer, const std::vector<Trace>& traces_to_add) {
        // Auto-gets container internally
        auto ids = transactions.add(traces, traces_to_add);
        auto& vec = layer_index[layer];
        for (ObjectId id : ids) {
            vec.push_back(id);
        }
        return ids;
    }
    
    /**
     * @brief Add multiple vias - Batch operation
     */
    std::vector<ObjectId> add_vias(LayerId layer, const std::vector<Via>& vias_to_add) {
        auto ids = transactions.add(vias, vias_to_add);
        auto& vec = layer_index[layer];
        for (ObjectId id : ids) {
            vec.push_back(id);
        }
        return ids;
    }
    
    // =========================================================================
    // Replace operations
    // =========================================================================
    
    /**
     * @brief Replace trace
     * 
     * Usage:
     *   bool ok = db.replace_trace(handle, new_trace);
     */
    bool replace_trace(ObjectId handle, const Trace& new_trace) {
        return transactions.replace(traces, handle, new_trace);
    }
    
    /**
     * @brief Replace via
     */
    bool replace_via(ObjectId handle, const Via& new_via) {
        return transactions.replace(vias, handle, new_via);
    }
    
    // =========================================================================
    // Erase operations
    // =========================================================================
    
    /**
     * @brief Remove trace
     * 
     * Usage:
     *   bool ok = db.remove_trace(layer_id, handle);
     */
    bool remove_trace(LayerId layer, ObjectId handle) {
        auto& vec = layer_index[layer];
        vec.erase(std::remove(vec.begin(), vec.end(), handle), vec.end());
        return transactions.erase(traces, handle);
    }
    
    /**
     * @brief Remove multiple traces - Batch erase
     * 
     * Usage:
     *   std::vector<ObjectId> handles = {h1, h2, h3, ...};
     *   db.remove_traces(layer_id, handles);
     * 
     * One transaction for all erasures.
     */
    size_t remove_traces(LayerId layer, const std::vector<ObjectId>& handles) {
        auto& vec = layer_index[layer];
        for (ObjectId h : handles) {
            vec.erase(std::remove(vec.begin(), vec.end(), h), vec.end());
        }
        return transactions.erase(traces, handles);
    }
    
    /**
     * @brief Remove via
     */
    bool remove_via(LayerId layer, ObjectId handle) {
        auto& vec = layer_index[layer];
        vec.erase(std::remove(vec.begin(), vec.end(), handle), vec.end());
        return transactions.erase(vias, handle);
    }
    
    // =========================================================================
    // Modify operations
    // =========================================================================
    
    /**
     * @brief Modify trace in place
     * 
     * Usage:
     *   db.modify_trace(handle, [](Trace& t) {
     *       t.width = 50;
     *       t.net = "GND";
     *   });
     */
    template<typename Modifier>
    bool modify_trace(ObjectId handle, Modifier&& mod) {
        return transactions.modify(traces, handle, std::forward<Modifier>(mod));
    }
    
    /**
     * @brief Modify via in place
     */
    template<typename Modifier>
    bool modify_via(ObjectId handle, Modifier&& mod) {
        return transactions.modify(vias, handle, std::forward<Modifier>(mod));
    }
    
    // =========================================================================
    // Undo/Redo - Works on batch operations too!
    // =========================================================================
    
    bool undo() { return transactions.undo(); }
    bool redo() { return transactions.redo(); }
    bool can_undo() const { return transactions.can_undo(); }
    bool can_redo() const { return transactions.can_redo(); }
    
    // =========================================================================
    // Example: Bulk load from file
    // =========================================================================
    
    /**
     * @brief Bulk load traces from Gerber file
     * 
     * This is where batch operations shine!
     * Loading 10000 traces = 1 transaction, not 10000.
     */
    size_t bulk_load_traces(LayerId layer, const std::vector<Trace>& traces_to_load) {
        if (traces_to_load.empty()) return 0;
        
        // Single transaction for ALL traces
        auto ids = add_traces(layer, traces_to_load);
        return ids.size();
    }
};

// ============================================================================
// Usage Example Code
// ============================================================================

void example_usage() {
    PCBDatabase db;
    
    // Single object operations
    Trace t1 = {{0, 0}, {100, 0}, 50};
    Trace t2 = {{100, 0}, {200, 0}, 50};
    ObjectId id1 = db.add_trace(1, t1);
    ObjectId id2 = db.add_trace(1, t2);
    
    // Batch operations - MUCH faster!
    std::vector<Trace> traces;
    for (int i = 0; i < 1000; ++i) {
        traces.push_back({{i * 10.0, 0}, {(i + 1) * 10.0, 0}, 50});
    }
    auto ids = db.add_traces(1, traces);  // One transaction!
    
    // Modify
    db.modify_trace(id1, [](Trace& t) {
        t.width = 100;
    });
    
    // Replace
    Trace new_trace = {{0, 0}, {100, 100}, 75};
    db.replace_trace(id1, new_trace);
    
    // Batch erase
    std::vector<ObjectId> to_erase = {id1, id2};
    db.remove_traces(1, to_erase);  // One transaction!
    
    // Undo/Redo
    if (db.can_undo()) {
        db.undo();  // Undoes last operation (batch or single)
    }
    if (db.can_redo()) {
        db.redo();  // Redoes last undone operation
    }
}

} // namespace car
