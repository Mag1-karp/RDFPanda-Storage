#include "InputParser.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>

std::vector<Triple> InputParser::parseNTriples(const std::string& filename) {
    std::vector<Triple> triples;
    std::ifstream file(filename);
    // std::cout << "Parsing file: " << filename << std::endl;
    std::string line;
    std::regex tripleRegex(R"(<([^>]+)> <([^>]+)> (\"[^\"]*\"|<[^>]+>|_:.*) \.)");

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

    // 正则表达式匹配三元组
    // std::regex tripleRegex(R"((<[^>]+>|_:.*)\s+(<[^>]+>)\s+(\"[^\"]*\"|<[^>]+>|_:.*)\s*\.)");
    // std::regex tripleRegex(R"((<[^>]+>|_:.*|[^:]+:[^ ]+)\s+(<[^>]+>|[^:]+:[^ ]+)\s+(\"[^\"]*\"|<[^>]+>|_:.*)\s*\.)");
    std::regex tripleRegex(R"((<[^>]+>|_:.*|[^:]+:[^ ]+)\s+(<[^>]+>|[^:]+:[^ ]+)\s+(\"[^\"]*\"|<[^>]+>|_:.*|[^:]+:[^ ]+)\s*\.)");

    // 正则表达式匹配前缀声明
    std::regex prefixRegex(R"(@prefix\s+([^:]+):\s+<([^>]+)>\s*\.)");

    // 前缀映射表
    std::map<std::string, std::string> prefixMap;

    while (std::getline(file, line)) {
        // 去除行首尾空白字符
        line = std::regex_replace(line, std::regex(R"(^\s+|\s+$)"), "");

        // 跳过空行和注释
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // 匹配前缀声明
        std::smatch prefixMatch;
        if (std::regex_match(line, prefixMatch, prefixRegex)) {
            // std::cout << "Prefix declaration: " << prefixMatch[1].str() << " -> " << prefixMatch[2].str() << std::endl;
            std::string prefixName = prefixMatch[1].str();
            std::string prefixUri = prefixMatch[2].str();
            prefixMap[prefixName] = prefixUri;
            continue;
        }

        // 匹配三元组
        std::smatch tripleMatch;
        if (std::regex_match(line, tripleMatch, tripleRegex)) {
            // std::cout << "Triple: " << tripleMatch[0].str() << std::endl;
            std::string subject = tripleMatch[1].str();
            std::string predicate = tripleMatch[2].str();
            std::string object = tripleMatch[3].str();

            // 展开前缀（如果存在）
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

            subject = expandPrefix(subject);
            predicate = expandPrefix(predicate);
            object = expandPrefix(object);

            triples.emplace_back(subject, predicate, object);
        }
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