/**
 * @file car_string_pool.h
 * @brief String interning pool
 */

#pragma once

#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using StringId = uint32_t;

class StringPool {
public:
    static StringPool& get_instance() {
        static StringPool instance;
        return instance;
    }

    StringPool(const StringPool&) = delete;
    StringPool& operator=(const StringPool&) = delete;

    StringId intern(std::string_view sv) {
        if (sv.empty()) {
            return 0;
        }

        {
            std::shared_lock lock(m_mutex);
            auto it = m_map.find(std::string(sv));
            if (it != m_map.end()) {
                return it->second;
            }
        }

        std::unique_lock lock(m_mutex);
        auto key = std::string(sv);
        auto it = m_map.find(key);
        if (it != m_map.end()) {
            return it->second;
        }

        StringId id = static_cast<StringId>(m_strings.size());
        m_strings.emplace_back(std::move(key));
        m_map[m_strings.back()] = id;
        return id;
    }

    std::string_view get(StringId id) const {
        if (id == 0 || id >= m_strings.size()) {
            return "";
        }
        return m_strings[id];
    }

    void clear() {
        std::unique_lock lock(m_mutex);
        m_map.clear();
        m_strings.clear();
        m_strings.emplace_back("");
    }

private:
    StringPool() { m_strings.emplace_back(""); }

    mutable std::shared_mutex m_mutex;
    std::vector<std::string> m_strings;
    std::unordered_map<std::string, StringId> m_map;
};
