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
                {"?x", "http://example.org/friendOf", "?y"},
                {"?y", "http://example.org/friendOf", "?z"},
            },
            Triple{"?x", "http://example.org/knows", "?z"}
    );

    rules.push_back(rule1);
    // rules.push_back(rule2);

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
    std::vector<Rule> rules = parser.parseDatalogFromFile("input_examples/ruleExample.dl");
    for (const auto& rule : rules) {
        std::cout << rule.name << std::endl;
        for (const auto& triple : rule.body) {
            std::cout << triple.subject << " " << triple.predicate << " " << triple.object << std::endl;
        }
        std::cout << "=> " << rule.head.subject << " " << rule.head.predicate << " " << rule.head.object << std::endl;
    }
}

int main() {

    TestInfer();
    // TestDatalogParser();

    return 0;
}
