/**
 * @file car_database.h
 * @brief Main PCB database with KLayout-style typed layer-op transaction log
 */

#pragma once

#include <cstdint>
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

    enum class OpType : uint8_t { INSERT, ERASE };

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
    struct LayerOp {
        OpType op = OpType::INSERT;
        std::vector<ObjectId> handles;
        std::vector<EntityT<K>> objects;
    };

    struct OpOrder {
        EntityKind kind = EntityKind::TRACE;
        uint32_t index = 0;
    };

    struct Transaction {
        std::string desc;
        std::vector<OpOrder> order;
        std::vector<LayerOp<EntityKind::PADSTACK>> padstack_ops;
        std::vector<LayerOp<EntityKind::TRACE>> trace_ops;
        std::vector<LayerOp<EntityKind::VIA>> via_ops;
        std::vector<LayerOp<EntityKind::COMPONENT>> component_ops;
        std::vector<LayerOp<EntityKind::NET>> net_ops;
        std::vector<LayerOp<EntityKind::LAYER>> layer_ops;
        std::vector<LayerOp<EntityKind::LAYERSTACK>> layerstack_ops;
        std::vector<LayerOp<EntityKind::SURFACE>> surface_ops;
        std::vector<LayerOp<EntityKind::BONDWIRE>> bondwire_ops;
        std::vector<LayerOp<EntityKind::PORT>> port_ops;
        std::vector<LayerOp<EntityKind::SYMBOL>> symbol_ops;
        std::vector<LayerOp<EntityKind::BOARD>> board_ops;

        bool empty() const { return order.empty(); }
    };

    template <typename T>
    struct KindOf;

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

    void begin(const std::string& desc) {
        if (m_in_tx) return;
        m_open_tx = Transaction{};
        m_open_tx.desc = desc;
        m_in_tx = true;
    }

    void commit() {
        if (!m_in_tx) return;
        if (!m_open_tx.empty()) {
            m_undo.push_back(std::move(m_open_tx));
            if (m_undo.size() > MAX_UNDO) m_undo.erase(m_undo.begin());
            m_redo.clear();
        }
        m_open_tx = Transaction{};
        m_in_tx = false;
    }

    void rollback() {
        m_open_tx = Transaction{};
        m_in_tx = false;
    }

    template <typename T>
    ObjectId insert(const T& obj, const std::string& desc = "insert") {
        return insert_k<KindOf<std::decay_t<T>>::value>(obj, desc);
    }

    template <typename T>
    bool replace(ObjectId handle, const T& next, const std::string& desc = "replace") {
        return replace_k<KindOf<std::decay_t<T>>::value>(handle, next, desc);
    }

    bool erase(ObjectId handle, EntityKind kind, const std::string& desc = "erase") {
        switch (kind) {
            case EntityKind::PADSTACK: return erase_k<EntityKind::PADSTACK>(handle, desc);
            case EntityKind::TRACE: return erase_k<EntityKind::TRACE>(handle, desc);
            case EntityKind::VIA: return erase_k<EntityKind::VIA>(handle, desc);
            case EntityKind::COMPONENT: return erase_k<EntityKind::COMPONENT>(handle, desc);
            case EntityKind::NET: return erase_k<EntityKind::NET>(handle, desc);
            case EntityKind::LAYER: return erase_k<EntityKind::LAYER>(handle, desc);
            case EntityKind::LAYERSTACK: return erase_k<EntityKind::LAYERSTACK>(handle, desc);
            case EntityKind::SURFACE: return erase_k<EntityKind::SURFACE>(handle, desc);
            case EntityKind::BONDWIRE: return erase_k<EntityKind::BONDWIRE>(handle, desc);
            case EntityKind::PORT: return erase_k<EntityKind::PORT>(handle, desc);
            case EntityKind::SYMBOL: return erase_k<EntityKind::SYMBOL>(handle, desc);
            case EntityKind::BOARD: return erase_k<EntityKind::BOARD>(handle, desc);
        }
        return false;
    }

    template <EntityKind K>
    ObjectId insert_k(const EntityT<K>& obj, const std::string& desc = "insert") {
        ObjectId handle = container<K>().add(obj);
        on_insert<K>(handle, obj);

        LayerOp<K> op{};
        op.op = OpType::INSERT;
        op.handles.push_back(handle);
        op.objects.push_back(obj);
        record_op<K>(desc, std::move(op));
        return handle;
    }

    template <EntityKind K>
    bool erase_k(ObjectId handle, const std::string& desc = "erase") {
        auto& c = container<K>();
        const EntityT<K>* cur = c.get(handle);
        if (!cur) return false;

        EntityT<K> obj = *cur;
        on_erase<K>(handle, obj);
        c.remove(handle);

        LayerOp<K> op{};
        op.op = OpType::ERASE;
        op.handles.push_back(handle);
        op.objects.push_back(std::move(obj));
        record_op<K>(desc, std::move(op));
        return true;
    }

    template <EntityKind K>
    bool replace_k(ObjectId handle, const EntityT<K>& next, const std::string& desc = "replace") {
        auto& c = container<K>();
        const EntityT<K>* cur = c.get(handle);
        if (!cur) return false;

        EntityT<K> old_obj = *cur;

        // apply current state directly
        on_erase<K>(handle, old_obj);
        c.replace(handle, next);
        on_insert<K>(handle, next);

        // log as erase(old) + insert(new), no before/after record in one op
        LayerOp<K> erase_op{};
        erase_op.op = OpType::ERASE;
        erase_op.handles.push_back(handle);
        erase_op.objects.push_back(std::move(old_obj));

        LayerOp<K> insert_op{};
        insert_op.op = OpType::INSERT;
        insert_op.handles.push_back(handle);
        insert_op.objects.push_back(next);

        if (m_replaying) return true;
        if (m_in_tx) {
            append_op_to_tx<K>(m_open_tx, std::move(erase_op));
            append_op_to_tx<K>(m_open_tx, std::move(insert_op));
            return true;
        }

        Transaction tx{};
        tx.desc = desc;
        append_op_to_tx<K>(tx, std::move(erase_op));
        append_op_to_tx<K>(tx, std::move(insert_op));
        m_undo.push_back(std::move(tx));
        if (m_undo.size() > MAX_UNDO) m_undo.erase(m_undo.begin());
        m_redo.clear();
        return true;
    }

    bool undo() {
        if (m_undo.empty()) return false;
        Transaction tx = std::move(m_undo.back());
        m_undo.pop_back();

        m_replaying = true;
        for (auto it = tx.order.rbegin(); it != tx.order.rend(); ++it) {
            apply_inverse_ordered(tx, *it);
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
        for (const auto& ord : tx.order) {
            apply_forward_ordered(tx, ord);
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
            if (!prefix.empty() && s.substr(0, prefix.size()) == prefix) out.push_back(id);
        }
        return out;
    }

    void init_quadtree(const BBox& bounds) { quadtree = std::make_unique<QuadTree>(bounds, 0); }

    void clear() {
        padstacks.clear(); traces.clear(); vias.clear(); components.clear(); nets.clear(); layers.clear();
        layerstacks.clear(); surfaces.clear(); bondwires.clear(); ports.clear(); symbols.clear(); boards.clear();
        m_name_by_object.clear(); m_undo.clear(); m_redo.clear(); m_open_tx = Transaction{}; m_in_tx = false;
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
    std::vector<LayerOp<K>>& tx_ops(Transaction& tx) {
        if constexpr (K == EntityKind::PADSTACK) return tx.padstack_ops;
        else if constexpr (K == EntityKind::TRACE) return tx.trace_ops;
        else if constexpr (K == EntityKind::VIA) return tx.via_ops;
        else if constexpr (K == EntityKind::COMPONENT) return tx.component_ops;
        else if constexpr (K == EntityKind::NET) return tx.net_ops;
        else if constexpr (K == EntityKind::LAYER) return tx.layer_ops;
        else if constexpr (K == EntityKind::LAYERSTACK) return tx.layerstack_ops;
        else if constexpr (K == EntityKind::SURFACE) return tx.surface_ops;
        else if constexpr (K == EntityKind::BONDWIRE) return tx.bondwire_ops;
        else if constexpr (K == EntityKind::PORT) return tx.port_ops;
        else if constexpr (K == EntityKind::SYMBOL) return tx.symbol_ops;
        else return tx.board_ops;
    }

    template <EntityKind K>
    const std::vector<LayerOp<K>>& tx_ops_const(const Transaction& tx) const {
        if constexpr (K == EntityKind::PADSTACK) return tx.padstack_ops;
        else if constexpr (K == EntityKind::TRACE) return tx.trace_ops;
        else if constexpr (K == EntityKind::VIA) return tx.via_ops;
        else if constexpr (K == EntityKind::COMPONENT) return tx.component_ops;
        else if constexpr (K == EntityKind::NET) return tx.net_ops;
        else if constexpr (K == EntityKind::LAYER) return tx.layer_ops;
        else if constexpr (K == EntityKind::LAYERSTACK) return tx.layerstack_ops;
        else if constexpr (K == EntityKind::SURFACE) return tx.surface_ops;
        else if constexpr (K == EntityKind::BONDWIRE) return tx.bondwire_ops;
        else if constexpr (K == EntityKind::PORT) return tx.port_ops;
        else if constexpr (K == EntityKind::SYMBOL) return tx.symbol_ops;
        else return tx.board_ops;
    }

    template <EntityKind K>
    void append_op_to_tx(Transaction& tx, LayerOp<K> op) {
        auto& vec = tx_ops<K>(tx);
        vec.push_back(std::move(op));
        tx.order.push_back({K, static_cast<uint32_t>(vec.size() - 1)});
    }

    template <EntityKind K>
    void record_op(const std::string& desc, LayerOp<K> op) {
        if (m_replaying) return;
        if (m_in_tx) {
            append_op_to_tx<K>(m_open_tx, std::move(op));
            return;
        }

        Transaction tx{};
        tx.desc = desc;
        append_op_to_tx<K>(tx, std::move(op));
        m_undo.push_back(std::move(tx));
        if (m_undo.size() > MAX_UNDO) m_undo.erase(m_undo.begin());
        m_redo.clear();
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

    template <EntityKind K>
    void apply_forward_t(const LayerOp<K>& op) {
        auto& c = container<K>();
        if (op.op == OpType::INSERT) {
            for (size_t i = 0; i < op.handles.size(); ++i) {
                c.restore(op.handles[i], op.objects[i]);
                on_insert<K>(op.handles[i], op.objects[i]);
            }
        } else {
            for (size_t i = 0; i < op.handles.size(); ++i) {
                if (const auto* cur = c.get(op.handles[i])) on_erase<K>(op.handles[i], *cur);
                c.remove(op.handles[i]);
            }
        }
    }

    template <EntityKind K>
    void apply_inverse_t(const LayerOp<K>& op) {
        auto& c = container<K>();
        if (op.op == OpType::INSERT) {
            for (size_t i = 0; i < op.handles.size(); ++i) {
                if (const auto* cur = c.get(op.handles[i])) on_erase<K>(op.handles[i], *cur);
                c.remove(op.handles[i]);
            }
        } else {
            for (size_t i = 0; i < op.handles.size(); ++i) {
                c.restore(op.handles[i], op.objects[i]);
                on_insert<K>(op.handles[i], op.objects[i]);
            }
        }
    }

    template <size_t... Is>
    void apply_forward_dispatch(const Transaction& tx, const OpOrder& ord, std::index_sequence<Is...>) {
        (void)((ord.kind == static_cast<EntityKind>(Is)
            ? (apply_forward_t(tx_ops_const<static_cast<EntityKind>(Is)>(tx)[ord.index]), true)
            : false) || ...);
    }

    template <size_t... Is>
    void apply_inverse_dispatch(const Transaction& tx, const OpOrder& ord, std::index_sequence<Is...>) {
        (void)((ord.kind == static_cast<EntityKind>(Is)
            ? (apply_inverse_t(tx_ops_const<static_cast<EntityKind>(Is)>(tx)[ord.index]), true)
            : false) || ...);
    }

    void apply_forward_ordered(const Transaction& tx, const OpOrder& ord) {
        apply_forward_dispatch(tx, ord, std::make_index_sequence<12>{});
    }

    void apply_inverse_ordered(const Transaction& tx, const OpOrder& ord) {
        apply_inverse_dispatch(tx, ord, std::make_index_sequence<12>{});
    }

    static constexpr std::size_t MAX_UNDO = 256;

    bool m_replaying = false;
    bool m_in_tx = false;
    Transaction m_open_tx{};
    std::vector<Transaction> m_undo;
    std::vector<Transaction> m_redo;
    std::unordered_map<ObjectId, StringId> m_name_by_object;
};

template <> struct PCBDatabase::KindOf<PadstackDef> { static constexpr EntityKind value = EntityKind::PADSTACK; };
template <> struct PCBDatabase::KindOf<Trace> { static constexpr EntityKind value = EntityKind::TRACE; };
template <> struct PCBDatabase::KindOf<Via> { static constexpr EntityKind value = EntityKind::VIA; };
template <> struct PCBDatabase::KindOf<Component> { static constexpr EntityKind value = EntityKind::COMPONENT; };
template <> struct PCBDatabase::KindOf<Net> { static constexpr EntityKind value = EntityKind::NET; };
template <> struct PCBDatabase::KindOf<Layer> { static constexpr EntityKind value = EntityKind::LAYER; };
template <> struct PCBDatabase::KindOf<LayerStack> { static constexpr EntityKind value = EntityKind::LAYERSTACK; };
template <> struct PCBDatabase::KindOf<Surface> { static constexpr EntityKind value = EntityKind::SURFACE; };
template <> struct PCBDatabase::KindOf<BondWire> { static constexpr EntityKind value = EntityKind::BONDWIRE; };
template <> struct PCBDatabase::KindOf<Port> { static constexpr EntityKind value = EntityKind::PORT; };
template <> struct PCBDatabase::KindOf<Symbol> { static constexpr EntityKind value = EntityKind::SYMBOL; };
template <> struct PCBDatabase::KindOf<Board> { static constexpr EntityKind value = EntityKind::BOARD; };
