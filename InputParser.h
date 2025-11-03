#ifndef RDFPANDA_STORAGE_INPUTPARSER_H
#define RDFPANDA_STORAGE_INPUTPARSER_H

#include <string>
#include <vector>
#include <tuple>
#include <sqlite3.h>
#include <mysql.h>

#include "TripleStore.h"
#include "StringPool.h"

// using Triple = std::tuple<std::string, std::string, std::string>;

class InputParser {
private:
    StringPool* string_pool = nullptr;  // 可选的字符串池引用

public:
    // 设置字符串池（用于优化性能）
    void setStringPool(StringPool* pool) {
        string_pool = pool;
        if (pool) {
            Triple::setStringPool(pool);
        }
    }
    std::vector<Triple> parseNTriples(const std::string& filename);
    std::vector<Triple> parseTurtle(const std::string& filename);
    std::vector<Triple> parseCSV(const std::string& filename);

    // 先从SQLite开始尝试对数据库表的解析，之后可能扩展到其他数据库
    std::vector<Triple> parseSQLiteTable(const std::string& dbName, const std::string& tableName);
    // 尝试MySQL
    std::vector<Triple> parseMySQLTable(const std::string& schemaName, const std::string& tableName);
    // 并行化MySQL解析
    std::vector<Triple> parseMySQLTableParallel(const std::string& schemaName, const std::string& tableName, size_t pageSize = 10000);
    // 高级并行化MySQL解析（带连接池）
    std::vector<Triple> parseMySQLTableAdvanced(const std::string& schemaName, const std::string& tableName, 
                                               size_t pageSize = 10000, size_t maxConnections = 8);

    std::vector<Rule> parseDatalogFromFile(const std::string& filename);
    std::vector<Rule> parseDatalogFromConsole(const std::string& datalogString);
    
    // 获取字符串池统计信息（如果有的话）
    StringPool::PoolStats getStringPoolStats() const {
        if (string_pool) {
            return string_pool->getStats();
        }
        return {0, 0, 0, 0.0};
    }
};

#endif //RDFPANDA_STORAGE_INPUTPARSER_H