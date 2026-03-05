/**
 * @file car_string_pool.h
 * @brief String pool with transparent hashing
 * @version 20.0
 */

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <shared_mutex>
#include <vector>

using StringId = uint32_t;

/**
 * @class StringPool
 * @brief String interning pool with transparent hashing
 */
class StringPool {
public:
    static StringPool& get_instance() {
        static StringPool instance;
        return instance;
    }

    StringPool(const StringPool&) = delete;
    StringPool& operator=(const StringPool&) = delete;

    StringId intern(std::string_view s) {
        if (s.empty()) return 0;
        {
            std::shared_lock lock(m_mutex);
            auto it = m_map.find(s);
            if (it != m_map.end()) return it->second;
        }
        std::unique_lock lock(m_mutex);
        auto it = m_map.find(s);
        if (it != m_map.end()) return it->second;
        StringId id = static_cast<StringId>(m_strings.size());
        m_strings.emplace_back(s);
        m_map[m_strings.back()] = id;
        return id;
    }

    StringId intern(const std::string& s) {
        return intern(std::string_view(s));
    }

    std::string_view get(StringId id) const {
        if (id == 0 || id >= m_strings.size()) return "";
        return m_strings[id];
    }

    void clear() {
        std::unique_lock lock(m_mutex);
        m_map.clear();
        m_strings.clear();
        m_strings.emplace_back("");
    }

    size_t size() const { return m_strings.size(); }

private:
    StringPool() { m_strings.emplace_back(""); }
    mutable std::shared_mutex m_mutex;
    std::vector<std::string> m_strings;
    std::unordered_map<std::string, StringId> m_map;
};

inline StringId intern_string(const char* s) {
    return StringPool::get_instance().intern(s);
}

inline StringId intern_string(const std::string& s) {
    return StringPool::get_instance().intern(s);
}

inline std::string_view get_string(StringId id) {
    return StringPool::get_instance().get(id);
}
