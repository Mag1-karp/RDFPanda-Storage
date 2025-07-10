#include "BloomFilter.h"
#include <functional>

BloomFilter::BloomFilter(size_t size, size_t numHashes) : size(size), bitArray(size, false) {
    // 初始化多个哈希函数
    for (size_t i = 0; i < numHashes; ++i) {
        hashFunctions.emplace_back([i](const std::string& key) {
            std::hash<std::string> hashFunc;
            return hashFunc(key) + i * 31; // 使用不同种子生成哈希值
        });
    }
}

void BloomFilter::add(const std::string& element) {
    for (const auto& hashFunc : hashFunctions) {
        size_t hashValue = hashFunc(element) % size;
        bitArray[hashValue] = true;
    }
}

bool BloomFilter::contains(const std::string& element) const {
    for (const auto& hashFunc : hashFunctions) {
        size_t hashValue = hashFunc(element) % size;
        if (!bitArray[hashValue]) {
            return false; // 如果某个位为0，则元素一定不存在
        }
    }
    return true; // 所有位都为1，元素可能存在
}
