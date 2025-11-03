#include "InputParser.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>
#include <future>
#include <vector>
#include <mutex>
#include <queue>
#include <atomic>

#include "DatabaseConfig.h"

std::vector<Triple> InputParser::parseNTriples(const std::string& filename) {
    std::vector<Triple> triples;
    std::ifstream file(filename);
    // std::cout << "Parsing file: " << filename << std::endl;
    std::string line;
    std::regex tripleRegex(R"(<([^>]+)> <([^>]+)> (\"[^\"]*\"|<[^>]+>|_:.*) \.)");
    // 主语： <uri>
    // 谓语： <uri>
    // 宾语： <uri> 或 "literal" 或 _:blankNode

    while (std::getline(file, line)) {
        // std::cout << line << std::endl;
        std::smatch match;
        if (std::regex_match(line, match, tripleRegex)) {
            std::string subject = match[1].str();
            std::string predicate = match[2].str();
            std::string object = match[3].str();
            triples.emplace_back(subject, predicate, object);
        }
    }

    return triples;
}

std::vector<Triple> InputParser::parseTurtle(const std::string& filename) {
    std::vector<Triple> triples;
    std::ifstream file(filename);
    std::string line;

    // 正则表达式匹配三元组和前缀声明
    std::regex tripleRegex(R"((<[^>]+>|_:.*|[^:]+:[^ ]+)\s+(<[^>]+>|[^:]+:[^ ]+)\s+(\"[^\"]*\"|<[^>]+>|_:.*|[^:]+:[^ ]+)\s*\.)");
    std::regex prefixRegex(R"(@prefix\s+([^:]+):\s+<([^>]+)>\s*\.)");

    // 全局前缀映射表
    std::map<std::string, std::string> prefixMap;

    // 第一步：预处理前缀声明
    while (std::getline(file, line)) {
        line = std::regex_replace(line, std::regex(R"(^\s+|\s+$)"), "");
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::smatch prefixMatch;
        if (std::regex_match(line, prefixMatch, prefixRegex)) {
            std::string prefixName = prefixMatch[1].str();
            std::string prefixUri = prefixMatch[2].str();
            prefixMap[prefixName] = prefixUri;
        }

        // 当匹配到三元组时说明已经读到数据部分，停止读取前缀声明
        std::smatch tripleMatch;
        if (std::regex_match(line, tripleMatch, tripleRegex)) {
            break;
        }
    }

    // 重置文件流
    file.clear();
    file.seekg(0, std::ios::beg);

    // 第二步：多线程处理三元组
    std::vector<std::string> lines;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    const size_t numThreads = std::thread::hardware_concurrency();
    size_t chunkSize = lines.size() / numThreads;
    std::vector<std::vector<Triple>> threadResults(numThreads);
    std::mutex resultMutex;

    auto parseChunk = [&](size_t start, size_t end, size_t threadIndex) {
        // std::chrono::high_resolution_clock::time_point startTime = std::chrono::high_resolution_clock::now();
        std::vector<Triple> localTriples;
        const char* ws = " \t\n\r";

        for (size_t i = start; i < end; ++i) {
            std::string line = lines[i];
            // line = std::regex_replace(line, std::regex(R"(^\s+|\s+$)"), "");
            // 去除行首尾空白字符
            line.erase(0, line.find_first_not_of(ws));
            line.erase(line.find_last_not_of(ws) + 1);

            if (line.empty() || line[0] == '#' || std::regex_match(line, prefixRegex)) {
                continue;
            }

            std::smatch tripleMatch;
            if (std::regex_match(line, tripleMatch, tripleRegex)) {
                auto expandPrefix = [&prefixMap](const std::string& term) -> std::string {
                    size_t colonPos = term.find(':');
                    if (colonPos != std::string::npos) {
                        std::string prefix = term.substr(0, colonPos);
                        if (prefixMap.find(prefix) != prefixMap.end()) {
                            return prefixMap[prefix] + term.substr(colonPos + 1);
                        }
                    }
                    return term;
                };

                std::string subject = expandPrefix(tripleMatch[1].str());
                std::string predicate = expandPrefix(tripleMatch[2].str());
                std::string object = expandPrefix(tripleMatch[3].str());
                localTriples.emplace_back(subject, predicate, object);
            }

        }

        std::lock_guard<std::mutex> lock(resultMutex);
        threadResults[threadIndex] = std::move(localTriples);

//        std::chrono::high_resolution_clock::time_point endTime = std::chrono::high_resolution_clock::now();
//        std::chrono::duration<double> elapsedTime = endTime - startTime;
//        std::cout << "Thread " << threadIndex << " processed " << (end - start) << " lines in " << elapsedTime.count() << " seconds." << std::endl;
    };

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numThreads; ++i) {
        size_t start = i * chunkSize;
        size_t end = (i == numThreads - 1) ? lines.size() : start + chunkSize;
        threads.emplace_back(parseChunk, start, end, i);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (const auto& threadResult : threadResults) {
        triples.insert(triples.end(), threadResult.begin(), threadResult.end());
    }

    return triples;
}

std::vector<Triple> InputParser::parseCSV(const std::string& filename) {
    std::vector<Triple> triples;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string subject, predicate, object;

        if (std::getline(ss, subject, ',') &&
            std::getline(ss, predicate, ',') &&
            std::getline(ss, object, ',')) {
            triples.emplace_back(subject, predicate, object);
        }
    }

    return triples;
}

std::vector<Triple> InputParser::parseMySQLTable(const std::string& schemaName, const std::string& tableName) {
    std::vector<Triple> triples;
    MYSQL* conn;
    MYSQL_RES* res;
    MYSQL_ROW row;

    // 初始化 MySQL 连接
    conn = mysql_init(nullptr);
    if (conn == nullptr) {
        std::cerr << "mysql_init() failed" << std::endl;
        return triples;
    }

    // 连接到数据库
    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASSWORD, schemaName.c_str(), DB_PORT, nullptr, 0) == nullptr) {
        std::cerr << "mysql_real_connect() failed: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return triples;
    }

    // 构建查询语句
    std::string query = "SELECT subject, predicate, object FROM " + tableName;

    // 执行查询
    if (mysql_query(conn, query.c_str())) {
        std::cerr << "mysql_query() failed: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return triples;
    }

    // 获取查询结果
    res = mysql_store_result(conn);
    if (res == nullptr) {
        std::cerr << "mysql_store_result() failed: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return triples;
    }

    // 解析结果集
    while ((row = mysql_fetch_row(res))) {
        std::string subject = row[0] ? row[0] : "";
        std::string predicate = row[1] ? row[1] : "";
        std::string object = row[2] ? row[2] : "";
        triples.emplace_back(subject, predicate, object);
    }

    // 释放资源
    mysql_free_result(res);
    mysql_close(conn);

    return triples;
}

std::vector<Triple> InputParser::parseSQLiteTable(const std::string &dbName, const std::string &tableName) {
    std::vector<Triple> triples;
    sqlite3* db;
    sqlite3_stmt* stmt;
    std::string dbPath = "./SQLiteDb/" + dbName + ".db";
    // 打开数据库
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Unable to open database: " << sqlite3_errmsg(db) << std::endl;
        return triples;
    }
    std::string query = "SELECT subject, predicate, object FROM " + tableName;
    // 准备查询语句
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return triples;
    }
    // 执行查询并解析结果
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* subject = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* predicate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* object = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        if (subject && predicate && object) {
            triples.emplace_back(subject, predicate, object);
        }
    }
    // 释放资源
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return triples;
}


std::vector<Rule> InputParser::parseDatalogFromFile(const std::string &filename) {
    std::vector<Rule> rules;
    std::ifstream file(filename);
    std::string line;

    // 正则表达式匹配 PREFIX 声明
    std::regex prefixRegex(R"(PREFIX\s+([^:]+):\s+<([^>]+)>)");
    // 正则表达式匹配规则（支持有无前缀的情况）
    std::regex ruleRegex(R"(([\w:]+\([^)]+\)) :- (.+)\.)");
    std::regex tripleRegex(R"(([\w:]+)\(([^,]+), ([^)]+)\))");

    // 前缀映射表
    std::map<std::string, std::string> prefixMap;

    while (std::getline(file, line)) {
        // 去除行首尾空白字符
        line = std::regex_replace(line, std::regex(R"(^\s+|\s+$)"), "");

        // 跳过空行和注释
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // 匹配 PREFIX 声明
        std::smatch prefixMatch;
        if (std::regex_match(line, prefixMatch, prefixRegex)) {
            std::string prefixName = prefixMatch[1].str();
            std::string prefixUri = prefixMatch[2].str();
            prefixMap[prefixName] = prefixUri;
            continue;
        }

        // 匹配规则
        std::smatch ruleMatch;
        if (std::regex_match(line, ruleMatch, ruleRegex)) {
            std::string headStr = ruleMatch[1].str();
            std::string bodyStr = ruleMatch[2].str();

            // 展开前缀的辅助函数
            auto expandPrefix = [&prefixMap](const std::string& term) -> std::string {
                size_t colonPos = term.find(':');
                if (colonPos != std::string::npos) {
                    std::string prefix = term.substr(0, colonPos);
                    if (prefixMap.find(prefix) != prefixMap.end()) {
                        return prefixMap[prefix] + term.substr(colonPos + 1);
                    }
                }
                return term; // 如果没有前缀，直接返回原始值
            };

            // 解析规则头部
            std::smatch headMatch;
            auto matchResult = std::regex_match(headStr, headMatch, tripleRegex);
            Triple head(
                headMatch[2].str(),
                expandPrefix(headMatch[1].str()),
                headMatch[3].str()
            );

            // 解析规则体
            std::vector<Triple> body;
            std::sregex_iterator bodyBegin(bodyStr.begin(), bodyStr.end(), tripleRegex);
            std::sregex_iterator bodyEnd;
            for (std::sregex_iterator i = bodyBegin; i != bodyEnd; ++i) {
                const std::smatch& bodyMatch = *i;
                body.emplace_back(
                    bodyMatch[2].str(),
                    expandPrefix(bodyMatch[1].str()),
                    bodyMatch[3].str()
                );
            }

            rules.emplace_back("", body, head);
        }
    }

    return rules;
}

std::vector<Triple> InputParser::parseMySQLTableParallel(const std::string& schemaName, const std::string& tableName, size_t pageSize) {
    std::vector<Triple> allTriples;
    
    // 首先获取表的总行数
    MYSQL* countConn = mysql_init(nullptr);
    if (countConn == nullptr) {
        std::cerr << "mysql_init() failed for count query" << std::endl;
        return allTriples;
    }
    
    if (mysql_real_connect(countConn, DB_HOST, DB_USER, DB_PASSWORD, schemaName.c_str(), DB_PORT, nullptr, 0) == nullptr) {
        std::cerr << "mysql_real_connect() failed for count query: " << mysql_error(countConn) << std::endl;
        mysql_close(countConn);
        return allTriples;
    }
    
    // 获取表的总行数
    std::string countQuery = "SELECT COUNT(*) FROM " + tableName;
    size_t totalRows = 0;
    
    if (mysql_query(countConn, countQuery.c_str()) == 0) {
        MYSQL_RES* countRes = mysql_store_result(countConn);
        if (countRes) {
            MYSQL_ROW countRow = mysql_fetch_row(countRes);
            if (countRow && countRow[0]) {
                totalRows = std::stoul(countRow[0]);
            }
            mysql_free_result(countRes);
        }
    }
    
    mysql_close(countConn);
    
    if (totalRows == 0) {
        std::cerr << "No data found in table " << tableName << std::endl;
        return allTriples;
    }
    
    // 计算分页数量和线程数
    const size_t numThreads = std::min(static_cast<size_t>(std::thread::hardware_concurrency()), (totalRows + pageSize - 1) / pageSize);
    const size_t actualPageSize = (totalRows + numThreads - 1) / numThreads;
    
    std::vector<std::vector<Triple>> threadResults(numThreads);
    std::vector<std::thread> threads;
    std::mutex resultMutex;
    
    // 并行处理函数
    auto processPage = [&](size_t threadIndex, size_t offset, size_t limit) {
        MYSQL* conn = mysql_init(nullptr);
        if (conn == nullptr) {
            std::cerr << "mysql_init() failed for thread " << threadIndex << std::endl;
            return;
        }
        
        if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASSWORD, schemaName.c_str(), DB_PORT, nullptr, 0) == nullptr) {
            std::cerr << "mysql_real_connect() failed for thread " << threadIndex << ": " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        
        // 构建分页查询
        std::string query = "SELECT subject, predicate, object FROM " + tableName + 
                           " LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset);
        
        if (mysql_query(conn, query.c_str()) != 0) {
            std::cerr << "mysql_query() failed for thread " << threadIndex << ": " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        
        MYSQL_RES* res = mysql_store_result(conn);
        if (res == nullptr) {
            std::cerr << "mysql_store_result() failed for thread " << threadIndex << ": " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        
        std::vector<Triple> localTriples;
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            std::string subject = row[0] ? row[0] : "";
            std::string predicate = row[1] ? row[1] : "";
            std::string object = row[2] ? row[2] : "";
            localTriples.emplace_back(subject, predicate, object);
        }
        
        mysql_free_result(res);
        mysql_close(conn);
        
        // 线程安全地存储结果
        std::lock_guard<std::mutex> lock(resultMutex);
        threadResults[threadIndex] = std::move(localTriples);
    };
    
    // 启动工作线程
    for (size_t i = 0; i < numThreads; ++i) {
        size_t offset = i * actualPageSize;
        size_t limit = std::min(actualPageSize, totalRows - offset);
        
        if (limit > 0) {
            threads.emplace_back(processPage, i, offset, limit);
        }
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 合并结果
    for (const auto& threadResult : threadResults) {
        allTriples.insert(allTriples.end(), threadResult.begin(), threadResult.end());
    }
    
    return allTriples;
}

std::vector<Triple> InputParser::parseMySQLTableAdvanced(const std::string& schemaName, const std::string& tableName, 
                                                       size_t pageSize, size_t maxConnections) {
    std::vector<Triple> allTriples;
    
    // 获取表的总行数
    MYSQL* countConn = mysql_init(nullptr);
    if (countConn == nullptr) {
        std::cerr << "mysql_init() failed for count query" << std::endl;
        return allTriples;
    }
    
    if (mysql_real_connect(countConn, DB_HOST, DB_USER, DB_PASSWORD, schemaName.c_str(), DB_PORT, nullptr, 0) == nullptr) {
        std::cerr << "mysql_real_connect() failed for count query: " << mysql_error(countConn) << std::endl;
        mysql_close(countConn);
        return allTriples;
    }
    
    std::string countQuery = "SELECT COUNT(*) FROM " + tableName;
    size_t totalRows = 0;
    
    if (mysql_query(countConn, countQuery.c_str()) == 0) {
        MYSQL_RES* countRes = mysql_store_result(countConn);
        if (countRes) {
            MYSQL_ROW countRow = mysql_fetch_row(countRes);
            if (countRow && countRow[0]) {
                totalRows = std::stoul(countRow[0]);
            }
            mysql_free_result(countRes);
        }
    }
    mysql_close(countConn);
    
    if (totalRows == 0) {
        return allTriples;
    }
    
    // 计算分页数量
    const size_t numPages = (totalRows + pageSize - 1) / pageSize;
    const size_t numThreads = std::min(maxConnections, numPages);
    
    // 线程安全的任务队列
    std::queue<std::pair<size_t, size_t>> taskQueue; // (offset, limit)
    for (size_t i = 0; i < numPages; ++i) {
        size_t offset = i * pageSize;
        size_t limit = std::min(pageSize, totalRows - offset);
        taskQueue.push({offset, limit});
    }
    
    std::mutex queueMutex;
    std::mutex resultMutex;
    std::vector<Triple> results;
    std::atomic<size_t> completedTasks{0};
    std::atomic<size_t> failedTasks{0};
    
    // 工作线程函数
    auto workerThread = [&](size_t threadId) {
        MYSQL* conn = mysql_init(nullptr);
        if (conn == nullptr) {
            std::cerr << "Thread " << threadId << ": mysql_init() failed" << std::endl;
            return;
        }
        
        // 设置连接超时
        unsigned int timeout = 30;
        mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        mysql_options(conn, MYSQL_OPT_READ_TIMEOUT, &timeout);
        mysql_options(conn, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
        
        if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASSWORD, schemaName.c_str(), DB_PORT, nullptr, 0) == nullptr) {
            std::cerr << "Thread " << threadId << ": mysql_real_connect() failed: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        
        while (true) {
            std::pair<size_t, size_t> task;
            
            // 从队列中获取任务
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (taskQueue.empty()) {
                    break;
                }
                task = taskQueue.front();
                taskQueue.pop();
            }
            
            // 执行任务
            std::string query = "SELECT subject, predicate, object FROM " + tableName + 
                               " LIMIT " + std::to_string(task.second) + " OFFSET " + std::to_string(task.first);
            
            if (mysql_query(conn, query.c_str()) != 0) {
                std::cerr << "Thread " << threadId << ": Query failed: " << mysql_error(conn) << std::endl;
                failedTasks++;
                continue;
            }
            
            MYSQL_RES* res = mysql_store_result(conn);
            if (res == nullptr) {
                std::cerr << "Thread " << threadId << ": mysql_store_result() failed: " << mysql_error(conn) << std::endl;
                failedTasks++;
                continue;
            }
            
            std::vector<Triple> localTriples;
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res))) {
                if (row[0] && row[1] && row[2]) {
                    localTriples.emplace_back(row[0], row[1], row[2]);
                }
            }
            
            mysql_free_result(res);
            
            // 线程安全地添加结果
            {
                std::lock_guard<std::mutex> lock(resultMutex);
                results.insert(results.end(), localTriples.begin(), localTriples.end());
            }
            
            completedTasks++;
        }
        
        mysql_close(conn);
    };
    
    // 启动工作线程
    std::vector<std::thread> threads;
    for (size_t i = 0; i < numThreads; ++i) {
        threads.emplace_back(workerThread, i);
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 检查是否有失败的任务
    if (failedTasks > 0) {
        std::cerr << "Warning: " << failedTasks << " tasks failed during parallel MySQL parsing" << std::endl;
    }
    
    std::cout << "Completed " << completedTasks << " tasks, processed " << results.size() << " triples" << std::endl;
    
    return results;
}
