#ifndef LRUCACHE_H
#define LRUCACHE_H

#include <unordered_map>
#include <unordered_set>
#include <list>
#include <mutex>

#include "TripleStore.h"

// 线程安全的LRU缓存实现
template<typename Key, typename Value>
class LRUCache {
private:
    size_t capacity;
    std::unordered_map<Key, std::pair<Value, typename std::list<Key>::iterator>> cache;
    std::list<Key> lru_list;
    mutable std::mutex cache_mutex;

public:
    LRUCache(size_t cap) : capacity(cap) {}

    bool get(const Key& key, Value& value);

    void put(const Key& key, const Value& value);

    size_t size() const;

    void clear();
};



#endif //LRUCACHE_H
