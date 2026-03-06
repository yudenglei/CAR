/**
 * @file car_transaction.h
 * @brief Lightweight transaction-based Undo/Redo system
 */

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class Transaction {
public:
    struct Command {
        std::function<void()> undo;
        std::function<void()> redo;
    };

    Transaction() = default;
    explicit Transaction(std::string d) : description(std::move(d)) {}

    void add(Command cmd) { commands.push_back(std::move(cmd)); }
    bool empty() const { return commands.empty(); }

    std::string description;
    std::vector<Command> commands;
};

class TransactionManager {
public:
    void begin(const std::string& desc) {
        m_current = Transaction(desc);
        m_in_transaction = true;
    }

    void record(Transaction::Command cmd) {
        if (!m_in_transaction) {
            return;
        }
        m_current.add(std::move(cmd));
    }

    void commit() {
        if (!m_in_transaction) {
            return;
        }
        m_in_transaction = false;
        if (m_current.empty()) {
            m_current = Transaction();
            return;
        }

        m_undo_stack.push_back(std::move(m_current));
        m_redo_stack.clear();
        if (m_undo_stack.size() > MAX_UNDO) {
            m_undo_stack.erase(m_undo_stack.begin());
        }
    }

    void rollback() {
        m_current = Transaction();
        m_in_transaction = false;
    }

    bool undo() {
        if (m_undo_stack.empty()) {
            return false;
        }

        Transaction tx = std::move(m_undo_stack.back());
        m_undo_stack.pop_back();
        for (auto it = tx.commands.rbegin(); it != tx.commands.rend(); ++it) {
            it->undo();
        }
        m_redo_stack.push_back(std::move(tx));
        return true;
    }

    bool redo() {
        if (m_redo_stack.empty()) {
            return false;
        }

        Transaction tx = std::move(m_redo_stack.back());
        m_redo_stack.pop_back();
        for (auto& cmd : tx.commands) {
            cmd.redo();
        }
        m_undo_stack.push_back(std::move(tx));
        return true;
    }

    bool can_undo() const { return !m_undo_stack.empty(); }
    bool can_redo() const { return !m_redo_stack.empty(); }

private:
    static constexpr std::size_t MAX_UNDO = 200;

    bool m_in_transaction = false;
    Transaction m_current;
    std::vector<Transaction> m_undo_stack;
    std::vector<Transaction> m_redo_stack;
};
