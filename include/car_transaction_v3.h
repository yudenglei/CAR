/**
 * @file car_transaction_v3.h
 * @brief KLayout-style Transaction System v3.1
 * @version 3.1 - Batch operations + Auto-container
 * 
 * Key improvements from v3.0:
 * 1. Change uses vector for batch operations (like KLayout's layer_op)
 * 2. add/erase/replace auto-get container internally (like KLayout get_layer)
 * 3. Simplified API - no explicit get_container call needed
 */

#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cstring>
#include <unordered_map>
#include <algorithm>

namespace car {

// ============================================================================
// Operation Types (KLayout style)
using OpType = uint8_t;
constexpr OpType OpInsert = 0;
constexpr OpType OpErase  = 1;
constexpr OpType OpUpdate = 2;

// ============================================================================
// Change - KLayout layer_op style with vector support
// 
// KLayout's layer_op stores: type, index, old/new data
// Our Change now supports BATCH operations via vector
// ============================================================================

/**
 * @brief Single change entry - like KLayout's individual layer_op
 */
struct ChangeEntry {
    uint32_t slot_idx;     // Slot index in storage
    uint32_t old_gen;      // Generation before operation
    uint32_t new_gen;      // Generation after operation
    
    ChangeEntry() = default;
    ChangeEntry(uint32_t idx, uint32_t old_g, uint32_t new_g)
        : slot_idx(idx), old_gen(old_g), new_gen(new_g) {}
};

/**
 * @brief Change - Batch operations like KLayout's layer_op
 * 
 * KLayout's dbshapes uses vector of operations for batch undo/redo
 * We follow the same pattern - one Change can contain multiple operations
 */
struct Change {
    OpType   op;            // Insert/Erase/Update
    uint16_t type_id;       // Object type (Trace, Via, etc.)
    std::vector<ChangeEntry> entries;  // Vector of changes - like KLayout's batch ops
    
    // Backup data for undo/redo - aligned with entries vector
    std::vector<std::vector<uint8_t>> backups;
    
    Change() = default;
    
    Change(OpType o, uint16_t tid) : op(o), type_id(tid) {}
    
    // Add single entry
    void add_entry(ChangeEntry&& e, std::vector<uint8_t>&& backup = {}) {
        entries.push_back(std::move(e));
        if (!backup.empty()) {
            backups.push_back(std::move(backup));
        }
    }
    
    size_t size() const { return entries.size(); }
    bool empty() const { return entries.empty(); }
    
    // Access entry and backup together
    struct EntryWithBackup {
        const ChangeEntry& entry;
        const std::vector<uint8_t>* backup;
    };
    
    EntryWithBackup get(size_t i) const {
        return {entries[i], i < backups.size() ? &backups[i] : nullptr};
    }
};

// ============================================================================
// Container Interface
// ============================================================================

class IContainer {
public:
    virtual ~IContainer() = default;
    virtual bool remove(ObjectId h) = 0;
    virtual void restore(uint32_t idx, const void* data, uint32_t gen) = 0;
    virtual uint16_t type_id() const = 0;
};

template<typename T, uint16_t TypeId>
class TContainer : public IContainer {
public:
    using Storage = ReuseVector<T>;
    
    explicit TContainer(Storage& s) : storage_(s) {}
    
    bool remove(ObjectId h) override { 
        auto* obj = storage_.get(h);
        if (!obj) return false;
        storage_.remove(h);
        return true;
    }
    
    void restore(uint32_t idx, const void* data, uint32_t gen) override {
        storage_.restore(idx, *static_cast<const T*>(data), gen);
    }
    
    uint16_t type_id() const override { return TypeId; }
    
    Storage& storage() { return storage_; }
    
private:
    Storage& storage_;
};

// ============================================================================
// Type Trait - Map C++ types to ObjectTypeId automatically
// ============================================================================

/**
 * @brief Type trait to map C++ types to ObjectTypeId
 * 
 * Like KLayout's internal type mapping, but simpler.
 * Specialize for each type:
 *   template<> struct TypeTrait<MyClass> { static constexpr uint16_t id = ObjectTypeId::MyClass; };
 */
template<typename T>
struct TypeTrait {
    static constexpr uint16_t id = 0xFFFF;  // Must specialize!
};

// ============================================================================
// Transaction
// ============================================================================

class Transaction {
public:
    explicit Transaction(std::string desc = {}) : desc_(std::move(desc)) {}
    
    void add(Change&& c) { changes_.push_back(std::move(c)); }
    const std::string& desc() const { return desc_; }
    std::vector<Change>& changes() { return changes_; }
    const std::vector<Change>& changes() const { return changes_; }
    bool empty() const { return changes_.empty(); }

private:
    std::string desc_;
    std::vector<Change> changes_;
};

// ============================================================================
// TransactionManager - KLayout style v3.1
// 
// Key improvements:
// 1. Auto-get container inside add/erase/replace (like KLayout get_layer)
// 2. Batch operations support
// ============================================================================

class TransactionManager {
public:
    TransactionManager() = default;
    
    // =========================================================================
    // Internal: Get or create container (like KLayout get_layer)
    // =========================================================================
    
    /**
     * @brief Get container - Like KLayout's get_layer()
     * 
     * Used internally by add/erase/replace.
     * Can also be called explicitly if needed.
     */
    template<typename T, uint16_t TypeId>
    TContainer<T, TypeId>& get_container(ReuseVector<T>& storage) {
        uint16_t id = TypeId;
        auto it = containers_.find(id);
        if (it != containers_.end()) {
            return *static_cast<TContainer<T, TypeId>*>(it->second.get());
        }
        
        auto* c = new TContainer<T, TypeId>(storage);
        containers_[id] = std::unique_ptr<IContainer>(c);
        return *c;
    }
    
    // =========================================================================
    // NEW: Auto-container add - No need to call get_container manually!
    // 
    // Usage:
    //   ObjectId id = tx.add(traces, trace_obj);
    //   // OR with type trait (auto-resolve):
    //   ObjectId id = tx.add(traces, trace_obj);  // uses TypeTrait<T>::id
    // =========================================================================
    
    /**
     * @brief Add object - Auto-gets container internally
     * 
     * Usage:
     *   ObjectId id = tx.add(traces, trace_obj);
     * 
     * Internally calls get_container<Trace, TypeTrait<Trace>::id>(traces)
     * No explicit container registration needed!
     */
    template<typename T>
    ObjectId add(ReuseVector<T>& storage, const T& obj) {
        constexpr uint16_t TypeId = TypeTrait<T>::id;
        return add_with_id<T, TypeId>(storage, obj);
    }
    
    /**
     * @brief Add with explicit type ID (backward compatibility)
     */
    template<typename T, uint16_t TypeId>
    ObjectId add(ReuseVector<T>& storage, const T& obj) {
        return add_with_id<T, TypeId>(storage, obj);
    }
    
    // =========================================================================
    // NEW: Batch add - Add multiple objects in one transaction
    // 
    // Usage:
    //   std::vector<Trace> traces = {...};
    //   std::vector<ObjectId> ids = tx.add(traces_storage, traces);
    // =========================================================================
    
    /**
     * @brief Add multiple objects - Batch operation
     * 
     * Creates ONE transaction for all objects (like KLayout batch ops)
     * 
     * Usage:
     *   std::vector<Via> vias = {via1, via2, via3, ...};
     *   auto ids = tx.add(vias_storage, vias);
     */
    template<typename T>
    std::vector<ObjectId> add(ReuseVector<T>& storage, const std::vector<T>& objects) {
        constexpr uint16_t TypeId = TypeTrait<T>::id;
        return add_batch_with_id<T, TypeId>(storage, objects);
    }
    
    template<typename T, uint16_t TypeId>
    std::vector<ObjectId> add(ReuseVector<T>& storage, const std::vector<T>& objects) {
        return add_batch_with_id<T, TypeId>(storage, objects);
    }
    
    // =========================================================================
    // Erase - Auto-container
    // =========================================================================
    
    /**
     * @brief Erase object - Auto-gets container internally
     * 
     * Usage:
     *   bool ok = tx.erase(traces, handle);
     */
    template<typename T>
    bool erase(ReuseVector<T>& storage, ObjectId handle) {
        constexpr uint16_t TypeId = TypeTrait<T>::id;
        return erase_with_id<T, TypeId>(storage, handle);
    }
    
    template<typename T, uint16_t TypeId>
    bool erase(ReuseVector<T>& storage, ObjectId handle) {
        return erase_with_id<T, TypeId>(storage, handle);
    }
    
    /**
     * @brief Erase multiple objects - Batch operation
     * 
     * Usage:
     *   std::vector<ObjectId> handles = {h1, h2, h3, ...};
     *   tx.erase(traces_storage, handles);
     */
    template<typename T>
    size_t erase(ReuseVector<T>& storage, const std::vector<ObjectId>& handles) {
        constexpr uint16_t TypeId = TypeTrait<T>::id;
        return erase_batch_with_id<T, TypeId>(storage, handles);
    }
    
    template<typename T, uint16_t TypeId>
    size_t erase(ReuseVector<T>& storage, const std::vector<ObjectId>& handles) {
        return erase_batch_with_id<T, TypeId>(storage, handles);
    }
    
    // =========================================================================
    // Replace - Auto-container
    // =========================================================================
    
    /**
     * @brief Replace object - Auto-gets container internally
     * 
     * Usage:
     *   bool ok = tx.replace(traces, handle, new_trace);
     */
    template<typename T>
    bool replace(ReuseVector<T>& storage, ObjectId handle, const T& new_obj) {
        constexpr uint16_t TypeId = TypeTrait<T>::id;
        return replace_with_id<T, TypeId>(storage, handle, new_obj);
    }
    
    template<typename T, uint16_t TypeId>
    bool replace(ReuseVector<T>& storage, ObjectId handle, const T& new_obj) {
        return replace_with_id<T, TypeId>(storage, handle, new_obj);
    }
    
    // =========================================================================
    // Modify - Auto-container
    // =========================================================================
    
    /**
     * @brief Modify object in place - Auto-gets container internally
     * 
     * Usage:
     *   tx.modify(traces, handle, [](Trace& t) {
     *       t.width = 50;
     *   });
     */
    template<typename T, typename Modifier>
    bool modify(ReuseVector<T>& storage, ObjectId handle, Modifier&& mod) {
        constexpr uint16_t TypeId = TypeTrait<T>::id;
        return modify_with_id<T, TypeId>(storage, handle, std::forward<Modifier>(mod));
    }
    
    template<typename T, uint16_t TypeId, typename Modifier>
    bool modify(ReuseVector<T>& storage, ObjectId handle, Modifier&& mod) {
        return modify_with_id<T, TypeId>(storage, handle, std::forward<Modifier>(mod));
    }
    
    // =========================================================================
    // Transaction control
    // =========================================================================
    
    void begin(const std::string& desc = {}) {
        current_ = Transaction(desc);
        in_tx_ = true;
    }
    
    void commit() {
        if (!in_tx_ || current_.empty()) {
            in_tx_ = false;
            current_ = Transaction();
            return;
        }
        
        undo_stack_.push_back(std::move(current_));
        redo_stack_.clear();
        
        if (undo_stack_.size() > max_undo_) {
            undo_stack_.erase(undo_stack_.begin());
        }
        
        in_tx_ = false;
        current_ = Transaction();
    }
    
    void rollback() {
        in_tx_ = false;
        current_ = Transaction();
    }
    
    void record(Change&& c) {
        if (!in_tx_) return;
        current_.add(std::move(c));
    }
    
    // =========================================================================
    // Undo/Redo
    // =========================================================================
    
    bool undo() {
        if (undo_stack_.empty()) return false;
        
        Transaction& tx = undo_stack_.back();
        
        // Execute undo for each change
        for (auto& change : tx.changes()) {
            apply_undo(change);
        }
        
        redo_stack_.push_back(std::move(tx));
        undo_stack_.pop_back();
        
        return true;
    }
    
    bool redo() {
        if (redo_stack_.empty()) return false;
        
        Transaction& tx = redo_stack_.back();
        
        // Execute redo for each change
        for (auto& change : tx.changes()) {
            apply_redo(change);
        }
        
        undo_stack_.push_back(std::move(tx));
        redo_stack_.pop_back();
        
        return true;
    }
    
    size_t undo_stack_size() const { return undo_stack_.size(); }
    size_t redo_stack_size() const { return redo_stack_.size(); }
    bool can_undo() const { return !undo_stack_.empty(); }
    bool can_redo() const { return !redo_stack_.empty(); }
    
    void clear() {
        undo_stack_.clear();
        redo_stack_.clear();
        current_ = Transaction();
        in_tx_ = false;
    }
    
    void set_max_undo(size_t max_undo) { max_undo_ = max_undo; }
    size_t max_undo() const { return max_undo_; }

private:
    // =========================================================================
    // Internal implementations
    // =========================================================================
    
    template<typename T, uint16_t TypeId>
    ObjectId add_with_id(ReuseVector<T>& storage, const T& obj) {
        // Auto-get container like KLayout get_layer()
        get_container<T, TypeId>(storage);
        
        begin("Add");
        
        ObjectId handle = storage.add(obj);
        auto [idx, gen] = storage.get_slot_info(handle);
        
        Change c(OpInsert, TypeId);
        c.add_entry(ChangeEntry(idx, 0, gen));
        record(std::move(c));
        
        commit();
        return handle;
    }
    
    template<typename T, uint16_t TypeId>
    std::vector<ObjectId> add_batch_with_id(ReuseVector<T>& storage, const std::vector<T>& objects) {
        if (objects.empty()) return {};
        
        // Auto-get container
        get_container<T, TypeId>(storage);
        
        begin("Add batch");
        
        std::vector<ObjectId> handles;
        handles.reserve(objects.size());
        
        Change c(OpInsert, TypeId);
        
        for (const auto& obj : objects) {
            ObjectId handle = storage.add(obj);
            auto [idx, gen] = storage.get_slot_info(handle);
            handles.push_back(handle);
            c.add_entry(ChangeEntry(idx, 0, gen));
        }
        
        record(std::move(c));
        commit();
        
        return handles;
    }
    
    template<typename T, uint16_t TypeId>
    bool erase_with_id(ReuseVector<T>& storage, ObjectId handle) {
        T* obj = storage.get(handle);
        if (!obj) return false;
        
        // Auto-get container
        get_container<T, TypeId>(storage);
        
        begin("Erase");
        
        auto [idx, old_gen] = storage.get_slot_info(handle);
        
        // Backup data for undo
        std::vector<uint8_t> backup(sizeof(T));
        std::memcpy(backup.data(), obj, sizeof(T));
        
        Change c(OpErase, TypeId);
        c.add_entry(ChangeEntry(idx, old_gen, old_gen), std::move(backup));
        record(std::move(c));
        
        storage.remove(handle);
        commit();
        
        return true;
    }
    
    template<typename T, uint16_t TypeId>
    size_t erase_batch_with_id(ReuseVector<T>& storage, const std::vector<ObjectId>& handles) {
        if (handles.empty()) return 0;
        
        // Auto-get container
        get_container<T, TypeId>(storage);
        
        begin("Erase batch");
        
        Change c(OpErase, TypeId);
        
        for (ObjectId handle : handles) {
            T* obj = storage.get(handle);
            if (!obj) continue;
            
            auto [idx, old_gen] = storage.get_slot_info(handle);
            
            std::vector<uint8_t> backup(sizeof(T));
            std::memcpy(backup.data(), obj, sizeof(T));
            
            c.add_entry(ChangeEntry(idx, old_gen, old_gen), std::move(backup));
            storage.remove(handle);
        }
        
        record(std::move(c));
        commit();
        
        return c.size();
    }
    
    template<typename T, uint16_t TypeId>
    bool replace_with_id(ReuseVector<T>& storage, ObjectId handle, const T& new_obj) {
        T* old_obj = storage.get(handle);
        if (!old_obj) return false;
        
        // Auto-get container
        get_container<T, TypeId>(storage);
        
        begin("Replace");
        
        auto [idx, old_gen] = storage.get_slot_info(handle);
        
        // Backup old data
        std::vector<uint8_t> backup(sizeof(T));
        std::memcpy(backup.data(), old_obj, sizeof(T));
        
        // Apply new
        *old_obj = new_obj;
        auto [new_idx, new_gen] = storage.get_slot_info(handle);
        
        Change c(OpUpdate, TypeId);
        c.add_entry(ChangeEntry(idx, old_gen, new_gen), std::move(backup));
        record(std::move(c));
        
        commit();
        return true;
    }
    
    template<typename T, uint16_t TypeId, typename Modifier>
    bool modify_with_id(ReuseVector<T>& storage, ObjectId handle, Modifier&& mod) {
        T* obj = storage.get(handle);
        if (!obj) return false;
        
        // Auto-get container
        get_container<T, TypeId>(storage);
        
        begin("Modify");
        
        // Backup
        std::vector<uint8_t> backup(sizeof(T));
        std::memcpy(backup.data(), obj, sizeof(T));
        
        // Modify
        std::forward<Modifier>(mod)(*obj);
        
        auto [idx, gen] = storage.get_slot_info(handle);
        
        Change c(OpUpdate, TypeId);
        c.add_entry(ChangeEntry(idx, gen, gen), std::move(backup));
        record(std::move(c));
        
        commit();
        return true;
    }
    
    // =========================================================================
    // Undo/Redo implementation
    // =========================================================================
    
    void apply_undo(const Change& change) {
        auto* container = find_container(change.type_id);
        if (!container) return;
        
        switch (change.op) {
            case OpInsert: {
                // Undo insert = erase
                for (size_t i = 0; i < change.size(); ++i) {
                    auto [idx, old_gen, new_gen] = change.entries[i];
                    ObjectId h = make_object_id(idx, new_gen);
                    container->remove(h);
                }
                break;
            }
            case OpErase: {
                // Undo erase = restore
                for (size_t i = 0; i < change.size(); ++i) {
                    auto& e = change.entries[i];
                    const auto* backup = i < change.backups.size() ? &change.backups[i] : nullptr;
                    if (backup && !backup->empty()) {
                        container->restore(e.slot_idx, backup->data(), e.old_gen);
                    }
                }
                break;
            }
            case OpUpdate: {
                // Undo update = restore old
                for (size_t i = 0; i < change.size(); ++i) {
                    auto& e = change.entries[i];
                    const auto* backup = i < change.backups.size() ? &change.backups[i] : nullptr;
                    if (backup && !backup->empty()) {
                        container->restore(e.slot_idx, backup->data(), e.old_gen);
                    }
                }
                break;
            }
        }
    }
    
    void apply_redo(const Change& change) {
        auto* container = find_container(change.type_id);
        if (!container) return;
        
        switch (change.op) {
            case OpInsert: {
                // Redo insert = insert again (data in storage)
                // No action needed - data already in storage
                break;
            }
            case OpErase: {
                // Redo erase = erase again
                for (size_t i = 0; i < change.size(); ++i) {
                    auto [idx, old_gen, new_gen] = change.entries[i];
                    ObjectId h = make_object_id(idx, new_gen);
                    container->remove(h);
                }
                break;
            }
            case OpUpdate: {
                // Redo update = apply new (data already in storage)
                break;
            }
        }
    }
    
    IContainer* find_container(uint16_t type_id) {
        auto it = containers_.find(type_id);
        if (it != containers_.end()) {
            return it->second.get();
        }
        return nullptr;
    }
    
    static ObjectId make_object_id(uint32_t idx, uint32_t gen) {
        return ObjectId((idx << 8) | (gen & 0xFF));
    }
    
    // =========================================================================
    // Members
    // =========================================================================
    
    std::unordered_map<uint16_t, std::unique_ptr<IContainer>> containers_;
    std::vector<Transaction> undo_stack_;
    std::vector<Transaction> redo_stack_;
    Transaction current_;
    bool in_tx_ = false;
    size_t max_undo_ = 100;
};

// ============================================================================
// Type Trait Specializations
// ============================================================================

// Add your type specializations like this:
// template<> struct TypeTrait<Trace> { static constexpr uint16_t id = ObjectTypeId::Trace; };
// template<> struct TypeTrait<Via> { static constexpr uint16_t id = ObjectTypeId::Via; };

} // namespace car
