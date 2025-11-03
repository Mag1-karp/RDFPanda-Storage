#include <unordered_map>
#include <unordered_set>
#include <list>
#include <mutex>

#include "LRUCache.h"

template<typename Key, typename Value>
bool LRUCache<Key, Value>::get(const Key& key, Value& value) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    auto it = cache.find(key);
    if (it != cache.end()) {
        // 移动到列表前端
        lru_list.erase(it->second.second);
        lru_list.push_front(key);
        it->second.second = lru_list.begin();
        value = it->second.first;
        return true;
    }
    return false;
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::put(const Key& key, const Value& value) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    auto it = cache.find(key);
    if (it != cache.end()) {
        // 更新现有条目
        it->second.first = value;
        lru_list.erase(it->second.second);
        lru_list.push_front(key);
        it->second.second = lru_list.begin();
    } else {
        // 添加新条目
        if (cache.size() >= capacity) {
            // 删除最老的条目
            Key oldest = lru_list.back();
            lru_list.pop_back();
            cache.erase(oldest);
        }
        lru_list.push_front(key);
        cache[key] = {value, lru_list.begin()};
    }
}

template<typename Key, typename Value>
size_t LRUCache<Key, Value>::size() const {
    std::lock_guard<std::mutex> lock(cache_mutex);
    return cache.size();
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::clear() {
    std::lock_guard<std::mutex> lock(cache_mutex);
    cache.clear();
    lru_list.clear();
}