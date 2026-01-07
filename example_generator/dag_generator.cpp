#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>
#include <cstring>

class DAGGenerator {
private:
    int num_nodes;
    int num_edges;
    std::string output_file;
    std::mt19937 rng;
    
public:
    DAGGenerator(int nodes, int edges, const std::string& output) 
        : num_nodes(nodes), num_edges(edges), output_file(output), rng(std::random_device{}()) {
        
        // 验证参数有效性
        if (num_nodes <= 1) {
            throw std::invalid_argument("Number of nodes must be greater than 1");
        }
        
        int max_possible_edges = num_nodes * (num_nodes - 1) / 2;
        if (num_edges > max_possible_edges) {
            throw std::invalid_argument("Too many edges for given number of nodes. Max possible: " + std::to_string(max_possible_edges));
        }
        
        if (num_edges < 0) {
            throw std::invalid_argument("Number of edges cannot be negative");
        }
    }
    
    void generate() {
        std::vector<std::pair<int, int>> edges;
        
        // 计算平均出度
        double average_out_degree = (double)num_edges / num_nodes;
        
        int total_edges_generated = 0;
        
        // 遍历每个节点 (除了最后一个，因为它没有可连接的目标)
        for (int i = 0; i < num_nodes - 1; i++) {
            // 计算当前节点应该生成的出度
            int remaining_nodes = num_nodes - 1 - i; // 还有多少个节点需要处理
            int remaining_edges = num_edges - total_edges_generated; // 还需要生成多少条边
            
            // 如果没有剩余边需要生成，跳出循环
            if (remaining_edges <= 0) break;
            
            // 计算当前节点的出度：基础出度 + 随机调整
            int max_possible_from_current = num_nodes - 1 - i; // 当前节点最多能连接多少个节点
            double expected_out_degree = (double)remaining_edges / remaining_nodes;
            
            // 添加随机性：±30%的变化
            std::uniform_real_distribution<double> variation(0.7, 1.3);
            int out_degree = std::round(expected_out_degree * variation(rng));
            
            // 确保出度在合理范围内
            out_degree = std::max(0, std::min(out_degree, max_possible_from_current));
            out_degree = std::min(out_degree, remaining_edges); // 不能超过剩余需要生成的边数
            
            if (out_degree > 0) {
                generateEdgesFromNode(i, out_degree, edges);
                total_edges_generated += out_degree;
            }
        }
        
        // 如果还需要更多边，随机添加
        while (total_edges_generated < num_edges) {
            int src = std::uniform_int_distribution<int>(0, num_nodes - 2)(rng);
            int dst = std::uniform_int_distribution<int>(src + 1, num_nodes - 1)(rng);
            
            // 检查是否已存在这条边
            bool edge_exists = false;
            for (const auto& edge : edges) {
                if (edge.first == src && edge.second == dst) {
                    edge_exists = true;
                    break;
                }
            }
            
            if (!edge_exists) {
                edges.push_back({src, dst});
                total_edges_generated++;
            }
        }
        
        // 写入文件
        writeToFile(edges);
        
        // 输出统计信息
        printStatistics(edges);
    }
    
private:
    void generateEdgesFromNode(int source_node, int out_degree, std::vector<std::pair<int, int>>& edges) {
        // 生成所有可能的目标节点
        std::vector<int> possible_targets;
        for (int j = source_node + 1; j < num_nodes; j++) {
            possible_targets.push_back(j);
        }
        
        // 洗牌算法：随机重排
        std::shuffle(possible_targets.begin(), possible_targets.end(), rng);
        
        // 取前 out_degree 个作为连接目标
        int actual_out_degree = std::min(out_degree, (int)possible_targets.size());
        for (int k = 0; k < actual_out_degree; k++) {
            edges.push_back({source_node, possible_targets[k]});
        }
    }
    
    void writeToFile(const std::vector<std::pair<int, int>>& edges) {
        std::ofstream file(output_file);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open output file: " + output_file);
        }
        
        // 写入Turtle格式的前缀
        file << "@prefix p: <http://dag.org#> .\n\n";
        file << "# Generated DAG with " << num_nodes << " nodes and " << edges.size() << " edges\n";
        file << "# Each edge represents: source_node p:edge target_node\n\n";
        
        // 写入所有边
        for (const auto& edge : edges) {
            file << "p:N" << edge.first << " p:edge p:N" << edge.second << " .\n";
        }
        
        file.close();
        std::cout << "DAG generated successfully! Written to: " << output_file << std::endl;
    }
    
    void printStatistics(const std::vector<std::pair<int, int>>& edges) {
        std::cout << "\n=== DAG Statistics ===" << std::endl;
        std::cout << "Total nodes: " << num_nodes << std::endl;
        std::cout << "Total edges: " << edges.size() << std::endl;
        std::cout << "Average degree: " << (2.0 * edges.size()) / num_nodes << std::endl;
        
        // 计算出度分布
        std::vector<int> out_degrees(num_nodes, 0);
        std::vector<int> in_degrees(num_nodes, 0);
        
        for (const auto& edge : edges) {
            out_degrees[edge.first]++;
            in_degrees[edge.second]++;
        }
        
        // 找到最大和最小出度
        int max_out = *std::max_element(out_degrees.begin(), out_degrees.end());
        int min_out = *std::min_element(out_degrees.begin(), out_degrees.end());
        int max_in = *std::max_element(in_degrees.begin(), in_degrees.end());
        int min_in = *std::min_element(in_degrees.begin(), in_degrees.end());
        
        std::cout << "Out-degree range: [" << min_out << ", " << max_out << "]" << std::endl;
        std::cout << "In-degree range: [" << min_in << ", " << max_in << "]" << std::endl;
        
        // 验证DAG属性
        bool is_valid_dag = true;
        for (const auto& edge : edges) {
            if (edge.first >= edge.second) {
                is_valid_dag = false;
                break;
            }
        }
        
        std::cout << "DAG property verified: " << (is_valid_dag ? "Valid" : "Invalid") << std::endl;
    }
};

int main() {
    // 在这里修改参数
    int num_nodes = 5000;                    // 节点数量
    int num_edges = 30000;                   // 边的数量
    std::string output_file = "dag_n5k_e30k.ttl";  // 输出文件名
    
    try {
        std::cout << "Generating DAG with " << num_nodes << " nodes and " << num_edges << " edges..." << std::endl;
        std::cout << "Output file: " << output_file << std::endl << std::endl;
        
        DAGGenerator generator(num_nodes, num_edges, output_file);
        generator.generate();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}