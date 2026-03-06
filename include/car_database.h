/**
 * @file car_database.h
 * @brief Main PCB database with layer-op style transaction log (no function callbacks)
 */

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "car_pcb_objects.h"
#include "car_quadtree.h"
#include "car_reuse_vector.h"
#include "car_string_pool.h"

class PCBDatabase {
public:
    enum class EntityKind : uint8_t {
        PADSTACK, TRACE, VIA, COMPONENT, NET, LAYER, LAYERSTACK, SURFACE, BONDWIRE, PORT, SYMBOL, BOARD,
    };

    enum class OpType : uint8_t { INSERT, ERASE, REPLACE };

    struct LayerOp {
        OpType op = OpType::INSERT;
        EntityKind kind = EntityKind::TRACE;
        ObjectId handle = 0;
        ObjectId before_ref = 0; // archive handle for old object when needed
        ObjectId after_ref = 0;  // archive handle for new object when needed
    };

    struct Transaction {
        std::string desc;
        std::vector<LayerOp> ops;
    };

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
    std::unique_ptr<QuadTree> quadtree;

    template <EntityKind K>
    using EntityT = std::conditional_t<K == EntityKind::PADSTACK, PadstackDef,
        std::conditional_t<K == EntityKind::TRACE, Trace,
        std::conditional_t<K == EntityKind::VIA, Via,
        std::conditional_t<K == EntityKind::COMPONENT, Component,
        std::conditional_t<K == EntityKind::NET, Net,
        std::conditional_t<K == EntityKind::LAYER, Layer,
        std::conditional_t<K == EntityKind::LAYERSTACK, LayerStack,
        std::conditional_t<K == EntityKind::SURFACE, Surface,
        std::conditional_t<K == EntityKind::BONDWIRE, BondWire,
        std::conditional_t<K == EntityKind::PORT, Port,
        std::conditional_t<K == EntityKind::SYMBOL, Symbol, Board>>>>>>>>>>>;

    template <EntityKind K>
    ObjectId insert(const EntityT<K>& obj, const std::string& desc = "insert") {
        auto& c = container<K>();
        ObjectId handle = c.add(obj);
        on_insert<K>(handle, obj);

        LayerOp op;
        op.op = OpType::INSERT;
        op.kind = K;
        op.handle = handle;
        op.after_ref = archive_add<K>(obj);
        push_tx(desc, op);
        return handle;
    }

    template <EntityKind K>
    bool replace(ObjectId handle, const EntityT<K>& next, const std::string& desc = "replace") {
        auto& c = container<K>();
        const EntityT<K>* cur = c.get(handle);
        if (!cur) return false;

        const EntityT<K> prev = *cur;
        on_erase<K>(handle, prev);
        c.replace(handle, next);
        on_insert<K>(handle, next);

        LayerOp op;
        op.op = OpType::REPLACE;
        op.kind = K;
        op.handle = handle;
        op.before_ref = archive_add<K>(prev);
        op.after_ref = archive_add<K>(next);
        push_tx(desc, op);
        return true;
    }

    template <EntityKind K>
    bool erase(ObjectId handle, const std::string& desc = "erase") {
        auto& c = container<K>();
        const EntityT<K>* cur = c.get(handle);
        if (!cur) return false;

        const EntityT<K> prev = *cur;
        on_erase<K>(handle, prev);
        c.remove(handle);

        LayerOp op;
        op.op = OpType::ERASE;
        op.kind = K;
        op.handle = handle;
        op.before_ref = archive_add<K>(prev);
        push_tx(desc, op);
        return true;
    }

    bool undo() {
        if (m_undo.empty()) return false;
        Transaction tx = std::move(m_undo.back());
        m_undo.pop_back();

        m_replaying = true;
        for (auto it = tx.ops.rbegin(); it != tx.ops.rend(); ++it) {
            apply_inverse(*it);
        }
        m_replaying = false;

        m_redo.push_back(std::move(tx));
        return true;
    }

    bool redo() {
        if (m_redo.empty()) return false;
        Transaction tx = std::move(m_redo.back());
        m_redo.pop_back();

        m_replaying = true;
        for (const auto& op : tx.ops) {
            apply_forward(op);
        }
        m_replaying = false;

        m_undo.push_back(std::move(tx));
        return true;
    }

    bool can_undo() const { return !m_undo.empty(); }
    bool can_redo() const { return !m_redo.empty(); }

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

    void init_quadtree(const BBox& bounds) { quadtree = std::make_unique<QuadTree>(bounds, 0); }

    void clear() {
        padstacks.clear(); traces.clear(); vias.clear(); components.clear(); nets.clear(); layers.clear();
        layerstacks.clear(); surfaces.clear(); bondwires.clear(); ports.clear(); symbols.clear(); boards.clear();
        m_archive_padstacks.clear(); m_archive_traces.clear(); m_archive_vias.clear(); m_archive_components.clear();
        m_archive_nets.clear(); m_archive_layers.clear(); m_archive_layerstacks.clear(); m_archive_surfaces.clear();
        m_archive_bondwires.clear(); m_archive_ports.clear(); m_archive_symbols.clear(); m_archive_boards.clear();
        m_name_by_object.clear(); m_undo.clear(); m_redo.clear();
        strings.clear(); shapes.clear();
        if (quadtree) quadtree->clear();
    }

private:
    PCBDatabase() = default;

    template <typename T, typename = void>
    struct has_name_id : std::false_type {};
    template <typename T>
    struct has_name_id<T, std::void_t<decltype(std::declval<T>().name_id)>> : std::true_type {};

    template <typename T>
    void index_name_if_exists(ObjectId id, const T& obj) {
        if constexpr (has_name_id<T>::value) {
            if (obj.name_id != 0) m_name_by_object[id] = obj.name_id;
        }
    }

    template <typename T>
    void unindex_name_if_exists(ObjectId id, const T& obj) {
        if constexpr (has_name_id<T>::value) {
            auto it = m_name_by_object.find(id);
            if (it != m_name_by_object.end() && (obj.name_id == 0 || it->second == obj.name_id)) {
                m_name_by_object.erase(it);
            }
        }
    }

    template <EntityKind K>
    ReuseVector<EntityT<K>>& container() {
        if constexpr (K == EntityKind::PADSTACK) return padstacks;
        else if constexpr (K == EntityKind::TRACE) return traces;
        else if constexpr (K == EntityKind::VIA) return vias;
        else if constexpr (K == EntityKind::COMPONENT) return components;
        else if constexpr (K == EntityKind::NET) return nets;
        else if constexpr (K == EntityKind::LAYER) return layers;
        else if constexpr (K == EntityKind::LAYERSTACK) return layerstacks;
        else if constexpr (K == EntityKind::SURFACE) return surfaces;
        else if constexpr (K == EntityKind::BONDWIRE) return bondwires;
        else if constexpr (K == EntityKind::PORT) return ports;
        else if constexpr (K == EntityKind::SYMBOL) return symbols;
        else return boards;
    }

    template <EntityKind K>
    ReuseVector<EntityT<K>>& archive_container() {
        if constexpr (K == EntityKind::PADSTACK) return m_archive_padstacks;
        else if constexpr (K == EntityKind::TRACE) return m_archive_traces;
        else if constexpr (K == EntityKind::VIA) return m_archive_vias;
        else if constexpr (K == EntityKind::COMPONENT) return m_archive_components;
        else if constexpr (K == EntityKind::NET) return m_archive_nets;
        else if constexpr (K == EntityKind::LAYER) return m_archive_layers;
        else if constexpr (K == EntityKind::LAYERSTACK) return m_archive_layerstacks;
        else if constexpr (K == EntityKind::SURFACE) return m_archive_surfaces;
        else if constexpr (K == EntityKind::BONDWIRE) return m_archive_bondwires;
        else if constexpr (K == EntityKind::PORT) return m_archive_ports;
        else if constexpr (K == EntityKind::SYMBOL) return m_archive_symbols;
        else return m_archive_boards;
    }

    template <EntityKind K>
    ObjectId archive_add(const EntityT<K>& obj) {
        return archive_container<K>().add(obj);
    }

    template <EntityKind K>
    const EntityT<K>& archive_get(ObjectId ref) {
        return *archive_container<K>().get(ref);
    }

    template <EntityKind K>
    void on_insert(ObjectId id, const EntityT<K>& obj) {
        index_name_if_exists(id, obj);
        if constexpr (K == EntityKind::TRACE) layer_index.add(obj.layer_id, id, LayerIndex::Kind::TRACE);
        if constexpr (K == EntityKind::VIA) layer_index.add(obj.start_layer, id, LayerIndex::Kind::VIA);
        if constexpr (K == EntityKind::SURFACE) layer_index.add(obj.layer_id, id, LayerIndex::Kind::SURFACE);
    }

    template <EntityKind K>
    void on_erase(ObjectId id, const EntityT<K>& obj) {
        if constexpr (K == EntityKind::TRACE) layer_index.remove(obj.layer_id, id, LayerIndex::Kind::TRACE);
        if constexpr (K == EntityKind::VIA) layer_index.remove(obj.start_layer, id, LayerIndex::Kind::VIA);
        if constexpr (K == EntityKind::SURFACE) layer_index.remove(obj.layer_id, id, LayerIndex::Kind::SURFACE);
        unindex_name_if_exists(id, obj);
    }

    void push_tx(const std::string& desc, LayerOp op) {
        if (m_replaying) return;
        Transaction tx;
        tx.desc = desc;
        tx.ops.push_back(std::move(op));
        m_undo.push_back(std::move(tx));
        if (m_undo.size() > MAX_UNDO) m_undo.erase(m_undo.begin());
        m_redo.clear();
    }

    template <EntityKind K>
    void apply_forward_t(const LayerOp& op) {
        auto& c = container<K>();
        if (op.op == OpType::INSERT) {
            const auto& obj = archive_get<K>(op.after_ref);
            c.restore(op.handle, obj);
            on_insert<K>(op.handle, obj);
        } else if (op.op == OpType::ERASE) {
            if (const auto* cur = c.get(op.handle)) on_erase<K>(op.handle, *cur);
            c.remove(op.handle);
        } else {
            const auto& obj = archive_get<K>(op.after_ref);
            if (const auto* cur = c.get(op.handle)) on_erase<K>(op.handle, *cur);
            c.replace(op.handle, obj);
            on_insert<K>(op.handle, obj);
        }
    }

    template <EntityKind K>
    void apply_inverse_t(const LayerOp& op) {
        auto& c = container<K>();
        if (op.op == OpType::INSERT) {
            if (const auto* cur = c.get(op.handle)) on_erase<K>(op.handle, *cur);
            c.remove(op.handle);
        } else if (op.op == OpType::ERASE) {
            const auto& old_obj = archive_get<K>(op.before_ref);
            c.restore(op.handle, old_obj);
            on_insert<K>(op.handle, old_obj);
        } else {
            const auto& old_obj = archive_get<K>(op.before_ref);
            if (const auto* cur = c.get(op.handle)) on_erase<K>(op.handle, *cur);
            c.replace(op.handle, old_obj);
            on_insert<K>(op.handle, old_obj);
        }
    }

    void apply_forward(const LayerOp& op) {
        switch (op.kind) {
            case EntityKind::PADSTACK: apply_forward_t<EntityKind::PADSTACK>(op); break;
            case EntityKind::TRACE: apply_forward_t<EntityKind::TRACE>(op); break;
            case EntityKind::VIA: apply_forward_t<EntityKind::VIA>(op); break;
            case EntityKind::COMPONENT: apply_forward_t<EntityKind::COMPONENT>(op); break;
            case EntityKind::NET: apply_forward_t<EntityKind::NET>(op); break;
            case EntityKind::LAYER: apply_forward_t<EntityKind::LAYER>(op); break;
            case EntityKind::LAYERSTACK: apply_forward_t<EntityKind::LAYERSTACK>(op); break;
            case EntityKind::SURFACE: apply_forward_t<EntityKind::SURFACE>(op); break;
            case EntityKind::BONDWIRE: apply_forward_t<EntityKind::BONDWIRE>(op); break;
            case EntityKind::PORT: apply_forward_t<EntityKind::PORT>(op); break;
            case EntityKind::SYMBOL: apply_forward_t<EntityKind::SYMBOL>(op); break;
            case EntityKind::BOARD: apply_forward_t<EntityKind::BOARD>(op); break;
        }
    }

    void apply_inverse(const LayerOp& op) {
        switch (op.kind) {
            case EntityKind::PADSTACK: apply_inverse_t<EntityKind::PADSTACK>(op); break;
            case EntityKind::TRACE: apply_inverse_t<EntityKind::TRACE>(op); break;
            case EntityKind::VIA: apply_inverse_t<EntityKind::VIA>(op); break;
            case EntityKind::COMPONENT: apply_inverse_t<EntityKind::COMPONENT>(op); break;
            case EntityKind::NET: apply_inverse_t<EntityKind::NET>(op); break;
            case EntityKind::LAYER: apply_inverse_t<EntityKind::LAYER>(op); break;
            case EntityKind::LAYERSTACK: apply_inverse_t<EntityKind::LAYERSTACK>(op); break;
            case EntityKind::SURFACE: apply_inverse_t<EntityKind::SURFACE>(op); break;
            case EntityKind::BONDWIRE: apply_inverse_t<EntityKind::BONDWIRE>(op); break;
            case EntityKind::PORT: apply_inverse_t<EntityKind::PORT>(op); break;
            case EntityKind::SYMBOL: apply_inverse_t<EntityKind::SYMBOL>(op); break;
            case EntityKind::BOARD: apply_inverse_t<EntityKind::BOARD>(op); break;
        }
    }

    static constexpr std::size_t MAX_UNDO = 256;

    bool m_replaying = false;
    std::vector<Transaction> m_undo;
    std::vector<Transaction> m_redo;
    std::unordered_map<ObjectId, StringId> m_name_by_object;

    // per-kind immutable object archive for undo/redo payload references
    ReuseVector<PadstackDef> m_archive_padstacks;
    ReuseVector<Trace> m_archive_traces;
    ReuseVector<Via> m_archive_vias;
    ReuseVector<Component> m_archive_components;
    ReuseVector<Net> m_archive_nets;
    ReuseVector<Layer> m_archive_layers;
    ReuseVector<LayerStack> m_archive_layerstacks;
    ReuseVector<Surface> m_archive_surfaces;
    ReuseVector<BondWire> m_archive_bondwires;
    ReuseVector<Port> m_archive_ports;
    ReuseVector<Symbol> m_archive_symbols;
    ReuseVector<Board> m_archive_boards;
};
