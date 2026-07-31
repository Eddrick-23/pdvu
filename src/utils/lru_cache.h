#pragma once
#include <algorithm>
#include <iterator>
#include <mutex>
#include <vector>

#include "utils/profiling.h"

/**
 * @brief A thread-safe Least Recently Used (LRU) cache backed by a std::vector.
 *
 * This implementation uses a std::vector to store key-value pairs in LRU order,
 * with the most recently accessed items kept at the front. Thread safety is
 * managed internally via a std::mutex.
 *
 * @tparam Key The type of keys stored in the cache.
 * @tparam Value The type of values stored in the cache.
 */
template <typename Key, typename Value>
class LRUCache {
 public:
  /**
   * @brief Represents a single key-value entry in the cache
   */
  struct Entry {
    Key key;
    Value value;

    auto operator<=>(const Entry&) const = default;
  };

  /**
   * @brief Constructs an LRUCache with the specified maximum capacity.
   *
   * @param size The maximum number of entries the cache can hold.
   */
  explicit LRUCache(size_t size) : capacity(size) { entries.reserve(size); }

  /**
   * @brief Retrieves a value from the cache by key.
   *
   * Performs a linear search for the key. If found, the entry is promoted to
   * the front of the cache (most recently used) and its value is returned.
   *
   * @param key The key to look up.
   * @return std::optional<Value> Containing the value if found, or std::nullopt otherwise.
   */
  std::optional<Value> get(Key key) {
    ZoneScopedN("cache get");
    std::scoped_lock lock(mut);
    // linear search vector, on hit, we shift that entry to front, and slide the
    // rest back
    for (size_t i = 0; i < entries.size(); i++) {
      if (entries[i].key == key) {
        // move to current element to front and shift everything before to the
        // right by 1
        std::rotate(entries.begin(), entries.begin() + i, entries.begin() + i + 1);
        return entries[0].value;
      }
    }

    return {};
  }

  /**
   * @brief Inserts or updates a key-value pair in the cache.
   *
   * If the key already exists, its value is updated and moved to the front.
   * Otherwise, the pair is inserted at the front. If the cache is at capacity,
   * the least recently used element (at the back) is evicted.
   *
   * @param key The key to insert or update.
   * @param val The value to associate with the key.
   */
  void put(Key key, Value val) {
    ZoneScopedN("cache put");
    // put current pair at the front, the last element is pushed out
    std::scoped_lock lock(mut);
    for (size_t i = 0; i < entries.size(); i++) {
      if (entries[i].key == key) {
        entries[i].value = std::move(val);
        // shift to front
        std::rotate(entries.begin(), entries.begin() + i, entries.begin() + i + 1);
        return;
      }
    }
    if (entries.size() == capacity) {  // evict if at capacity
      entries.pop_back();
    }
    entries.insert(entries.begin(), Entry{std::move(key), std::move(val)});
  }

  /**
   * @brief Retrieves a reference to the underlying storage vector.
   *
   * Intended primarily for testing and inspecting current internal cache state.
   *
   * @return A constant reference to the underlying std::vector of entries.
   */
  [[nodiscard]] const auto& get_entries() { return entries; }

  /**
   * @brief Removes a key-value pair from the cache if it exists.
   *
   * @param key The key to erase from the cache.
   */
  void erase(Key key) {
    ZoneScopedN("cache erase");
    std::scoped_lock lock(mut);
    auto it = std::ranges::find(entries, key, &Entry::key);
    if (it != entries.end()) {
      entries.erase(it);
    }
  }

 private:
  size_t capacity;  ///< Maximum number of items the cache can hold.
  std::vector<Entry>
      entries;     ///< Internal storage ordering entries from most to least recently used.
  std::mutex mut;  ///< Mutex protecting internal access and state transitions.
};