#include "TripleStore.h"

void TripleStore::addTriple(const Triple& triple) {
    // 检查是否已存在（可选）
    triples.push_back(triple);
    size_t index = triples.size() - 1;

    // 更新索引
    subject_index[std::get<0>(triple)].push_back(index);
    predicate_index[std::get<1>(triple)].push_back(index);
    object_index[std::get<2>(triple)].push_back(index);
}

std::vector<Triple> TripleStore::queryBySubject(const std::string& subject) {
    if (subject_index.find(subject) == subject_index.end()) {
        return {};
    }
    std::vector<Triple> result;
    for (size_t index : subject_index[subject]) {
        result.push_back(triples[index]);
    }
    return result;
}

std::vector<Triple> TripleStore::queryByPredicate(const std::string& predicate) {
    if (predicate_index.find(predicate) == predicate_index.end()) {
        return {};
    }
    std::vector<Triple> result;
    for (size_t index : predicate_index[predicate]) {
        result.push_back(triples[index]);
    }
    return result;
}

std::vector<Triple> TripleStore::queryByObject(const std::string& object) {
    if (object_index.find(object) == object_index.end()) {
        return {};
    }
    std::vector<Triple> result;
    for (size_t index : object_index[object]) {
        result.push_back(triples[index]);
    }
    return result;
}