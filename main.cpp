#include <chrono>
#include <iostream>
#include <fstream>
#include <string>

#include "InputParser.h"
#include "TripleStore.h"
#include "DatalogEngine.h"

//// 测试用，打印文件内容
void printFileContent(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        std::cout << line << std::endl;
    }
}

//// 测试用，解析并打印n-triples文件
void parseNTFile(const std::string& filename) {
    InputParser parser;
    std::vector<Triple> triples = parser.parseNTriples(filename);
    for (const auto& triple : triples) {
        std::cout << triple.subject << " " << triple.predicate << " " << triple.object << std::endl;
    }
}

//// 测试用，解析并打印turtle文件
void parseTurtleFile(const std::string& filename) {
    InputParser parser;
    std::vector<Triple> triples = parser.parseTurtle(filename);
    for (const auto& triple : triples) {
        std::cout << triple.subject << " " << triple.predicate << " " << triple.object << std::endl;
    }
}

//// 测试用，解析并打印csv文件
void parseCSVFile(const std::string& filename) {
    InputParser parser;
    std::vector<Triple> triples = parser.parseCSV(filename);
    for (const auto& triple : triples) {
        std::cout << triple.subject << " " << triple.predicate << " " << triple.object << std::endl;
    }
}

//// 测试用，解析并打印数据库表
void parseDatabaseTable(const std::string& schemaName, const std::string& tableName) {
    InputParser parser;
    std::vector<Triple> triples = parser.parseMySQLTable(schemaName, tableName);
    for (const auto& triple : triples) {
        std::cout << triple.subject << " " << triple.predicate << " " << triple.object << std::endl;
    }
}

//// 测试用，测试SQLite数据库的连接和查询功能
void connectSQLite(const std::string& dbPath) {
    sqlite3* db;
    char* errMsg = nullptr;

    // 打开数据库
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Unable to open database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::cout << "Successfully opened database: " << dbPath << std::endl;

    // 创建表
    const char* createTableSQL = "CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, name TEXT);";
    if (sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Failed to create table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    } else {
        std::cout << "Table created successfully." << std::endl;
    }

    // 插入数据
    const char* insertSQL = "INSERT INTO test (name) VALUES ('example');";
    if (sqlite3_exec(db, insertSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Failed to insert data: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    } else {
        std::cout << "Successfully inserted data." << std::endl;
    }

    // 查询数据
    const char* selectSQL = "SELECT * FROM test;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::cout << "ID: " << id << ", Name: " << name << std::endl;
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Query failed: " << sqlite3_errmsg(db) << std::endl;
    }

    // 关闭数据库
    sqlite3_close(db);
}

//// 测试用，测试对SQLite中表的解析
void TestSQLiteTableParser() {
    InputParser parser;
    std::vector<Triple> triples = parser.parseSQLiteTable("test", "test_triple");
    for (const auto& triple : triples) {
        std::cout << triple.subject << " " << triple.predicate << " " << triple.object << std::endl;
    }
}

//// 测试用，测试TripleStore的按主语查询功能
void TestQueryBySubject() {
    InputParser parser;
    TripleStore store;

    std::vector<Triple> triples = parser.parseTurtle("input_examples/example.ttl");
    for (const auto& triple : triples) {
        store.addTriple(triple);
    }

    std::vector<Triple> queryResult = store.queryBySubject("http://example.org/Alice");
    for (const auto& triple : queryResult) {
        std::cout << triple.subject << " " << triple.predicate << " " << triple.object << std::endl;
    }
}

//// 测试用，测试简单推理功能
void TestInfer() {
    InputParser parser;
    TripleStore store;

    std::vector<Triple> triples = parser.parseTurtle("input_examples/example.ttl");
    for (const auto& triple : triples) {
        store.addTriple(triple);
    }

    // 定义规则
    std::vector<Rule> rules;
    Rule rule1(
            "rule1",
            std::vector<Triple>{
                {"?x", "http://example.org/friendOf", "?y"},
            },
            Triple{"?x", "http://example.org/knows", "?y"}
    );

    Rule rule2(
            "rule2",
            std::vector<Triple>{
                // {"?x", "http://example.org/knows", "?y"},
                {"?x", "http://example.org/knows", "?y"},
                {"?y", "http://example.org/knows", "?z"},
            },
            Triple{"?x", "http://example.org/knows", "?z"}
    );

    Rule rule3(
            "rule3",
            std::vector<Triple>{
                {"?x", "http://example.org/knows", "?y"},
            },
            Triple{"?y", "http://example.org/knows", "?x"}
    );

    rules.push_back(rule1);  // 一次迭代
    rules.push_back(rule2);  // 两次迭代，需要用到rule1的推理结果
    rules.push_back(rule3);  // 三次迭代，需要用到rule1和2的推理结果

    // 创建DatalogEngine实例
    DatalogEngine engine(store, rules);

    // 执行推理
    engine.reason();

    // 查询推理结果
    std::vector<Triple> queryResult = store.queryByPredicate("http://example.org/knows");
    for (const auto& triple : queryResult) {
        std::cout << triple.subject << " " << triple.predicate << " " << triple.object << std::endl;
    }
}

//// 测试用，解析并打印Datalog文件
void TestDatalogParser() {
    InputParser parser;
    // std::vector<Rule> rules = parser.parseDatalogFromFile("input_examples/ruleExample.dl");
    std::vector<Rule> rules = parser.parseDatalogFromFile("input_examples/DAG-R.dl");
    for (const auto& rule : rules) {
        std::cout << rule.name << std::endl;
        for (const auto& triple : rule.body) {
            std::cout << triple.subject << " " << triple.predicate << " " << triple.object << std::endl;
        }
        std::cout << "=> " << rule.head.subject << " " << rule.head.predicate << " " << rule.head.object << std::endl;
    }
}

//// 测试大文件读入及推理
void TestLargeFile() {
    InputParser parser;
    TripleStore store;
    std::vector<Triple> triples = parser.parseTurtle("input_examples/DAG.ttl");
    // std::cout << "Total triples: " << triples.size() << std::endl;
    int count = 0;
    for (const auto& triple : triples) {
        // std::cout << count++ << std::endl;
        store.addTriple(triple);
    }

    std::vector<Rule> rules = parser.parseDatalogFromFile("input_examples/DAG-R.dl");

    DatalogEngine engine(store, rules);
    engine.reason();

}

//// 测试百到万级三元组和两位数规则
void TestMidFile() {
    InputParser parser;
    TripleStore store;
    std::vector<Triple> triples = parser.parseTurtle("input_examples/mid-k.ttl");
    // std::cout << "Total triples: " << triples.size() << std::endl;
    int count = 0;
    for (const auto& triple : triples) {
        // std::cout << count++ << std::endl;
        store.addTriple(triple);
    }

    std::vector<Rule> rules = parser.parseDatalogFromFile("input_examples/mid.dl");

    DatalogEngine engine(store, rules);
    engine.reason();

}

//// 测试百万级别三元组和两位数规则
void TestMillionTriples() {
    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    InputParser parser;
    TripleStore store;

    std::vector<Triple> triples = parser.parseTurtle("input_examples/DAG.ttl");
    // std::vector<Triple> triples = parser.parseTurtle("input_examples/data_1m.ttl");
    std::cout << "Total triples: " << triples.size() << std::endl;

    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Elapsed time for parsing triples: " << elapsed.count() << " seconds" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    for (const auto& triple : triples) {
        store.addTriple(triple);
    }
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Elapsed time for storing triples: " << elapsed.count() << " seconds" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    std::vector<Rule> rules = parser.parseDatalogFromFile("input_examples/DAG-R.dl");
    // std::vector<Rule> rules = parser.parseDatalogFromFile("input_examples/mid.dl");
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Elapsed time for parsing rules:   " << elapsed.count() << " seconds" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    DatalogEngine engine(store, rules);
    engine.reason();
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Elapsed time for reasoning:       " << elapsed.count() << " seconds" << std::endl;

}

//// 计时用
void startTimer() {
    // 用结束时间与开始时间相减
    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    // 这里调用要测试的函数
    // TestLargeFile();
    // TestMidFile();
    TestMillionTriples();

    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    // 输出用时
    std::cout << "Elapsed time: " << elapsed.count() << " seconds" << std::endl;
}

int main() {

    // TestInfer();
    // TestDatalogParser();
    // TestLargeFile();
    // startTimer();
    TestMillionTriples();

    // parseDatabaseTable("rdfpanda", "triples");
    // connectSQLite("./SQLiteDb/test.db");
    // TestSQLiteTableParser();

    return 0;
}
