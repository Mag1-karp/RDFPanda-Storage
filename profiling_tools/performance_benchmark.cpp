#include <iostream>
#include <vector>
#include <string>
#include "InputParser.h"
#include "TripleStore.h"
#include "DatalogEngine.h"
#include "PerformanceProfiler.h"
#include "MemoryMonitor.h"

/**
 * 综合性能测试工具
 * 用于识别RDFPanda-Storage系统中的性能瓶颈
 */
class PerformanceBenchmark {
public:
    struct BenchmarkConfig {
        std::string dataFile;
        std::string ruleFile;
        std::string description;
    };

    void runFullBenchmark(const BenchmarkConfig& config) {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Performance Benchmark: " << config.description << std::endl;
        std::cout << "Data File: " << config.dataFile << std::endl;
        std::cout << "Rule File: " << config.ruleFile << std::endl;
        std::cout << std::string(80, '=') << std::endl;

        MemoryMonitor::printMemoryUsage("Initial");

        InputParser parser;
        TripleStore store;
        std::vector<Triple> triples;
        std::vector<Rule> rules;

        // 1. 测试解析性能
        {
            PROFILE_SCOPE("1. Parse Turtle File");
            MemoryMonitor::printMemoryUsage("Before Parsing");

            triples = parser.parseTurtle(config.dataFile);

            std::cout << "  Parsed " << triples.size() << " triples" << std::endl;
            MemoryMonitor::printMemoryUsage("After Parsing");
        }

        // 2. 测试三元组存储性能
        {
            PROFILE_SCOPE("2. Store Triples - Total");
            MemoryMonitor::printMemoryUsage("Before Storing");

            // 细分测试：批量vs逐个添加
            size_t batchSize = 1000;
            for (size_t i = 0; i < triples.size(); i += batchSize) {
                PROFILE_SCOPE("2a. Store Triples - Batch");
                size_t end = std::min(i + batchSize, triples.size());
                for (size_t j = i; j < end; ++j) {
                    store.addTriple(triples[j]);
                }
            }

            std::cout << "  Stored " << store.getTripleCount() << " triples" << std::endl;
            MemoryMonitor::printMemoryUsage("After Storing");
        }

        // 3. 测试规则解析
        {
            PROFILE_SCOPE("3. Parse Datalog Rules");
            rules = parser.parseDatalogFromFile(config.ruleFile);
            std::cout << "  Parsed " << rules.size() << " rules" << std::endl;
        }

        // 4. 测试索引构建 (Trie结构)
        {
            PROFILE_SCOPE("4. Index Building");
            MemoryMonitor::printMemoryUsage("Before Index Building");

            // Trie索引在TripleStore中自动构建
            // 这里可以添加显式的索引刷新调用（如果有的话）

            MemoryMonitor::printMemoryUsage("After Index Building");
        }

        // 5. 测试推理性能
        {
            PROFILE_SCOPE("5. Datalog Reasoning - Total");
            MemoryMonitor::printMemoryUsage("Before Reasoning");

            DatalogEngine engine(store, rules);

            {
                PROFILE_SCOPE("5a. Iterative Reasoning");
                engine.iterativeReason();
            }

            size_t finalTripleCount = engine.getTripleCount();
            std::cout << "  Final triple count: " << finalTripleCount << std::endl;
            std::cout << "  Inferred triples: " << (finalTripleCount - triples.size()) << std::endl;

            MemoryMonitor::printMemoryUsage("After Reasoning");
        }

        // 6. 测试查询性能
        benchmarkQueries(store);

        // 打印完整性能报告
        std::cout << "\n";
        GlobalProfiler::getInstance().printReport();

        MemoryMonitor::printMemoryUsage("Final");
    }

private:
    void benchmarkQueries(TripleStore& store) {
        PROFILE_SCOPE("6. Query Performance");

        // 测试不同类型的查询
        {
            PROFILE_SCOPE("6a. Query by Subject (1000x)");
            for (int i = 0; i < 1000; ++i) {
                auto results = store.queryBySubject("http://example.org/node1");
            }
        }

        {
            PROFILE_SCOPE("6b. Query by Predicate (1000x)");
            for (int i = 0; i < 1000; ++i) {
                auto results = store.queryByPredicate("http://dag.org#edge");
            }
        }

        {
            PROFILE_SCOPE("6c. Query by Object (1000x)");
            for (int i = 0; i < 1000; ++i) {
                auto results = store.queryByObject("http://example.org/node10");
            }
        }
    }
};

// 组件级别的微基准测试
void runMicroBenchmarks() {
    std::cout << "\n=== Micro Benchmarks ===" << std::endl;

    // 测试字符串池性能
    {
        PROFILE_SCOPE("StringPool - 10000 insertions");
        StringPool pool;
        for (int i = 0; i < 10000; ++i) {
            std::string str = "http://example.org/entity" + std::to_string(i);
            pool.intern(str);
        }
    }

    // 测试LRU缓存性能
    {
        PROFILE_SCOPE("LRU Cache - 10000 operations");
        LRUCache<std::string, int> cache(1000);
        for (int i = 0; i < 10000; ++i) {
            std::string key = "key" + std::to_string(i % 2000);
            cache.put(key, i);
        }
    }

    GlobalProfiler::getInstance().printReport();
}

int main() {
    std::cout << "RDFPanda-Storage Performance Analysis Tool" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    // 运行微基准测试
    runMicroBenchmarks();

    // 运行不同规模的数据集测试
    PerformanceBenchmark benchmark;

    // 小规模测试
    benchmark.runFullBenchmark({
        "input_examples/dag_n1k_e2k.ttl",
        "input_examples/DAG-R.dl",
        "Small Scale Test (1K nodes, 2K edges)"
    });

    GlobalProfiler::getInstance().reset();

    // 中等规模测试
    benchmark.runFullBenchmark({
        "input_examples/dag_n1k_e10k.ttl",
        "input_examples/DAG-R.dl",
        "Medium Scale Test (1K nodes, 10K edges)"
    });

    GlobalProfiler::getInstance().reset();

    // 大规模测试
    benchmark.runFullBenchmark({
        "input_examples/DAG_subset_10k.ttl",
        "input_examples/DAG-R.dl",
        "Large Scale Test (10K subset)"
    });

    return 0;
}
