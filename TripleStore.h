#ifndef RDFPANDA_STORAGE_TRIPLESTORE_H
#define RDFPANDA_STORAGE_TRIPLESTORE_H

#include <utility>
#include <vector>
#include <unordered_map>
#include <string>

#include "Trie.h"
#include "StringPool.h"

//// Triple 和 Rule 类已定义在Trie.h中

class TripleStore {
private:
    // 字符串池
    StringPool string_pool;
    
    // 主存储：紧凑的ID存储
    struct TripleIds {
        uint32_t subject_id;
        uint32_t predicate_id;
        uint32_t object_id;
        
        TripleIds(uint32_t s, uint32_t p, uint32_t o) 
            : subject_id(s), predicate_id(p), object_id(o) {}
    };
    std::vector<TripleIds> triple_ids;
    
    
    // 使用Trie树优化
    Trie triePSO;
    Trie triePOS;

    // 优化后的索引：使用ID而非字符串
    std::unordered_map<uint32_t, std::vector<uint32_t>> subject_index;  // Subject ID → Triple Index
    std::unordered_map<uint32_t, std::vector<uint32_t>> predicate_index; // Predicate ID → Triple Index
    std::unordered_map<uint32_t, std::vector<uint32_t>> object_index;    // Object ID → Triple Index

public:
    // 构造函数：初始化字符串池
    TripleStore() {
        Triple::setStringPool(&string_pool);
    }
    
    void addTriple(const Triple& triple);
    std::vector<Triple> queryBySubject(const std::string& subject);
    std::vector<Triple> queryByPredicate(const std::string& predicate);
    std::vector<Triple> queryByObject(const std::string& object);
    
    // 新增：高效的ID查询接口
    std::vector<uint32_t> queryTripleIdsBySubjectId(uint32_t subject_id);
    std::vector<uint32_t> queryTripleIdsByPredicateId(uint32_t predicate_id);
    std::vector<uint32_t> queryTripleIdsByObjectId(uint32_t object_id);
    
    // 根据Triple ID获取Triple对象
    Triple getTripleById(uint32_t triple_id) const;
    
    const std::vector<TripleIds>& getAllTripleIds() const { return triple_ids; }
    
    // 获取三元组总数
    size_t getTripleCount() const { return triple_ids.size(); }

    TrieNode* getNodeByTriple(const Triple& triple) const;

    TrieNode* getTriePSORoot() const { return triePSO.root; }
    TrieNode* getTriePOSRoot() const { return triePOS.root; }
    
    // 获取字符串池统计信息
    StringPool::PoolStats getStringPoolStats() const {
        return string_pool.getStats();
    }
    
    // 获取字符串池的引用（用于其他模块）
    StringPool& getStringPool() { return string_pool; }
    const StringPool& getStringPool() const { return string_pool; }
};



#endif //RDFPANDA_STORAGE_TRIPLESTORE_H
