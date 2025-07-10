#ifndef BLOOMFILTER_H
#define BLOOMFILTER_H

#include <functional>
#include <string>
#include <vector>

class BloomFilter {
    // 用于判断某个三元组是否重复
    // todo: 可能会把某些不重复的三元组误判为重复，是否不能接受？
private:
    std::vector<bool> bitArray; // 位数组
    size_t size;               // 位数组大小
    std::vector<std::function<size_t(const std::string&)>> hashFunctions; // 哈希函数集合

public:
    BloomFilter(size_t size, size_t numHashes);

    void add(const std::string& element);
    bool contains(const std::string& element) const;

private:
    size_t hash(const std::string& element, size_t seed) const;
};



#endif //BLOOMFILTER_H
