#include "TripleStore.h"

void TripleStore::addTriple(const Triple& triple) {
    // 获取当前三元组的索引
    uint32_t triple_index = static_cast<uint32_t>(triple_ids.size());
    
    // 存储紧凑的ID版本
    triple_ids.emplace_back(
        triple.getSubjectId(),
        triple.getPredicateId(),
        triple.getObjectId()
    );

    // 更新优化后的索引：使用ID作为key
    subject_index[triple.getSubjectId()].push_back(triple_index);
    predicate_index[triple.getPredicateId()].push_back(triple_index);
    object_index[triple.getObjectId()].push_back(triple_index);

    // 继续使用Trie树优化（保持现有逻辑）
    triePSO.insertPSO(triple);
    triePOS.insertPOS(triple);
}

std::vector<Triple> TripleStore::queryBySubject(const std::string& subject) {
    // 转换为ID查询
    uint32_t subject_id = string_pool.getIdIfExists(subject);
    if (subject_id == UINT32_MAX) {
        return {};
    }
    
    std::vector<uint32_t> triple_indices = queryTripleIdsBySubjectId(subject_id);
    std::vector<Triple> result;
    result.reserve(triple_indices.size());
    
    for (uint32_t index : triple_indices) {
        result.push_back(getTripleById(index));
    }
    return result;
}

std::vector<Triple> TripleStore::queryByPredicate(const std::string& predicate) {
    // 转换为ID查询
    uint32_t predicate_id = string_pool.getIdIfExists(predicate);
    if (predicate_id == UINT32_MAX) {
        return {};
    }
    
    std::vector<uint32_t> triple_indices = queryTripleIdsByPredicateId(predicate_id);
    std::vector<Triple> result;
    result.reserve(triple_indices.size());
    
    for (uint32_t index : triple_indices) {
        result.push_back(getTripleById(index));
    }
    return result;
}

std::vector<Triple> TripleStore::queryByObject(const std::string& object) {
    // 转换为ID查询
    uint32_t object_id = string_pool.getIdIfExists(object);
    if (object_id == UINT32_MAX) {
        return {};
    }
    
    std::vector<uint32_t> triple_indices = queryTripleIdsByObjectId(object_id);
    std::vector<Triple> result;
    result.reserve(triple_indices.size());
    
    for (uint32_t index : triple_indices) {
        result.push_back(getTripleById(index));
    }
    return result;
}

// 新增：高效的ID查询接口
std::vector<uint32_t> TripleStore::queryTripleIdsBySubjectId(uint32_t subject_id) {
    auto it = subject_index.find(subject_id);
    if (it != subject_index.end()) {
        return it->second;
    }
    return {};
}

std::vector<uint32_t> TripleStore::queryTripleIdsByPredicateId(uint32_t predicate_id) {
    auto it = predicate_index.find(predicate_id);
    if (it != predicate_index.end()) {
        return it->second;
    }
    return {};
}

std::vector<uint32_t> TripleStore::queryTripleIdsByObjectId(uint32_t object_id) {
    auto it = object_index.find(object_id);
    if (it != object_index.end()) {
        return it->second;
    }
    return {};
}

Triple TripleStore::getTripleById(uint32_t triple_id) const {
    if (triple_id >= triple_ids.size()) {
        throw std::out_of_range("Triple ID out of range");
    }
    
    const auto& ids = triple_ids[triple_id];
    return Triple(ids.subject_id, ids.predicate_id, ids.object_id);
}

TrieNode* TripleStore::getNodeByTriple(const Triple& triple) const {
    // 返回指定三元组的Trie节点 (使用ID)
    TrieNode* node = triePSO.root;
    std::vector<uint32_t> keys = { triple.getPredicateId(), triple.getSubjectId(), triple.getObjectId() };
    for (const auto & key : keys) {
        if (node->children.find(key) == node->children.end()) {
            return nullptr;
        }
        node = node->children[key];
    }
    return node;
}