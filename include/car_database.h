/**
 * @file car_database.h
 * @brief Main PCB database with lightweight templated insert/replace/erase and log-based undo/redo
 */

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
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

    using Snapshot = std::variant<std::monostate, PadstackDef, Trace, Via, Component, Net, Layer, LayerStack, Surface, BondWire, Port, Symbol, Board>;

    struct Change {
        OpType op = OpType::INSERT;
        EntityKind kind = EntityKind::TRACE;
        ObjectId handle = 0;
        Snapshot before{};
        Snapshot after{};
    };

    struct Transaction {
        std::string desc;
        std::vector<Change> changes;
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
        push_tx(desc, Change{OpType::INSERT, K, handle, Snapshot{}, Snapshot{obj}});
        return handle;
    }

    template <EntityKind K>
    bool replace(ObjectId handle, const EntityT<K>& next, const std::string& desc = "replace") {
        auto& c = container<K>();
        const EntityT<K>* cur = c.get(handle);
        if (!cur) return false;

        EntityT<K> prev = *cur;
        on_erase<K>(handle, prev);
        c.replace(handle, next);
        on_insert<K>(handle, next);

        push_tx(desc, Change{OpType::REPLACE, K, handle, Snapshot{prev}, Snapshot{next}});
        return true;
    }

    template <EntityKind K>
    bool erase(ObjectId handle, const std::string& desc = "erase") {
        auto& c = container<K>();
        const EntityT<K>* cur = c.get(handle);
        if (!cur) return false;

        EntityT<K> snap = *cur;
        on_erase<K>(handle, snap);
        c.remove(handle);

        push_tx(desc, Change{OpType::ERASE, K, handle, Snapshot{snap}, Snapshot{}});
        return true;
    }

    bool undo() {
        if (m_undo.empty()) return false;
        Transaction tx = std::move(m_undo.back());
        m_undo.pop_back();

        m_replaying = true;
        for (auto it = tx.changes.rbegin(); it != tx.changes.rend(); ++it) apply_inverse(*it);
        m_replaying = false;

        m_redo.push_back(std::move(tx));
        return true;
    }

    bool redo() {
        if (m_redo.empty()) return false;
        Transaction tx = std::move(m_redo.back());
        m_redo.pop_back();

        m_replaying = true;
        for (const auto& c : tx.changes) apply_forward(c);
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
        if constexpr (has_name_id<T>::value) if (obj.name_id != 0) m_name_by_object[id] = obj.name_id;
    }
    template <typename T>
    void unindex_name_if_exists(ObjectId id, const T& obj) {
        if constexpr (has_name_id<T>::value) {
            auto it = m_name_by_object.find(id);
            if (it != m_name_by_object.end() && (obj.name_id == 0 || it->second == obj.name_id)) m_name_by_object.erase(it);
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

    void push_tx(const std::string& desc, Change c) {
        if (m_replaying) return;
        Transaction tx;
        tx.desc = desc;
        tx.changes.push_back(std::move(c));
        m_undo.push_back(std::move(tx));
        if (m_undo.size() > MAX_UNDO) m_undo.erase(m_undo.begin());
        m_redo.clear();
    }

    template <EntityKind K>
    static const EntityT<K>& as_snapshot(const Snapshot& s) { return std::get<EntityT<K>>(s); }

    template <EntityKind K>
    void apply_forward_t(const Change& c) {
        if (c.op == OpType::INSERT) {
            auto& obj = as_snapshot<K>(c.after);
            container<K>().restore(c.handle, obj);
            on_insert<K>(c.handle, obj);
        } else if (c.op == OpType::ERASE) {
            if (const auto* cur = container<K>().get(c.handle)) on_erase<K>(c.handle, *cur);
            container<K>().remove(c.handle);
        } else {
            auto& obj = as_snapshot<K>(c.after);
            if (const auto* cur = container<K>().get(c.handle)) on_erase<K>(c.handle, *cur);
            container<K>().replace(c.handle, obj);
            on_insert<K>(c.handle, obj);
        }
    }

    template <EntityKind K>
    void apply_inverse_t(const Change& c) {
        if (c.op == OpType::INSERT) {
            if (const auto* cur = container<K>().get(c.handle)) on_erase<K>(c.handle, *cur);
            container<K>().remove(c.handle);
        } else if (c.op == OpType::ERASE) {
            auto& obj = as_snapshot<K>(c.before);
            container<K>().restore(c.handle, obj);
            on_insert<K>(c.handle, obj);
        } else {
            auto& obj = as_snapshot<K>(c.before);
            if (const auto* cur = container<K>().get(c.handle)) on_erase<K>(c.handle, *cur);
            container<K>().replace(c.handle, obj);
            on_insert<K>(c.handle, obj);
        }
    }

    void apply_forward(const Change& c) {
        switch (c.kind) {
            case EntityKind::PADSTACK: apply_forward_t<EntityKind::PADSTACK>(c); break;
            case EntityKind::TRACE: apply_forward_t<EntityKind::TRACE>(c); break;
            case EntityKind::VIA: apply_forward_t<EntityKind::VIA>(c); break;
            case EntityKind::COMPONENT: apply_forward_t<EntityKind::COMPONENT>(c); break;
            case EntityKind::NET: apply_forward_t<EntityKind::NET>(c); break;
            case EntityKind::LAYER: apply_forward_t<EntityKind::LAYER>(c); break;
            case EntityKind::LAYERSTACK: apply_forward_t<EntityKind::LAYERSTACK>(c); break;
            case EntityKind::SURFACE: apply_forward_t<EntityKind::SURFACE>(c); break;
            case EntityKind::BONDWIRE: apply_forward_t<EntityKind::BONDWIRE>(c); break;
            case EntityKind::PORT: apply_forward_t<EntityKind::PORT>(c); break;
            case EntityKind::SYMBOL: apply_forward_t<EntityKind::SYMBOL>(c); break;
            case EntityKind::BOARD: apply_forward_t<EntityKind::BOARD>(c); break;
        }
    }

    void apply_inverse(const Change& c) {
        switch (c.kind) {
            case EntityKind::PADSTACK: apply_inverse_t<EntityKind::PADSTACK>(c); break;
            case EntityKind::TRACE: apply_inverse_t<EntityKind::TRACE>(c); break;
            case EntityKind::VIA: apply_inverse_t<EntityKind::VIA>(c); break;
            case EntityKind::COMPONENT: apply_inverse_t<EntityKind::COMPONENT>(c); break;
            case EntityKind::NET: apply_inverse_t<EntityKind::NET>(c); break;
            case EntityKind::LAYER: apply_inverse_t<EntityKind::LAYER>(c); break;
            case EntityKind::LAYERSTACK: apply_inverse_t<EntityKind::LAYERSTACK>(c); break;
            case EntityKind::SURFACE: apply_inverse_t<EntityKind::SURFACE>(c); break;
            case EntityKind::BONDWIRE: apply_inverse_t<EntityKind::BONDWIRE>(c); break;
            case EntityKind::PORT: apply_inverse_t<EntityKind::PORT>(c); break;
            case EntityKind::SYMBOL: apply_inverse_t<EntityKind::SYMBOL>(c); break;
            case EntityKind::BOARD: apply_inverse_t<EntityKind::BOARD>(c); break;
        }
    }

    static constexpr std::size_t MAX_UNDO = 256;

    bool m_replaying = false;
    std::vector<Transaction> m_undo;
    std::vector<Transaction> m_redo;
    std::unordered_map<ObjectId, StringId> m_name_by_object;
};
