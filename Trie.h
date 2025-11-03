#ifndef RDFPANDA_STORAGE_TRIE_H
#define RDFPANDA_STORAGE_TRIE_H


#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include "StringPool.h"

// 前向声明
class StringPool;

// Triple 和 Rule 类定义 - 使用字符串池优化
class Triple {
private:
    uint32_t subject_id;
    uint32_t predicate_id;
    uint32_t object_id;
    static StringPool* global_pool;  // 全局字符串池

public:
    // 兼容原有构造函数接口
    Triple(const std::string& subject, const std::string& predicate, const std::string& object);
    
    // 新增：直接使用ID构造（内部优化用）
    Triple(uint32_t subj_id, uint32_t pred_id, uint32_t obj_id) 
        : subject_id(subj_id), predicate_id(pred_id), object_id(obj_id) {}

    // 保持原有属性访问接口
    std::string subject() const;
    std::string predicate() const;
    std::string object() const;
    
    // 新增：高效的ID访问接口
    uint32_t getSubjectId() const { return subject_id; }
    uint32_t getPredicateId() const { return predicate_id; }
    uint32_t getObjectId() const { return object_id; }

    bool operator==(const Triple& rhs) const {
        return subject_id == rhs.subject_id && 
               predicate_id == rhs.predicate_id && 
               object_id == rhs.object_id;
    }

    bool operator!=(const Triple& rhs) const {
        return !(*this == rhs);
    }
    
    // 设置全局字符串池
    static void setStringPool(StringPool* pool) {
        global_pool = pool;
    }
    
    static StringPool* getStringPool() {
        return global_pool;
    }
};

class Rule {
public:
    std::string name;
    std::vector<Triple> body;
    Triple head;

    Rule(std::string name, const std::vector<Triple>& body, Triple head)
            : name(std::move(name)), body(body), head(std::move(head)) {}
};

// TrieNode：Trie 的节点，使用 std::map 保持子节点有序（即 PSO 顺序中的字典顺序）
// 优化：使用ID而非字符串作为键
class TrieNode {
public:
    std::map<uint32_t, TrieNode*> children;
    bool isEnd;

    TrieNode() : isEnd(false) {}
    ~TrieNode() {
        for (auto& pair : children) {
            delete pair.second;
        }
    }
};

// Trie 类，按 PSO 顺序存储三元组
// update: 按 PSO 和 POS 两种顺序存储三元组
class Trie {
public:
    TrieNode* root;

    Trie() {
        root = new TrieNode();
    }
    ~Trie() {
        delete root;
    }

    void insertPSO(const Triple& triple);
    void insertPOS(const Triple& triple);
    void printAll();

private:
    void printAllHelper(TrieNode* node, std::vector<std::string>& binding);

};

// TrieIterator：对 TrieNode 的子节点进行遍历，提供类似迭代器的接口
// 优化：使用ID而非字符串
class TrieIterator {
public:
    TrieNode* node; // 当前所在节点
    std::map<uint32_t, TrieNode*>::iterator it;
    std::map<uint32_t, TrieNode*>::iterator end;

    TrieIterator(TrieNode* n) : node(n) {
        if (node) {
            it = node->children.begin();
            end = node->children.end();
        }
    }

    bool atEnd() const {
        return it == end;
    }

    uint32_t key() const {
        return it->first;
    }

    void next() {
        if (!atEnd()) {
            ++it;
        }
    }

    // 跳跃到不小于 target 的位置
    void seek(uint32_t target) {
        it = node->children.lower_bound(target);
    }

    // open()：进入当前 key 对应的子节点，返回新的 TrieIterator
    TrieIterator open() {
        if (!atEnd()) {
            return TrieIterator(it->second);
        }
        return TrieIterator(nullptr);
    }
};

// LeapfrogJoin类：在一组TrieIterator上实现leapfrog交集查找（适用于单变量join）
class LeapfrogJoin {
public:
    std::vector<TrieIterator*> iterators;
    int p;       // 当前指针索引
    bool done;   // 标记是否结束

    LeapfrogJoin(std::vector<TrieIterator*>& its) : iterators(its), p(0), done(false) {
        // 对所有迭代器按当前 key 从小到大排序
        std::sort(iterators.begin(), iterators.end(), [](TrieIterator* a, TrieIterator* b) {
            return a->key() < b->key();
        });
        leapfrog_search();
    }

    bool atEnd() const {
        return done;
    }

    uint32_t key() const {
        return iterators[p]->key();
    }

    TrieIterator open() {
        return iterators[p]->open();
    }

    void next() {
        iterators[p]->next();
        if (iterators[p]->atEnd()) {
            done = true;
            return;
        }
        p = (p + 1) % iterators.size();
        leapfrog_search();
    }

private:
    void leapfrog_search();

};


#endif //RDFPANDA_STORAGE_TRIE_H
