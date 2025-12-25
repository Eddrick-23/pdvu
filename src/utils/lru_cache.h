#include <algorithm>
#include <vector>
#include <mutex>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedN
#endif

#pragma once

/*
 * Simple lru cache using std::vector
 */

template<typename Key, typename Value>
class LRUCache {
public:
    explicit LRUCache(size_t size) : capacity(size) {
        entries.reserve(size);
    };

    std::optional<Value> get(Key key) {
        ZoneScopedN("cache get");
        std::lock_guard<std::mutex> lock(mut);
        // linear search vector, on hit, we shift that entry to front, and slide the rest back
        for (size_t i = 0; i < entries.size(); i++) {
            if (entries[i].key == key) {
                // move to current element to front and shift everything before to the right by 1
                std::rotate(entries.begin(), entries.begin() + i, entries.begin() + i + 1);
                return entries[0].value;
            }
        }

        return {};
    }

    void put(Key key, Value val) {
        ZoneScopedN("cache put");
        // put current pair at the front, the last element is pushed out
        std::lock_guard<std::mutex> lock(mut);
        for (size_t i = 0; i < entries.size(); i++) {
            if (entries[i].key == key) {
                entries[i].value = std::move(val);
                // shift to front
                std::rotate(entries.begin(), entries.begin() + i, entries.begin() + i + 1);
                return;
            }
        }
        if (entries.size() == capacity) { // evict if at capacity
            entries.pop_back();
        }
        entries.insert(entries.begin(), Entry{std::move(key), std::move(val)});
    }

    const auto& get_entries() {
        // for testing to read the storage vector
        return entries;
    }

private:
    size_t capacity;
    struct Entry {
        Key key;
        Value value;
    };
    std::vector<Entry> entries;
    std::mutex mut;
};