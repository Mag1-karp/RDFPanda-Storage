#ifndef RDFPANDA_STORAGE_DATALOGENGINE_H
#define RDFPANDA_STORAGE_DATALOGENGINE_H

#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <array>
#include <mutex>

#include "TripleStore.h"
#include "BloomFilter.h"

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
    
    bool get(const Key& key, Value& value) {
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
    
    void put(const Key& key, const Value& value) {
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
    
    // 添加获取缓存统计信息的方法
    size_t size() const {
        std::lock_guard<std::mutex> lock(cache_mutex);
        return cache.size();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(cache_mutex);
        cache.clear();
        lru_list.clear();
    }
};

class DatalogEngine {
private:
    TripleStore& store;
    std::vector<Rule> rules;
    std::map<std::string, std::vector<std::pair<size_t, size_t>>> rulesMap; // 谓语 -> [规则下标, 规则体中谓语下标]
    
    // LRU缓存用于存在性检查
    LRUCache<std::string, bool> existenceCache;
    
    // 批处理相关（已移除批处理逻辑以确保推理正确性）
    // static const size_t BATCH_SIZE = 100;
    // std::vector<Triple> batchBuffer;
    
    // 分片锁相关
    static const size_t SHARD_COUNT = 24;
    std::array<std::mutex, SHARD_COUNT> shardMutexes;
    
    // 对象池相关
    std::vector<std::vector<Triple>*> tripleVectorPool;
    std::vector<std::map<std::string, std::string>*> bindingMapPool;
    std::mutex poolMutex;


public:
    DatalogEngine(TripleStore& store, const std::vector<Rule>& rules) : store(store), rules(rules), existenceCache(100000) {
        initiateRulesMap();
        
        // 预分配对象池
        tripleVectorPool.reserve(50);
        bindingMapPool.reserve(50);
        for (int i = 0; i < 20; ++i) {
            tripleVectorPool.push_back(new std::vector<Triple>());
            bindingMapPool.push_back(new std::map<std::string, std::string>());
        }
    }
    
    ~DatalogEngine() {
        // 清理对象池
        for (auto* vec : tripleVectorPool) delete vec;
        for (auto* map : bindingMapPool) delete map;
    }
    void reason();

private:
    // std::vector<Triple> applyRule(const Rule& rule);
    // bool matchTriple(const Triple& triple, const Triple& pattern, std::map<std::string, std::string>& variableBindings);
    // Triple instantiateTriple(const Triple& triple, const std::map<std::string, std::string>& variableBindings);
    static bool isVariable(const std::string& str);
    // std::string getElem(const Triple& triple, int i);

    void initiateRulesMap();

    void leapfrogTriejoin(TrieNode *psoRoot, TrieNode *posRoot, const Rule &rule,
                            std::vector<Triple> &newFacts,
                            std::map<std::string, std::string> &bindings);

    void join_by_variable(TrieNode *psoRoot, TrieNode *posRoot, const Rule &rule,
                          const std::set<std::string> &variables,
                          const std::map<std::string, std::vector<std::pair<int, int>>> &varPositions,
                          std::map<std::string, std::string> &bindings, int varIdx, std::vector<Triple> &newFacts);

    static std::string substituteVariable(const std::string &term, const std::map<std::string, std::string> &bindings);

    bool checkConflictingTriples(const std::map<std::string, std::string>& bindings,
                                    const std::map<std::string, std::vector<std::pair<int, int>>>& varPositions,
                                    const Rule& rule) const;
    
    // 优化的存在性检查方法
    bool tripleExists(const Triple& triple);
    
    // 批处理方法（已移除）
    // void processBatch(std::vector<Triple>& batch);
    std::string tripleToString(const Triple& triple) const;
    
    // 分片锁相关方法
    size_t getShardIndex(const std::string& predicate) const;
    std::mutex& getShardMutex(const std::string& predicate);
    
    // 对象池相关方法
    std::vector<Triple>* getTripleVector();
    void returnTripleVector(std::vector<Triple>* vec);
    std::map<std::string, std::string>* getBindingMap();
    void returnBindingMap(std::map<std::string, std::string>* map);


    /*
    void leapfrogTriejoin(TrieNode* trieRoot, const Rule& rule, std::vector<Triple>& newFacts);
    void join_recursive(std::vector<TrieIterator*>& iterators,
                        const Rule& rule, int varIndex,
                        std::map<std::string, std::string>& binding, std::vector<Triple>& newFacts);
*/


};


#endif //RDFPANDA_STORAGE_DATALOGENGINE_H
