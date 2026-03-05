/**
 * @file car_transaction.h
 * @brief Transaction-based Undo/Redo system
 */

#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <atomic>
#include <cstring>

constexpr size_t LARGE_OBJECT_THRESHOLD = 256;

enum class OperationType : uint8_t { ADD = 0, REMOVE = 1, REPLACE = 2 };
enum class ObjectType : uint8_t { UNKNOWN = 0, PADSTACK_DEF = 1, TRACE = 2, VIA = 3, COMPONENT = 4, NET = 5, LAYER = 6, SURFACE = 7, BONDWIRE = 8, TEXT = 9, BOARD = 10 };
using ObjectId = uint64_t;

struct Snapshot {
    std::vector<uint8_t> data;
    std::shared_ptr<void> large;
    bool is_large() const { return large != nullptr; }
    static Snapshot create_small(const void* ptr, size_t size) { Snapshot s; s.data.resize(size); memcpy(s.data.data(), ptr, size); return s; }
    template<typename T> static Snapshot create_large(const std::shared_ptr<T>& ptr) { Snapshot s; s.large = ptr; return s; }
};

class SnapshotCache {
public:
    static SnapshotCache& get_instance() { static SnapshotCache instance; return instance; }
    size_t add_snapshot(Snapshot&& s) { size_t id = m_next_id++; m_snapshots[id] = std::move(s); return id; }
    Snapshot* get_snapshot(size_t id) { auto it = m_snapshots.find(id); return it != m_snapshots.end() ? &it->second : nullptr; }
    void remove_snapshot(size_t id) { m_snapshots.erase(id); }
private: SnapshotCache() = default; std::atomic<size_t> m_next_id = 1; std::unordered_map<size_t, Snapshot> m_snapshots;
};

struct Change {
    OperationType op; ObjectType obj_type; ObjectId handle; uint32_t slot_idx; uint32_t old_gen;
    Snapshot snapshot; bool is_large_object = false; size_t snapshot_id = 0;
    Change() = default;
    Change(OperationType o, ObjectType t, ObjectId h, uint32_t idx, uint32_t gen) : op(o), obj_type(t), handle(h), slot_idx(idx), old_gen(gen) {}
};

class Transaction {
{
public:
    Transaction() = default;
    explicit Transaction(const std::string& d) : description(d) {}
    void add_change(Change&& c) { changes.push_back(std::move(c)); }
    std::string description;
    std::vector<Change> changes;
};

class TransactionManager
{
public:
    void begin(const std::string& desc) { m_current = Transaction(desc); m_in_transaction = true; }
    void commit() { if (!m_in_transaction || m_current.changes.empty()) { m_in_transaction = false; return; } m_undo_stack.push_back(std::move(m_current)); m_redo_stack.clear(); m_in_transaction = false; if (m_undo_stack.size() > MAX_UNDO) m_undo_stack.erase(m_undo_stack.begin()); }
    void rollback() { m_in_transaction = false; m_current = Transaction(); }
    void record(Change&& change) { if (!m_in_transaction) return; m_current.add_change(std::move(change)); }
    bool undo() { if (m_undo_stack.empty()) return false; auto tx = std::move(m_undo_stack.back()); m_undo_stack.pop_back(); for (auto it = tx.changes.rbegin(); it != tx.changes.rend(); ++it) apply_reverse(*it); m_redo_stack.push_back(std::move(tx)); return true; }
    bool redo() { if (m_redo_stack.empty()) return false; auto tx = std::move(m_redo_stack.back()); m_redo_stack.pop_back(); for (auto& c : tx.changes) apply_forward(c); m_undo_stack.push_back(std::move(tx)); return true; }
    bool can_undo() const { return !m_undo_stack.empty(); }
    bool can_redo() const { return !m_redo_stack.empty(); }
    void apply_reverse(const Change& c);
    void apply_forward(const Change& c);
    void set_database(class PCBDatabase* db) { m_database = db; }
private:
    static constexpr size_t MAX_UNDO = 100;
    bool m_in_transaction = false;
    Transaction m_current;
    std::vector<Transaction> m_undo_stack;
    std::vector<Transaction> m_redo_stack;
    class PCBDatabase* m_database = nullptr;
};
