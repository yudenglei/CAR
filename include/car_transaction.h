/**
 * @file car_transaction.h
 * @brief Typed operation-log transaction manager (no std::function callbacks)
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

enum class TxOpType : uint8_t { INSERT = 0, ERASE = 1, REPLACE = 2 };

/**
 * @brief Generic operation record.
 * before_ref/after_ref are optional opaque references (e.g. archive indexes).
 */
struct TxOp {
    TxOpType op = TxOpType::INSERT;
    uint16_t kind = 0;
    uint64_t handle = 0;
    uint32_t before_ref = 0xFFFFFFFFu;
    uint32_t after_ref = 0xFFFFFFFFu;
};

struct TxRecord {
    std::string description;
    std::vector<TxOp> ops;

    bool empty() const { return ops.empty(); }
};

/**
 * @brief Lightweight transaction stack helper.
 *
 * This class only stores/reorders operation logs.
 * Actual replay is performed by the caller (e.g. PCBDatabase) to avoid function callbacks.
 */
class TransactionManager {
public:
    void begin(const std::string& desc) {
        m_current = TxRecord{};
        m_current.description = desc;
        m_in_transaction = true;
    }

    void record(const TxOp& op) {
        if (!m_in_transaction) return;
        m_current.ops.push_back(op);
    }

    void record(TxOp&& op) {
        if (!m_in_transaction) return;
        m_current.ops.push_back(std::move(op));
    }

    void commit() {
        if (!m_in_transaction) return;
        m_in_transaction = false;

        if (m_current.empty()) {
            m_current = TxRecord{};
            return;
        }

        m_undo_stack.push_back(std::move(m_current));
        if (m_undo_stack.size() > MAX_UNDO) {
            m_undo_stack.erase(m_undo_stack.begin());
        }
        m_redo_stack.clear();
    }

    void rollback() {
        m_current = TxRecord{};
        m_in_transaction = false;
    }

    bool pop_undo(TxRecord& out) {
        if (m_undo_stack.empty()) return false;
        out = std::move(m_undo_stack.back());
        m_undo_stack.pop_back();
        return true;
    }

    bool pop_redo(TxRecord& out) {
        if (m_redo_stack.empty()) return false;
        out = std::move(m_redo_stack.back());
        m_redo_stack.pop_back();
        return true;
    }

    void push_redo(TxRecord&& tx) { m_redo_stack.push_back(std::move(tx)); }
    void push_undo(TxRecord&& tx) { m_undo_stack.push_back(std::move(tx)); }

    bool can_undo() const { return !m_undo_stack.empty(); }
    bool can_redo() const { return !m_redo_stack.empty(); }

private:
    static constexpr std::size_t MAX_UNDO = 200;

    bool m_in_transaction = false;
    TxRecord m_current;
    std::vector<TxRecord> m_undo_stack;
    std::vector<TxRecord> m_redo_stack;
};
