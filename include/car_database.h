/**
 * @file car_database.h
 * @brief Main PCB database with lightweight containers and templated undo/redo ops
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

    // ===== KLayout-style insert/replace/erase wrappers (templated core) =====
    ObjectId add_trace(LayerId layer, const Trace& trace) {
        return insert_entity(
            traces,
            trace,
            "insert_trace",
            [this, layer](ObjectId id, const Trace& t) {
                layer_index.add(layer, id, LayerIndex::Kind::TRACE);
                index_name_if_exists(id, t);
            },
            [this, layer](ObjectId id, const Trace& t) {
                layer_index.remove(layer, id, LayerIndex::Kind::TRACE);
                unindex_name_if_exists(id, t);
            }
        );
    }

    bool replace_trace(ObjectId handle, const Trace& next) {
        return replace_entity(
            traces,
            handle,
            next,
            "replace_trace",
            [this](ObjectId id, const Trace& t) { index_name_if_exists(id, t); },
            [this](ObjectId id, const Trace& t) { unindex_name_if_exists(id, t); }
        );
    }

    bool remove_trace(ObjectId handle) {
        return erase_entity(
            traces,
            handle,
            "erase_trace",
            [this](ObjectId id, const Trace& t) {
                layer_index.add(t.layer_id, id, LayerIndex::Kind::TRACE);
                index_name_if_exists(id, t);
            },
            [this](ObjectId id, const Trace& t) {
                layer_index.remove(t.layer_id, id, LayerIndex::Kind::TRACE);
                unindex_name_if_exists(id, t);
            }
        );
    }

    // extra example: via also uses the same template path
    ObjectId add_via(const Via& via) {
        return insert_entity(
            vias,
            via,
            "insert_via",
            [this](ObjectId id, const Via& v) {
                layer_index.add(v.start_layer, id, LayerIndex::Kind::VIA);
                index_name_if_exists(id, v);
            },
            [this](ObjectId id, const Via& v) {
                layer_index.remove(v.start_layer, id, LayerIndex::Kind::VIA);
                unindex_name_if_exists(id, v);
            }
        );
    }

    bool replace_via(ObjectId handle, const Via& next) {
        return replace_entity(
            vias,
            handle,
            next,
            "replace_via",
            [this](ObjectId id, const Via& v) {
                layer_index.add(v.start_layer, id, LayerIndex::Kind::VIA);
                index_name_if_exists(id, v);
            },
            [this](ObjectId id, const Via& v) {
                layer_index.remove(v.start_layer, id, LayerIndex::Kind::VIA);
                unindex_name_if_exists(id, v);
            }
        );
    }

    bool remove_via(ObjectId handle) {
        return erase_entity(
            vias,
            handle,
            "erase_via",
            [this](ObjectId id, const Via& v) {
                layer_index.add(v.start_layer, id, LayerIndex::Kind::VIA);
                index_name_if_exists(id, v);
            },
            [this](ObjectId id, const Via& v) {
                layer_index.remove(v.start_layer, id, LayerIndex::Kind::VIA);
                unindex_name_if_exists(id, v);
            }
        );
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

    template <typename T, typename = void>
    struct has_name_id : std::false_type {};

    template <typename T>
    struct has_name_id<T, std::void_t<decltype(std::declval<T>().name_id)>> : std::true_type {};

    template <typename T>
    void index_name_if_exists(ObjectId id, const T& obj) {
        if constexpr (has_name_id<T>::value) {
            if (obj.name_id != 0) {
                m_name_by_object[id] = obj.name_id;
            }
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

    template <typename T, typename OnApply, typename OnRevert>
    ObjectId insert_entity(ReuseVector<T>& container, const T& obj, const std::string& desc, OnApply on_apply, OnRevert on_revert) {
        ObjectId handle = container.add(obj);
        on_apply(handle, obj);

        transactions.begin(desc);
        transactions.record({
            [this, &container, handle, obj, on_revert]() {
                if (const auto* cur = container.get(handle)) {
                    on_revert(handle, *cur);
                } else {
                    on_revert(handle, obj);
                }
                container.remove(handle);
            },
            [this, &container, handle, obj, on_apply]() {
                container.restore(handle, obj);
                on_apply(handle, obj);
            },
        });
        transactions.commit();
        return handle;
    }

    template <typename T, typename OnApply, typename OnRevert>
    bool replace_entity(ReuseVector<T>& container, ObjectId handle, const T& next, const std::string& desc, OnApply on_apply, OnRevert on_revert) {
        const T* cur = container.get(handle);
        if (!cur) {
            return false;
        }

        const T prev = *cur;
        on_revert(handle, prev);
        container.replace(handle, next);
        on_apply(handle, next);

        transactions.begin(desc);
        transactions.record({
            [this, &container, handle, prev, next, on_apply, on_revert]() {
                on_revert(handle, next);
                container.replace(handle, prev);
                on_apply(handle, prev);
            },
            [this, &container, handle, prev, next, on_apply, on_revert]() {
                on_revert(handle, prev);
                container.replace(handle, next);
                on_apply(handle, next);
            },
        });
        transactions.commit();
        return true;
    }

    template <typename T, typename OnRestore, typename OnErase>
    bool erase_entity(ReuseVector<T>& container, ObjectId handle, const std::string& desc, OnRestore on_restore, OnErase on_erase) {
        const T* cur = container.get(handle);
        if (!cur) {
            return false;
        }

        const T snapshot = *cur;
        on_erase(handle, snapshot);
        container.remove(handle);

        transactions.begin(desc);
        transactions.record({
            [this, &container, handle, snapshot, on_restore]() {
                container.restore(handle, snapshot);
                on_restore(handle, snapshot);
            },
            [this, &container, handle, snapshot, on_erase]() {
                on_erase(handle, snapshot);
                container.remove(handle);
            },
        });
        transactions.commit();
        return true;
    }

    std::unordered_map<ObjectId, StringId> m_name_by_object;
};
