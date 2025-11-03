#include "Trie.h"
#include "StringPool.h"

// 初始化静态成员
StringPool* Triple::global_pool = nullptr;

// 实现Triple的方法
Triple::Triple(const std::string& subject, const std::string& predicate, const std::string& object) {
    if (global_pool == nullptr) {
        throw std::runtime_error("StringPool not initialized. Call Triple::setStringPool() first.");
    }
    
    subject_id = global_pool->getId(subject);
    predicate_id = global_pool->getId(predicate);
    object_id = global_pool->getId(object);
}

std::string Triple::subject() const {
    if (global_pool == nullptr) {
        return "";
    }
    return global_pool->getString(subject_id);
}

std::string Triple::predicate() const {
    if (global_pool == nullptr) {
        return "";
    }
    return global_pool->getString(predicate_id);
}

std::string Triple::object() const {
    if (global_pool == nullptr) {
        return "";
    }
    return global_pool->getString(object_id);
}

// 插入时采用 PSO 顺序：先插入 predicate，再 subject，最后 object
void Trie::insertPSO(const Triple& triple) {
    TrieNode* curr = root;
    // 顺序：predicate, subject, object (使用ID)
    std::vector<uint32_t> keys = { triple.getPredicateId(), triple.getSubjectId(), triple.getObjectId() };
    for (const auto & key : keys) {
        if (curr->children.find(key) == curr->children.end()) {
            curr->children[key] = new TrieNode();
        }
        curr = curr->children[key];
    }
    curr->isEnd = true;
}

// 插入时采用 POS 顺序：先插入 predicate，再 object，最后 subject
void Trie::insertPOS(const Triple &triple) {
    TrieNode* curr = root;
    // 顺序：predicate, object, subject (使用ID)
    std::vector<uint32_t> keys = { triple.getPredicateId(), triple.getObjectId(), triple.getSubjectId() };
    for (const auto & key : keys) {
        if (curr->children.find(key) == curr->children.end()) {
            curr->children[key] = new TrieNode();
        }
        curr = curr->children[key];
    }
    curr->isEnd = true;
}


// 仅用于调试，遍历并打印 Trie 中所有存储的三元组
void Trie::printAll() {
    std::vector<std::string> binding;
    printAllHelper(root, binding);
}

void Trie::printAllHelper(TrieNode* node, std::vector<std::string>& binding) {
    if (binding.size() == 3 && node->isEnd) {
        // binding 中顺序为 [predicate, subject, object]，
        // 但 Triple 构造函数要求 (subject, predicate, object)
        std::cout << "Triple: (" << binding[1] << ", " << binding[0] << ", " << binding[2] << ")\n";
    }
    for (auto& pair : node->children) {
        // 将ID转换为字符串用于调试显示
        std::string idStr = std::to_string(pair.first);
        binding.push_back(idStr);
        printAllHelper(pair.second, binding);
        binding.pop_back();
    }
}

// 在一组 TrieIterator 上执行 leapfrog 交集查找，找到所有迭代器中当前键值相等的位置
void LeapfrogJoin::leapfrog_search() {
    if (iterators.empty()) { // 如果没有迭代器，直接返回
        done = true;
        return;
    }
    while (true) {
        // 找出所有迭代器中最大的当前key (使用ID)
        uint32_t maxKey = iterators[0]->key();
        for (auto it : iterators) {
            if (it->key() > maxKey)
                maxKey = it->key(); // 遍历更新最大key
        }
        // 对于当前 key 小于 maxKey 的迭代器，执行 seek(maxKey)
        bool allEqual = true;
        for (TrieIterator* it : iterators) {
            if (it->key() < maxKey) {
                it->seek(maxKey);
                if (it->atEnd()) {
                    done = true;
                    return;
                }
                allEqual = false;
            }
        }
        if (allEqual) break;
    }
}