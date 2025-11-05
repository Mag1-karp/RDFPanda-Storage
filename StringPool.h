#ifndef STRINGPOOL_H
#define STRINGPOOL_H

#include <string>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <cstdint>
#include <mutex>

class StringPool {
private:
    // 双向映射
    std::unordered_map<std::string, uint32_t> str_to_id;
    std::vector<std::string> id_to_str;
    
    uint32_t next_id = 0;
    
    // 读写锁：读多写少的场景
    mutable std::shared_mutex pool_mutex;
    
    // 统计信息
    size_t total_string_bytes = 0;
    size_t unique_strings = 0;

public:
    StringPool() {
        // 预留空间避免频繁扩容
        str_to_id.reserve(1000000);
        id_to_str.reserve(1000000);
    }

    // 获取字符串对应的ID，不存在则创建（带预分配优化）
    uint32_t getId(const std::string& str) {
        // 先尝试读锁（大部分情况是查找已存在的字符串）
        {
            std::shared_lock<std::shared_mutex> read_lock(pool_mutex);
            auto it = str_to_id.find(str);
            if (it != str_to_id.end()) {
                return it->second;  // 已存在，直接返回
            }
        }
        
        // 需要插入新字符串，使用写锁
        std::unique_lock<std::shared_mutex> write_lock(pool_mutex);
        
        // 双重检查：可能在等待写锁期间被其他线程插入
        auto it = str_to_id.find(str);
        if (it != str_to_id.end()) {
            return it->second;
        }
        
        // 真正的插入操作
        uint32_t id = next_id++;
        
        // 预分配策略：当容量不足时预分配更多空间
        if (id_to_str.size() >= id_to_str.capacity()) {
            size_t new_capacity = id_to_str.capacity() * 2;
            id_to_str.reserve(new_capacity);
        }
        
        str_to_id.emplace(str, id);
        id_to_str.push_back(str);
        
        // 更新统计
        total_string_bytes += str.size();
        unique_strings++;
        
        return id;
    }
    
    // 根据ID获取字符串
    const std::string& getString(uint32_t id) const {
        std::shared_lock<std::shared_mutex> read_lock(pool_mutex);
        if (id >= id_to_str.size()) {
            static const std::string empty_str;
            return empty_str;  // 返回空字符串而非抛出异常
        }
        return id_to_str[id];
    }
    
    // 检查字符串是否存在
    bool contains(const std::string& str) const {
        std::shared_lock<std::shared_mutex> read_lock(pool_mutex);
        return str_to_id.find(str) != str_to_id.end();
    }
    
    // 获取ID（不创建新的）
    uint32_t getIdIfExists(const std::string& str) const {
        std::shared_lock<std::shared_mutex> read_lock(pool_mutex);
        auto it = str_to_id.find(str);
        return (it != str_to_id.end()) ? it->second : UINT32_MAX;
    }
    
    // 统计信息
    struct PoolStats {
        size_t unique_strings;
        size_t total_string_bytes;
        size_t id_map_size;
        double compression_ratio;
    };
    
    PoolStats getStats() const {
        std::shared_lock<std::shared_mutex> read_lock(pool_mutex);
        size_t estimated_original_size = total_string_bytes * getAverageReferenceCount();
        return {
            unique_strings,
            total_string_bytes,
            str_to_id.size() * (sizeof(std::string) + sizeof(uint32_t)),
            static_cast<double>(estimated_original_size) / (total_string_bytes > 0 ? total_string_bytes : 1)
        };
    }
    
    // 清空池（谨慎使用）
    void clear() {
        std::unique_lock<std::shared_mutex> write_lock(pool_mutex);
        str_to_id.clear();
        id_to_str.clear();
        next_id = 0;
        total_string_bytes = 0;
        unique_strings = 0;
    }

    // 获取当前唯一字符串数量
    size_t size() const {
        std::shared_lock<std::shared_mutex> read_lock(pool_mutex);
        return unique_strings;
    }

private:
    // 估算平均引用次数（用于统计）
    size_t getAverageReferenceCount() const {
        // 基于RDF数据特点的估算
        return 5;  // 每个字符串平均被引用5次
    }
};

#endif // STRINGPOOL_H