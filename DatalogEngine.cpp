#include <algorithm>
#include <map>
#include <set>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <future>
#include "DatalogEngine.h"

#include <queue>

void DatalogEngine::initiateRulesMap() {
    // 建立规则关于规则体中各模式三元组的谓语ID的索引，方便迭代中用三元组触发规则的应用
    for (const auto& rule : rules) {
        for (const auto& triple : rule.body) {
            if (isVariable(triple.predicate())) {
                // 变量不作为索引
                continue;
            }
            uint32_t predicateId = triple.getPredicateId();

            if (rulesMap.find(predicateId) == rulesMap.end()) {
                // 如果当前谓语ID不在map中，则添加
                rulesMap[predicateId] = std::vector<std::pair<size_t, size_t>>();  // 规则下标，规则体中谓语下标
                // rulesMap[predicateId] = std::vector<size_t>();  // 规则下标
            }

            // 由于按照下标遍历，故每个vector中最后一个元素最大，只需检测是否小于当前规则下标即可防止重复
            // if (!rulesMap[predicateId].empty() && rulesMap[predicateId].back() >= &rule - &rules[0]) {
            //     continue;  // 已存在当前规则下标
            // }

            // 向map中谓语ID对应的规则下标列表中添加当前规则的下标以及该谓语在规则体中的下标
            rulesMap[predicateId].emplace_back(&rule - &rules[0], &triple - &rule.body[0]);
            // // 向map中谓语ID对应的规则下标列表中添加当前规则的下标
            // rulesMap[predicateId].emplace_back(&rule - &rules[0]);

        }
    }
}

void DatalogEngine::reason() {
    // bool newFactAdded = false;
    // int epoch = 0;

    // do {
    // std::cout << "Epoch: " << epoch++ << std::endl;
    // newFactAdded = false;
    std::queue<Triple> newFactQueue; // 存储新产生的事实，出队时触发对应规则的应用，并存到事实库中

    // 创建线程池
    std::vector<std::future<std::vector<Triple>>> futures;
    std::mutex storeMutex;

    std::atomic<int> reasonCount(0);

    // 先进行第一轮推理，初始时没有新事实，遍历规则逐条应用
    // int ruleId = 0;
    for (const auto& rule : rules) {
        // std::cout << "Applying rule: " << ruleId++ << std::endl;
        // 使用 std::async 异步执行规则
        reasonCount++;
        futures.push_back(std::async(std::launch::async, [&]() {
            std::vector<Triple> newFacts;
            std::map<std::string, uint32_t> bindingIds;  // 使用ID版本
            // 使用ID优化版本的推理方法
            leapfrogTriejoinOptimized(store.getTriePSORoot(), store.getTriePOSRoot(), rule, newFacts, bindingIds);
            // leapfrogTriejoin(store.getTriePSORoot(), store.getTriePOSRoot(), rule, newFacts, bindings);
            return newFacts;
        }));
    }

    // 收集线程结果并立即存储，确保推理依赖的正确性
    for (auto& future : futures) {
        std::vector<Triple> newFacts = future.get();
        
        // 立即存储所有新事实，确保后续推理能够找到依赖
        for (const auto& triple : newFacts) {
            std::lock_guard<std::mutex> lock(getShardMutex(triple.predicate()));
            // 只有成功添加的新三元组才加入推理队列
            if (store.addTriple(triple)) {
                // 添加到推理队列
                newFactQueue.push(triple);
            }
        }
    }


    std::atomic<int> activeTaskCount(0); // 活动任务计数器
    std::mutex queueMutex; // 保护队列的互斥锁

    std::atomic<bool> done(false);
    std::condition_variable cv;
    const auto threadCount = std::thread::hardware_concurrency();
    std::vector<std::thread> workers;

    // 工作线程
    auto worker = [&]() {
        while (true) {
            reasonCount++;
            Triple currentTriple("", "", "");
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                cv.wait(lock, [&] { return !newFactQueue.empty() || done; });
                if (done && newFactQueue.empty()) break;
                currentTriple = newFactQueue.front();
                newFactQueue.pop();
                activeTaskCount++; // 增加活动任务计数
                // reasonCount++; // 统计推理次数
                // if (newFactQueue.size() % 100 == 0) {
                //     std::cout << newFactQueue.size() << ' ' << reasonCount << std::endl;
                // }
            }

            // 处理 currentTriple，推理新事实并加锁入队
            // 增量优化：检查该三元组是否已经处理过
            uint64_t tripleHash = static_cast<uint64_t>(currentTriple.getSubjectId()) << 32 | 
                                 static_cast<uint64_t>(currentTriple.getPredicateId()) << 16 | 
                                 static_cast<uint64_t>(currentTriple.getObjectId());
            
            {
                std::lock_guard<std::mutex> lock(processedMutex);
                if (processedTriples.find(tripleHash) != processedTriples.end()) {
                    activeTaskCount--;
                    continue;  // 已处理过，跳过
                }
                processedTriples.insert(tripleHash);
            }
            // 根据rulesMap找到规则
            auto it = rulesMap.find(currentTriple.getPredicateId());
            if (it != rulesMap.end()) {
                for (const auto& rulePair : it->second) {
                    size_t ruleIdx = rulePair.first;
                    size_t patternIdx = rulePair.second;
                    const Rule& rule = rules[ruleIdx];
                    const Triple& pattern = rule.body[patternIdx];

                    // 绑定变量到ID
                    std::map<std::string, uint32_t> bindingIds;
                    if (isVariable(pattern.subject())) {
                        bindingIds[pattern.subject()] = currentTriple.getSubjectId();
                    }
                    if (isVariable(pattern.object())) {
                        bindingIds[pattern.object()] = currentTriple.getObjectId();
                    }

                    // 调用ID优化版本的推理方法
                    std::vector<Triple> inferredFacts;
                    leapfrogTriejoinOptimized(store.getTriePSORoot(), store.getTriePOSRoot(), rule, inferredFacts, bindingIds);
                    // leapfrogTriejoin(store.getTriePSORoot(), store.getTriePOSRoot(), rule, inferredFacts, bindingsPtr);
                    // reasonCount++;

                    // 先存储新事实，再加入队列
                    std::vector<Triple> newValidFacts;
                    
                    // 第一步：存储新事实  
                    for (const auto& fact : inferredFacts) {
                        std::lock_guard<std::mutex> storeLock(getShardMutex(fact.predicate()));
                        if (store.addTriple(fact)) {
                            // 标记为当前迭代的新事实
                            markTripleAsNewInCurrentIteration(fact);
                            newValidFacts.push_back(fact);
                        }
                    }
                    
                    // 第二步：加入推理队列
                    if (!newValidFacts.empty()) {
                        std::lock_guard<std::mutex> queueLock(queueMutex);
                        for (const auto& fact : newValidFacts) {
                            newFactQueue.push(fact);
                        }
                    }
                }
            }

            // 任务完成，减少活动任务计数
            activeTaskCount--;
        }
    };

    // 启动线程池
    workers.reserve(threadCount);
    for (int i = 0; i < threadCount; ++i) {
        workers.emplace_back(worker);
    }

    // 主线程不断唤醒工作线程
    while (true) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (newFactQueue.empty() && activeTaskCount == 0) {
                break;
            }
        }
        cv.notify_all();
        std::this_thread::yield();
    }

    // 结束所有线程
    done = true;
    cv.notify_all();
    for (auto& t : workers) t.join();


    // 输出推理完成后的事实库大小
    std::cout << "Total triples in store:           " << store.getTripleCount() << std::endl;
    // 输出总共推理的次数
    std::cout << "Total reasoning count:            " << reasonCount.load() << std::endl;
}

void DatalogEngine::iterativeReason() {
    bool newFactAdded = false;
    int epoch = 0;

    do {
        // std::cout << "Epoch: " << epoch++ << std::endl;
        newFactAdded = false;
        std::queue<Triple> newFactQueue;

        // 创建线程池
        std::vector<std::future<std::vector<Triple>>> futures;
        std::mutex storeMutex;

        std::atomic<int> reasonCount(0);

        // 遍历规则逐条应用
        for (const auto& rule : rules) {
            futures.push_back(std::async(std::launch::async, [&]() {
                std::vector<Triple> newFacts;
                std::map<std::string, uint32_t> bindingIds;
                // leapfrogTriejoinOptimized(store.getTriePSORoot(), store.getTriePOSRoot(), rule, newFacts, bindingIds);
                iterativeLeapfrogTriejoin(store.getTriePSORoot(), store.getTriePOSRoot(), rule, newFacts, bindingIds);
                return newFacts;
            }));
        }

        // 收集线程结果并立即存储
        for (auto& future : futures) {
            std::vector<Triple> newFacts = future.get();

            for (const auto& triple : newFacts) {
                std::lock_guard<std::mutex> lock(getShardMutex(triple.predicate()));
                if (store.addTriple(triple)) {
                    newFactQueue.push(triple);
                    newFactAdded = true;
                }
            }
        }

        // 并行化处理增量推理
        if (!newFactQueue.empty()) {
            std::atomic<bool> done(false);
            std::atomic<int> activeTaskCount(0);
            std::condition_variable cv;
            std::mutex queueMutex;
            const auto threadCount = std::thread::hardware_concurrency();
            std::vector<std::thread> workers;

            // 工作线程函数
            auto worker = [&]() {
                while (true) {
                    Triple currentTriple("", "", "");
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        cv.wait(lock, [&] { return !newFactQueue.empty() || done; });
                        if (done && newFactQueue.empty()) break;
                        currentTriple = newFactQueue.front();
                        newFactQueue.pop();
                        activeTaskCount++;
                    }

                    // 根据rulesMap找到相关规则
                    auto it = rulesMap.find(currentTriple.getPredicateId());
                    if (it != rulesMap.end()) {
                        for (const auto& rulePair : it->second) {
                            size_t ruleIdx = rulePair.first;
                            size_t patternIdx = rulePair.second;
                            const Rule& rule = rules[ruleIdx];
                            const Triple& pattern = rule.body[patternIdx];

                            // 绑定变量到ID
                            std::map<std::string, uint32_t> bindingIds;
                            if (isVariable(pattern.subject())) {
                                bindingIds[pattern.subject()] = currentTriple.getSubjectId();
                            }
                            if (isVariable(pattern.object())) {
                                bindingIds[pattern.object()] = currentTriple.getObjectId();
                            }

                            // 调用推理方法
                            std::vector<Triple> inferredFacts;
                            iterativeLeapfrogTriejoin(store.getTriePSORoot(), store.getTriePOSRoot(), rule, inferredFacts, bindingIds);

                            // 先存储新事实，再加入队列
                            std::vector<Triple> newValidFacts;
                            
                            // 第一步：存储新事实
                            for (const auto& fact : inferredFacts) {
                                std::lock_guard<std::mutex> storeLock(getShardMutex(fact.predicate()));
                                if (store.addTriple(fact)) {
                                    newValidFacts.push_back(fact);
                                    newFactAdded = true;
                                }
                            }
                            
                            // 第二步：加入推理队列
                            if (!newValidFacts.empty()) {
                                std::lock_guard<std::mutex> queueLock(queueMutex);
                                for (const auto& fact : newValidFacts) {
                                    newFactQueue.push(fact);
                                }
                            }
                        }
                    }

                    // 任务完成，减少活动任务计数
                    activeTaskCount--;
                }
            };

            // 启动工作线程池
            workers.reserve(threadCount);
            for (int i = 0; i < threadCount; ++i) {
                workers.emplace_back(worker);
            }

            // 主线程监控并唤醒工作线程
            while (true) {
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    if (newFactQueue.empty() && activeTaskCount == 0) {
                        break;
                    }
                }
                cv.notify_all();
                std::this_thread::yield();
            }

            // 结束所有线程
            done = true;
            cv.notify_all();
            for (auto& t : workers) {
                t.join();
            }
        }

    } while (newFactAdded);

    std::cout << "Total triples in store: " << store.getTripleCount() << std::endl;
}

bool DatalogEngine::isVariable(const std::string& term) {
    // 判断是否为变量，变量以?开头，如"?x"
    return !term.empty() && term[0] == '?';
}

void DatalogEngine::leapfrogTriejoin(
    TrieNode* psoRoot, TrieNode* posRoot,
    const Rule& rule,
    std::vector<Triple>& newFacts,
    std::map<std::string, std::string>& bindings
) {

    // std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    std::set<std::string> variables;
    std::map<std::string, std::vector<std::pair<int, int>>> varPositions; // 变量 -> [(triple_idx, position)]
    // todo: 能不能根据varPositions来筛选代入新三元组对应变量后可能产生冲突的三元组模式？需要找出主语和宾语变量都包含在新三元组对应模式中的三元组模式
    // todo: 例如新三元组对应模式为A(?x,?y)，则需要找其他(?x,?y)、(?y,?x)、(?x,?x)、(?y,?y)的模式，并查询代入新值后的三元组是否存在于事实库中
    // todo: 相当于对于两个[(idx, pos)]数组，找出所有idx，使(idx, 0)和(idx, 2)都存在
    // update: 已实现

    for (int i = 0; i < rule.body.size(); i++) {
        const Triple& triple = rule.body[i];
        if (isVariable(triple.subject())) {
            variables.insert(triple.subject());
            varPositions[triple.subject()].emplace_back(i, 0); // 0 表示主语位置
        }
        if (isVariable(triple.predicate())) {
            variables.insert(triple.predicate());
            varPositions[triple.predicate()].emplace_back(i, 1); // 1 表示谓语位置
            // 实际基本不考虑谓语为变量的情况，但以防万一还是加上
        }
        if (isVariable(triple.object())) {
            variables.insert(triple.object());
            varPositions[triple.object()].emplace_back(i, 2); // 2 表示宾语位置
        }
    }

    if (!checkConflictingTriples(bindings, varPositions, rule)) {
        return;
    }

    // std::map<std::string, std::string> bindings;
    // 对每个变量进行leapfrog join，使用优化的变量顺序
    join_by_variable(psoRoot, posRoot, rule, variables, varPositions, bindings, 0, newFacts);

    // std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    // std::chrono::duration<double> elapsed = end - start;
    // std::cout << elapsed.count() << std::endl;
}

void DatalogEngine::join_by_variable(
    TrieNode* psoRoot, TrieNode* posRoot,
    const Rule& rule,  // 当前规则
    const std::set<std::string>& variables,  // 当前规则的变量全集
    const std::map<std::string, std::vector<std::pair<int, int>>>& varPositions,  // 变量 -> [(变量所在三元组模式在规则体中的下标, 主0/谓1/宾2)]
    std::map<std::string, std::string>& bindings,  // 变量 -> 变量当前的绑定值（常量，未绑定则为空）
    int varIdx,
    std::vector<Triple>& newFacts
) {
    // 当所有变量都已绑定时，生成新的事实
    if (varIdx >= variables.size()) {
        std::string newSubject = substituteVariable(rule.head.subject(), bindings);
        std::string newPredicate = substituteVariable(rule.head.predicate(), bindings);
        std::string newObject = substituteVariable(rule.head.object(), bindings);

        newFacts.emplace_back(newSubject, newPredicate, newObject);
        return;
    }

    // 优化：在每次递归调用时重新计算变量选择性
    auto selectivities = computeVariableSelectivity(rule, variables, varPositions, bindings);
    
    // 如果没有未绑定的变量，结束递归
    if (selectivities.empty()) {
        std::string newSubject = substituteVariable(rule.head.subject(), bindings);
        std::string newPredicate = substituteVariable(rule.head.predicate(), bindings);
        std::string newObject = substituteVariable(rule.head.object(), bindings);
        newFacts.emplace_back(newSubject, newPredicate, newObject);
        return;
    }
    
    // 选择选择性最高的变量（候选值最少）
    std::string currentVar = selectivities[0].variable;

    // 对当前变量创建迭代器
    std::vector<TrieIterator*> iterators;
    for (const auto& pos : varPositions.at(currentVar)) {
        int tripleIdx = pos.first;
        int position = pos.second;
        const Triple& triple = rule.body[tripleIdx];

        TrieIterator* it = nullptr;

        // 根据变量位置选择适当的Trie
        if (position == 0) { // 主语位置
            // 如果宾语已绑定，就从posTrie中对应宾语的子节点中查找
            if (!isVariable(triple.object()) || bindings.find(triple.object()) != bindings.end()) {
                it = new TrieIterator(posRoot);
                // 对谓语进行seek (使用ID)
                uint32_t predId = substituteVariableToId(triple.predicate(), bindings);
                it->seek(predId);
                if (!it->atEnd() && it->key() == predId) {
                    // 对已确定的宾语进行seek (使用ID)
                    TrieIterator objIt = it->open();
                    uint32_t objId = substituteVariableToId(triple.object(), bindings);
                    objIt.seek(objId);
                    if (!objIt.atEnd() && objIt.key() == objId) {
                        iterators.push_back(new TrieIterator(objIt.open()));
                    }
                }
            }
            // 否则，从psoTrie中查找
            else {
                it = new TrieIterator(psoRoot);
                // 对谓语进行seek (使用ID)
                uint32_t predId = substituteVariableToId(triple.predicate(), bindings);
                it->seek(predId);
                if (!it->atEnd() && it->key() == predId) {
                    TrieIterator subIt = it->open();
                    iterators.push_back(new TrieIterator(subIt));
                }
            }
        }
        // 这里暂时不考虑谓语为变量，即position == 1的情况
        else if (position == 2) { // 宾语位置
            // 如果主语已绑定，就从psoTrie中对应主语的子节点中查找
            if (!isVariable(triple.subject()) || bindings.find(triple.subject()) != bindings.end()) {
                it = new TrieIterator(psoRoot);
                // 对谓语进行seek (使用ID)
                uint32_t predId = substituteVariableToId(triple.predicate(), bindings);
                it->seek(predId);
                if (!it->atEnd() && it->key() == predId) {
                    // 对已确定的主语进行seek (使用ID)
                    TrieIterator subjIt = it->open();
                    uint32_t subjId = substituteVariableToId(triple.subject(), bindings);
                    subjIt.seek(subjId);
                    if (!subjIt.atEnd() && subjIt.key() == subjId) {
                        iterators.push_back(new TrieIterator(subjIt.open()));
                    }
                }
            }
            // 否则，从posTrie中查找
            else {
                it = new TrieIterator(posRoot);
                // 对谓语进行seek (使用ID)
                uint32_t predId = substituteVariableToId(triple.predicate(), bindings);
                it->seek(predId);
                if (!it->atEnd() && it->key() == predId) {
                    TrieIterator objIt = it->open();
                    iterators.push_back(new TrieIterator(objIt));
                }
            }
        }

        if (it && iterators.empty()) {
            delete it;
        }
    }

    // 对当前变量执行leapfrog join
    if (!iterators.empty()) {
        LeapfrogJoin lf(iterators);
        while (!lf.atEnd()) {
            uint32_t keyId = lf.key();
            // 将ID转换为字符串进行绑定
            std::string key = store.getStringPool().getString(keyId);
            bindings[currentVar] = key;

            // 递归处理下一个变量（不需要varIdx+1，因为我们动态选择变量）
            join_by_variable(psoRoot, posRoot, rule, variables, varPositions, bindings, 0, newFacts);

            lf.next();
        }

        // 清理迭代器
        for (auto it : iterators) {
            delete it;
        }
    }

    // 删除当前变量的绑定
    bindings.erase(currentVar);
}

// 辅助函数：若绑定中存在变量则替换其绑定的值，否则返回原字符串（此时为常量）
std::string DatalogEngine::substituteVariable(const std::string& term, const std::map<std::string, std::string>& bindings) {
    if (isVariable(term) && bindings.find(term) != bindings.end()) {
        return bindings.at(term);
    }
    return term;
}

// 计算变量的选择性，用于优化join顺序
std::vector<DatalogEngine::VariableSelectivity> DatalogEngine::computeVariableSelectivity(
    const Rule& rule,
    const std::set<std::string>& variables,
    const std::map<std::string, std::vector<std::pair<int, int>>>& varPositions,
    const std::map<std::string, std::string>& bindings
) const {
    std::vector<VariableSelectivity> selectivities;
    
    for (const std::string& var : variables) {
        if (bindings.find(var) != bindings.end()) {
            continue;  // 已绑定的变量跳过
        }
        
        VariableSelectivity vs;
        vs.variable = var;
        vs.candidateCount = 0;
        
        // 估算候选值数量
        const auto& positions = varPositions.at(var);
        size_t minCandidates = SIZE_MAX;
        
        for (const auto& pos : positions) {
            int tripleIdx = pos.first;
            int position = pos.second;
            const Triple& triple = rule.body[tripleIdx];
            
            size_t candidates = 0;
            
            if (position == 0) {  // 主语位置
                // 估算：根据谓语获取主语候选数
                uint32_t predId = substituteVariableToId(triple.predicate(), bindings);
                candidates = store.queryTripleIdsByPredicateId(predId).size();
            } else if (position == 2) {  // 宾语位置  
                // 估算：根据谓语获取宾语候选数
                uint32_t predId = substituteVariableToId(triple.predicate(), bindings);
                candidates = store.queryTripleIdsByPredicateId(predId).size();
            }
            
            if (candidates > 0 && candidates < minCandidates) {
                minCandidates = candidates;
            }
        }
        
        vs.candidateCount = (minCandidates == SIZE_MAX) ? 1000000 : minCandidates;
        vs.selectivity = 1.0 / (vs.candidateCount + 1);  // 避免除零
        
        selectivities.push_back(vs);
    }
    
    // 按选择性排序，选择性高的变量优先
    std::sort(selectivities.begin(), selectivities.end());
    
    return selectivities;
}

// 基于迭代器的变量连接实现
void DatalogEngine::join_by_variable_iterative(
    TrieNode* psoRoot, TrieNode* posRoot,
    const Rule& rule,
    std::vector<Triple>& newFacts,
    std::map<std::string, uint32_t>& bindingIds
) {
    std::set<std::string> variables;
    std::map<std::string, std::vector<std::pair<int, int>>> varPositions;
    
    for (size_t i = 0; i < rule.body.size(); i++) {
        const Triple& triple = rule.body[i];
        if (isVariable(triple.subject())) {
            variables.insert(triple.subject());
            varPositions[triple.subject()].push_back({i, 0});
        }
        if (isVariable(triple.predicate())) {
            variables.insert(triple.predicate());
            varPositions[triple.predicate()].push_back({i, 1});
        }
        if (isVariable(triple.object())) {
            variables.insert(triple.object());
            varPositions[triple.object()].push_back({i, 2});
        }
    }

    auto selectivities = computeVariableSelectivityOptimized(rule, variables, varPositions, bindingIds);
    
    if (selectivities.empty()) {
        uint32_t newSubjectId = substituteVariableId(rule.head.subject(), bindingIds);
        uint32_t newPredicateId = substituteVariableId(rule.head.predicate(), bindingIds);
        uint32_t newObjectId = substituteVariableId(rule.head.object(), bindingIds);
        newFacts.emplace_back(newSubjectId, newPredicateId, newObjectId);
        return;
    }

    std::vector<std::string> orderedVariables;
    for (const auto& sel : selectivities) {
        orderedVariables.push_back(sel.variable);
    }

    VariableJoinIterator iterator(psoRoot, posRoot, rule, orderedVariables, varPositions, bindingIds, this);
    
    while (!iterator.atEnd()) {
        auto currentBindings = iterator.getCurrentBindings();
        uint32_t newSubjectId = substituteVariableId(rule.head.subject(), currentBindings);
        uint32_t newPredicateId = substituteVariableId(rule.head.predicate(), currentBindings);
        uint32_t newObjectId = substituteVariableId(rule.head.object(), currentBindings);
        newFacts.emplace_back(newSubjectId, newPredicateId, newObjectId);
        
        if (!iterator.next()) {
            break;
        }
    }
}

DatalogEngine::VariableJoinIterator::VariableJoinIterator(
    TrieNode* psoRoot, TrieNode* posRoot, const Rule& rule,
    const std::vector<std::string>& orderedVars,
    const std::map<std::string, std::vector<std::pair<int, int>>>& varPos,
    const std::map<std::string, uint32_t>& initialBindings,
    DatalogEngine* eng
) : psoRoot(psoRoot), posRoot(posRoot), rule(rule), orderedVariables(orderedVars), 
    varPositions(varPos), bindingIds(initialBindings), engine(eng), finished(false) {
    
    variableStates.resize(orderedVariables.size());
    for (size_t i = 0; i < orderedVariables.size(); ++i) {
        variableStates[i].variable = orderedVariables[i];
        variableStates[i].currentIteratorIndex = 0;
        variableStates[i].currentValue = 0;
        variableStates[i].atEnd = true;
    }
    
    for (size_t i = 0; i < orderedVariables.size(); ++i) {
        initializeVariableIterators(i);
        if (variableStates[i].atEnd) {
            finished = true;
            return;
        }
        bindingIds[variableStates[i].variable] = variableStates[i].currentValue;
    }
}

DatalogEngine::VariableJoinIterator::~VariableJoinIterator() {
    for (auto& varState : variableStates) {
        for (auto* iter : varState.iterators) {
            delete iter;
        }
    }
}

void DatalogEngine::VariableJoinIterator::initializeVariableIterators(size_t varIndex) {
    if (varIndex >= variableStates.size()) return;
    
    VariableState& varState = variableStates[varIndex];
    const std::string& var = varState.variable;
    
    for (auto* iter : varState.iterators) delete iter;
    varState.iterators.clear();
    
    const auto& positions = varPositions.at(var);
    for (const auto& pos : positions) {
        int tripleIdx = pos.first;
        int position = pos.second;
        const Triple& triple = rule.body[tripleIdx];
        
        TrieIterator* it = nullptr;
        
        if (position == 0) {
            if (!isVariable(triple.object()) || bindingIds.find(triple.object()) != bindingIds.end()) {
                it = new TrieIterator(posRoot);
                uint32_t predId = engine->substituteVariableId(triple.predicate(), bindingIds);
                it->seek(predId);
                if (!it->atEnd() && it->key() == predId) {
                    TrieIterator objIt = it->open();
                    uint32_t objId = engine->substituteVariableId(triple.object(), bindingIds);
                    objIt.seek(objId);
                    if (!objIt.atEnd() && objIt.key() == objId) {
                        varState.iterators.push_back(new TrieIterator(objIt.open()));
                    }
                }
                delete it;
            } else {
                it = new TrieIterator(psoRoot);
                uint32_t predId = engine->substituteVariableId(triple.predicate(), bindingIds);
                it->seek(predId);
                if (!it->atEnd() && it->key() == predId) {
                    TrieIterator subIt = it->open();
                    varState.iterators.push_back(new TrieIterator(subIt));
                }
                delete it;
            }
        } else if (position == 2) {
            if (!isVariable(triple.subject()) || bindingIds.find(triple.subject()) != bindingIds.end()) {
                it = new TrieIterator(psoRoot);
                uint32_t predId = engine->substituteVariableId(triple.predicate(), bindingIds);
                it->seek(predId);
                if (!it->atEnd() && it->key() == predId) {
                    TrieIterator subjIt = it->open();
                    uint32_t subjId = engine->substituteVariableId(triple.subject(), bindingIds);
                    subjIt.seek(subjId);
                    if (!subjIt.atEnd() && subjIt.key() == subjId) {
                        varState.iterators.push_back(new TrieIterator(subjIt.open()));
                    }
                }
                delete it;
            } else {
                it = new TrieIterator(posRoot);
                uint32_t predId = engine->substituteVariableId(triple.predicate(), bindingIds);
                it->seek(predId);
                if (!it->atEnd() && it->key() == predId) {
                    TrieIterator objIt = it->open();
                    varState.iterators.push_back(new TrieIterator(objIt));
                }
                delete it;
            }
        }
    }
    
    varState.currentIteratorIndex = 0;
    varState.atEnd = varState.iterators.empty();
    
    if (!varState.atEnd) {
        if (varState.iterators.size() == 1) {
            if (!varState.iterators[0]->atEnd()) {
                varState.currentValue = varState.iterators[0]->key();
            } else {
                varState.atEnd = true;
            }
        } else {
            LeapfrogJoin lf(varState.iterators);
            if (!lf.atEnd()) {
                varState.currentValue = lf.key();
            } else {
                varState.atEnd = true;
            }
        }
    }
}

bool DatalogEngine::VariableJoinIterator::advanceVariable(size_t varIndex) {
    if (varIndex >= variableStates.size()) return false;
    
    VariableState& varState = variableStates[varIndex];
    if (varState.atEnd || varState.iterators.empty()) return false;
    
    if (varState.iterators.size() == 1) {
        varState.iterators[0]->next();
        if (varState.iterators[0]->atEnd()) {
            varState.atEnd = true;
            return false;
        }
        varState.currentValue = varState.iterators[0]->key();
        return true;
    } else {
        LeapfrogJoin lf(varState.iterators);
        lf.next();
        if (lf.atEnd()) {
            varState.atEnd = true;
            return false;
        }
        varState.currentValue = lf.key();
        return true;
    }
}

bool DatalogEngine::VariableJoinIterator::next() {
    if (finished) return false;
    
    for (int varIndex = variableStates.size() - 1; varIndex >= 0; --varIndex) {
        if (advanceVariable(varIndex)) {
            bindingIds[variableStates[varIndex].variable] = variableStates[varIndex].currentValue;
            
            for (size_t i = varIndex + 1; i < variableStates.size(); ++i) {
                initializeVariableIterators(i);
                if (variableStates[i].atEnd) break;
                bindingIds[variableStates[i].variable] = variableStates[i].currentValue;
            }
            
            bool allValid = true;
            for (size_t i = varIndex + 1; i < variableStates.size(); ++i) {
                if (variableStates[i].atEnd) {
                    allValid = false;
                    break;
                }
            }
            
            if (allValid) return true;
        } else {
            variableStates[varIndex].atEnd = true;
            bindingIds.erase(variableStates[varIndex].variable);
        }
    }
    
    finished = true;
    return false;
}

void DatalogEngine::iterativeLeapfrogTriejoin(
    TrieNode* psoRoot, TrieNode* posRoot,
    const Rule& rule,
    std::vector<Triple>& newFacts,
    std::map<std::string, uint32_t>& bindingIds
) {
    join_by_variable_iterative(psoRoot, posRoot, rule, newFacts, bindingIds);
}

// ID优化版本的leapfrogTriejoin
void DatalogEngine::leapfrogTriejoinOptimized(
    TrieNode* psoRoot, TrieNode* posRoot,
    const Rule& rule,
    std::vector<Triple>& newFacts,
    std::map<std::string, uint32_t>& bindingIds
) {
    std::set<std::string> variables;
    std::map<std::string, std::vector<std::pair<int, int>>> varPositions;

    for (int i = 0; i < rule.body.size(); i++) {
        const Triple& triple = rule.body[i];
        if (isVariable(triple.subject())) {
            variables.insert(triple.subject());
            varPositions[triple.subject()].emplace_back(i, 0);
        }
        if (isVariable(triple.predicate())) {
            variables.insert(triple.predicate());
            varPositions[triple.predicate()].emplace_back(i, 1);
        }
        if (isVariable(triple.object())) {
            variables.insert(triple.object());
            varPositions[triple.object()].emplace_back(i, 2);
        }
    }

    // 创建ID版本的冲突检查
    auto checkConflictingTriplesOptimized = [&](const std::map<std::string, uint32_t>& ids) -> bool {
        for (const auto& [var, id] : ids) {
            if (varPositions.find(var) == varPositions.end()) {
                continue;
            }

            const auto& positions = varPositions.at(var);
            std::map<int, std::set<int>> idxToPos;

            for (const auto& [idx, pos] : positions) {
                idxToPos[idx].insert(pos);
            }

            for (const auto& [idx, posSet] : idxToPos) {
                if (posSet.count(0) && posSet.count(2)) {
                    const Triple& pattern = rule.body[idx];
                    uint32_t subjectId = substituteVariableId(pattern.subject(), ids);
                    uint32_t predicateId = substituteVariableId(pattern.predicate(), ids);
                    uint32_t objectId = substituteVariableId(pattern.object(), ids);

                    Triple actualTriple(subjectId, predicateId, objectId);

                    if (store.getNodeByTriple(actualTriple) != nullptr) {
                        return false;
                    }
                }
            }
        }

        for (const auto& triple : rule.body) {
            if (!isVariable(triple.subject()) && !isVariable(triple.predicate()) && !isVariable(triple.object())) {
                Triple actualTriple(triple.subject(), triple.predicate(), triple.object());

                if (store.getNodeByTriple(actualTriple) != nullptr) {
                    return false;
                }
            }
        }

        return true;
    };

    if (!checkConflictingTriplesOptimized(bindingIds)) {
        return;
    }

    join_by_variable_optimized(psoRoot, posRoot, rule, variables, varPositions, bindingIds, 0, newFacts);
    // iterativeLeapfrogTriejoin(psoRoot, posRoot, rule, newFacts, bindingIds);
}

uint32_t DatalogEngine::getIdFromString(const std::string& str) const {
    {
        std::lock_guard<std::mutex> lock(cacheAccessMutex);
        auto it = stringToIdCache.find(str);
        if (it != stringToIdCache.end()) {
            return it->second;
        }
    }
    
    uint32_t id = store.getStringPool().getId(str);
    
    {
        std::lock_guard<std::mutex> lock(cacheAccessMutex);
        // 限制缓存大小，避免内存无限增长
        if (stringToIdCache.size() < 100000) {
            stringToIdCache[str] = id;
        }
    }
    
    return id;
}

// Semi-Naive评估相关方法实现
bool DatalogEngine::isTripleNewInCurrentIteration(const Triple& triple) const {
    uint64_t tripleHash = static_cast<uint64_t>(triple.getSubjectId()) << 32 | 
                         static_cast<uint64_t>(triple.getPredicateId()) << 16 | 
                         static_cast<uint64_t>(triple.getObjectId());
    
    std::lock_guard<std::mutex> lock(newFactsMutex);
    return newFactsInPreviousIteration.find(tripleHash) != newFactsInPreviousIteration.end();
}

void DatalogEngine::markTripleAsNewInCurrentIteration(const Triple& triple) {
    uint64_t tripleHash = static_cast<uint64_t>(triple.getSubjectId()) << 32 | 
                         static_cast<uint64_t>(triple.getPredicateId()) << 16 | 
                         static_cast<uint64_t>(triple.getObjectId());
    
    std::lock_guard<std::mutex> lock(newFactsMutex);
    newFactsInCurrentIteration.insert(tripleHash);
}

void DatalogEngine::switchToNextIteration() {
    std::lock_guard<std::mutex> lock(newFactsMutex);
    newFactsInPreviousIteration = std::move(newFactsInCurrentIteration);
    newFactsInCurrentIteration.clear();
}

// 新增：替换变量并返回ID
uint32_t DatalogEngine::substituteVariableToId(const std::string& term, const std::map<std::string, std::string>& bindings) const {
    std::string value = substituteVariable(term, bindings);
    return getIdFromString(value);
}

bool DatalogEngine::checkConflictingTriples(
    const std::map<std::string, std::string>& bindings,
    const std::map<std::string, std::vector<std::pair<int, int>>>& varPositions,
    const Rule& rule
) const {
    // 遍历bindings中的变量
    for (const auto& [var, value] : bindings) {
        if (varPositions.find(var) == varPositions.end()) {
            continue; // 如果变量不在varPositions中，跳过
        }

        const auto& positions = varPositions.at(var);
        std::map<int, std::set<int>> idxToPos;

        // 构建idx到pos的映射
        for (const auto& [idx, pos] : positions) {
            idxToPos[idx].insert(pos);
        }

        // 检查是否同时存在(idx, 0)和(idx, 2)
        for (const auto& [idx, posSet] : idxToPos) {
            if (posSet.count(0) && posSet.count(2)) {
                // 构造实际的三元组
                const Triple& pattern = rule.body[idx];
                std::string subject = substituteVariable(pattern.subject(), bindings);
                std::string predicate = substituteVariable(pattern.predicate(), bindings);
                std::string object = substituteVariable(pattern.object(), bindings);

                Triple actualTriple(subject, predicate, object);

                // 检查三元组是否存在于事实库中
                if (store.getNodeByTriple(actualTriple) != nullptr) {
                    return false;
                }
            }
        }
    }

    // 检查规则体中是否有只含常量不含变量的三元组模式
    for (const auto& triple : rule.body) {
        if (!isVariable(triple.subject()) && !isVariable(triple.predicate()) && !isVariable(triple.object())) {
            // 构造实际的三元组
            Triple actualTriple(triple.subject(), triple.predicate(), triple.object());

            // 检查三元组是否存在于事实库中
            if (store.getNodeByTriple(actualTriple) != nullptr) {
                return false;
            }
        }
    }

    return true;
}

// 优化的存在性检查方法，使用Triple ID哈希缓存
bool DatalogEngine::tripleExists(const Triple& triple) {
    // 计算Triple ID的哈希值作为缓存key
    uint64_t key = static_cast<uint64_t>(triple.getSubjectId()) << 32 | 
                   static_cast<uint64_t>(triple.getPredicateId()) << 16 | 
                   static_cast<uint64_t>(triple.getObjectId());
    
    bool exists;
    if (tripleExistenceCache.get(key, exists)) {
        return exists;
    }
    
    // 直接查询数据库
    exists = (store.getNodeByTriple(triple) != nullptr);
    tripleExistenceCache.put(key, exists);
    
    return exists;
}

// 批处理方法（已移除以确保推理正确性）
// void DatalogEngine::processBatch(std::vector<Triple>& batch) {
//     ... 已移除
// }


// 分片锁相关方法
size_t DatalogEngine::getShardIndex(const std::string& predicate) const {
    std::hash<std::string> hasher;
    return hasher(predicate) % SHARD_COUNT;
}

std::mutex& DatalogEngine::getShardMutex(const std::string& predicate) {
    size_t index = getShardIndex(predicate);
    return shardMutexes[index];
}

// 对象池相关方法
std::vector<Triple>* DatalogEngine::getTripleVector() {
    std::lock_guard<std::mutex> lock(poolMutex);
    if (!tripleVectorPool.empty()) {
        std::vector<Triple>* vec = tripleVectorPool.back();
        tripleVectorPool.pop_back();
        vec->clear();
        return vec;
    }
    return new std::vector<Triple>();
}

void DatalogEngine::returnTripleVector(std::vector<Triple>* vec) {
    if (vec) {
        std::lock_guard<std::mutex> lock(poolMutex);
        if (tripleVectorPool.size() < 50) {
            vec->clear();
            tripleVectorPool.push_back(vec);
        } else {
            delete vec;
        }
    }
}

std::map<std::string, std::string>* DatalogEngine::getBindingMap() {
    std::lock_guard<std::mutex> lock(poolMutex);
    if (!bindingMapPool.empty()) {
        std::map<std::string, std::string>* map = bindingMapPool.back();
        bindingMapPool.pop_back();
        map->clear();
        return map;
    }
    return new std::map<std::string, std::string>();
}

void DatalogEngine::returnBindingMap(std::map<std::string, std::string>* map) {
    if (map) {
        std::lock_guard<std::mutex> lock(poolMutex);
        if (bindingMapPool.size() < 50) {
            map->clear();
            bindingMapPool.push_back(map);
        } else {
            delete map;
        }
    }
}
/*
// 迭代器式推理实现
void DatalogEngine::iterativeLeapfrogTriejoin(
    TrieNode* psoRoot, TrieNode* posRoot,
    const Rule& rule,
    std::vector<Triple>& newFacts,
    std::map<std::string, uint32_t>& bindingIds
) {
    // 收集变量和位置信息
    std::set<std::string> variables;
    std::map<std::string, std::vector<std::pair<int, int>>> varPositions;

    // 收集规则体中的变量及其位置
    for (int i = 0; i < rule.body.size(); i++) {
        const Triple& triple = rule.body[i];
        if (isVariable(triple.subject())) {
            variables.insert(triple.subject());
            varPositions[triple.subject()].emplace_back(i, 0);
        }
        if (isVariable(triple.predicate())) {
            variables.insert(triple.predicate());
            varPositions[triple.predicate()].emplace_back(i, 1);
        }
        if (isVariable(triple.object())) {
            variables.insert(triple.object());
            varPositions[triple.object()].emplace_back(i, 2);
        }
    }

    // if (!checkConflictingTriples(bindingIds, varPositions, rule)) {
    //     return;
    // }

    // 计算变量选择性并排序
    auto selectivities = computeVariableSelectivityOptimized(rule, variables, varPositions, bindingIds);

    // 按选择性排序变量
    std::vector<std::string> orderedVars;
    for (const auto& vs : selectivities) {
        orderedVars.push_back(vs.variable);
    }

    // 使用 LeapfrogJoin 替代嵌套循环
    std::vector<TrieIterator*> iterators;
    for (const auto& var : orderedVars) {
        for (const auto& pos : varPositions[var]) {
            int tripleIdx = pos.first;
            int position = pos.second;
            const Triple& triple = rule.body[tripleIdx];

            TrieIterator* it = nullptr;
            if (position == 0) { // 主语位置
                it = new TrieIterator(psoRoot);
            } else if (position == 2) { // 宾语位置
                it = new TrieIterator(posRoot);
            }

            if (it) {
                iterators.push_back(it);
            }
        }
    }

    LeapfrogJoin lf(iterators);
    std::function<void(size_t)> iterate = [&](size_t depth) {
        if (depth == orderedVars.size()) {
            uint32_t newSubjectId = substituteVariableId(rule.head.subject(), bindingIds);
            uint32_t newPredicateId = substituteVariableId(rule.head.predicate(), bindingIds);
            uint32_t newObjectId = substituteVariableId(rule.head.object(), bindingIds);
            newFacts.emplace_back(newSubjectId, newPredicateId, newObjectId);
            return;
        }

        const std::string& var = orderedVars[depth];
        while (!lf.atEnd()) {
            bindingIds[var] = lf.key();
            iterate(depth + 1);
            lf.next();
        }
        bindingIds.erase(var);
    };

    iterate(0);

    // 清理迭代器
    for (auto it : iterators) {
        delete it;
    }
}*/

// ID优化版本的join_by_variable
void DatalogEngine::join_by_variable_optimized(
    TrieNode* psoRoot, TrieNode* posRoot,
    const Rule& rule,
    const std::set<std::string>& variables,
    const std::map<std::string, std::vector<std::pair<int, int>>>& varPositions,
    std::map<std::string, uint32_t>& bindingIds,
    int varIdx,
    std::vector<Triple>& newFacts
) {
    // 当所有变量都已绑定时，生成新的事实
    if (varIdx >= variables.size()) {
        uint32_t newSubjectId = substituteVariableId(rule.head.subject(), bindingIds);
        uint32_t newPredicateId = substituteVariableId(rule.head.predicate(), bindingIds);
        uint32_t newObjectId = substituteVariableId(rule.head.object(), bindingIds);

        newFacts.emplace_back(newSubjectId, newPredicateId, newObjectId);
        return;
    }

    // 优化：在每次递归调用时重新计算变量选择性
    auto selectivities = computeVariableSelectivityOptimized(rule, variables, varPositions, bindingIds);
    
    // 如果没有未绑定的变量，结束递归
    if (selectivities.empty()) {
        uint32_t newSubjectId = substituteVariableId(rule.head.subject(), bindingIds);
        uint32_t newPredicateId = substituteVariableId(rule.head.predicate(), bindingIds);
        uint32_t newObjectId = substituteVariableId(rule.head.object(), bindingIds);
        newFacts.emplace_back(newSubjectId, newPredicateId, newObjectId);
        return;
    }
    
    // 选择选择性最高的变量（候选值最少）
    std::string currentVar = selectivities[0].variable;

    // 对当前变量创建迭代器
    std::vector<TrieIterator*> iterators;
    for (const auto& pos : varPositions.at(currentVar)) {
        int tripleIdx = pos.first;
        int position = pos.second;
        const Triple& triple = rule.body[tripleIdx];

        TrieIterator* it = nullptr;

        // 根据变量位置选择适当的Trie
        if (position == 0) { // 主语位置
            // 如果宾语已绑定，就从posTrie中对应宾语的子节点中查找
            if (!isVariable(triple.object()) || bindingIds.find(triple.object()) != bindingIds.end()) {
                it = new TrieIterator(posRoot);
                uint32_t predId = substituteVariableId(triple.predicate(), bindingIds);
                it->seek(predId);
                if (!it->atEnd() && it->key() == predId) {
                    TrieIterator objIt = it->open();
                    uint32_t objId = substituteVariableId(triple.object(), bindingIds);
                    objIt.seek(objId);
                    if (!objIt.atEnd() && objIt.key() == objId) {
                        iterators.push_back(new TrieIterator(objIt.open()));
                    }
                }
            }
            // 否则，从psoTrie中查找
            else {
                it = new TrieIterator(psoRoot);
                uint32_t predId = substituteVariableId(triple.predicate(), bindingIds);
                it->seek(predId);
                if (!it->atEnd() && it->key() == predId) {
                    TrieIterator subIt = it->open();
                    iterators.push_back(new TrieIterator(subIt));
                }
            }
        }
        else if (position == 2) { // 宾语位置
            // 如果主语已绑定，就从psoTrie中对应主语的子节点中查找
            if (!isVariable(triple.subject()) || bindingIds.find(triple.subject()) != bindingIds.end()) {
                it = new TrieIterator(psoRoot);
                uint32_t predId = substituteVariableId(triple.predicate(), bindingIds);
                it->seek(predId);
                if (!it->atEnd() && it->key() == predId) {
                    TrieIterator subjIt = it->open();
                    uint32_t subjId = substituteVariableId(triple.subject(), bindingIds);
                    subjIt.seek(subjId);
                    if (!subjIt.atEnd() && subjIt.key() == subjId) {
                        iterators.push_back(new TrieIterator(subjIt.open()));
                    }
                }
            }
            // 否则，从posTrie中查找
            else {
                it = new TrieIterator(posRoot);
                uint32_t predId = substituteVariableId(triple.predicate(), bindingIds);
                it->seek(predId);
                if (!it->atEnd() && it->key() == predId) {
                    TrieIterator objIt = it->open();
                    iterators.push_back(new TrieIterator(objIt));
                }
            }
        }

        if (it && iterators.empty()) {
            delete it;
        }
    }

    // 对当前变量执行leapfrog join
    if (!iterators.empty()) {
        LeapfrogJoin lf(iterators);
        while (!lf.atEnd()) {
            uint32_t keyId = lf.key();
            bindingIds[currentVar] = keyId;

            // 递归处理下一个变量
            join_by_variable_optimized(psoRoot, posRoot, rule, variables, varPositions, bindingIds, 0, newFacts);

            lf.next();
        }

        // 清理迭代器
        for (auto it : iterators) {
            delete it;
        }
    }

    // 删除当前变量的绑定
    bindingIds.erase(currentVar);
}

// ID优化版本的辅助函数
uint32_t DatalogEngine::substituteVariableId(const std::string& term, const std::map<std::string, uint32_t>& bindingIds) {
    if (isVariable(term) && bindingIds.find(term) != bindingIds.end()) {
        return bindingIds.at(term);
    }
    // 如果不是变量或未绑定，从字符串池获取ID
    return Triple::getStringPool()->getId(term);
}

// ID优化版本的选择性计算
std::vector<DatalogEngine::VariableSelectivity> DatalogEngine::computeVariableSelectivityOptimized(
    const Rule& rule,
    const std::set<std::string>& variables,
    const std::map<std::string, std::vector<std::pair<int, int>>>& varPositions,
    const std::map<std::string, uint32_t>& bindingIds
) const {
    std::vector<VariableSelectivity> selectivities;
    
    for (const std::string& var : variables) {
        if (bindingIds.find(var) != bindingIds.end()) {
            continue;  // 已绑定的变量跳过
        }
        
        VariableSelectivity vs;
        vs.variable = var;
        vs.candidateCount = 0;
        
        // 估算候选值数量
        const auto& positions = varPositions.at(var);
        size_t minCandidates = SIZE_MAX;
        
        for (const auto& pos : positions) {
            int tripleIdx = pos.first;
            int position = pos.second;
            const Triple& triple = rule.body[tripleIdx];
            
            size_t candidates = 0;
            
            if (position == 0) {  // 主语位置
                // 估算：根据谓语获取主语候选数
                uint32_t predId = substituteVariableId(triple.predicate(), bindingIds);
                candidates = store.queryTripleIdsByPredicateId(predId).size();
            } else if (position == 2) {  // 宾语位置  
                // 估算：根据谓语获取宾语候选数
                uint32_t predId = substituteVariableId(triple.predicate(), bindingIds);
                candidates = store.queryTripleIdsByPredicateId(predId).size();
            }
            
            if (candidates > 0 && candidates < minCandidates) {
                minCandidates = candidates;
            }
        }
        
        vs.candidateCount = (minCandidates == SIZE_MAX) ? 1000000 : minCandidates;
        vs.selectivity = 1.0 / (vs.candidateCount + 1);  // 避免除零
        
        selectivities.push_back(vs);
    }
    
    // 按选择性排序，选择性高的变量优先
    std::sort(selectivities.begin(), selectivities.end());

    auto result = selectivities;

    return result;
}

void DatalogEngine::printTriples() {
    // 遍历存储中的所有三元组并打印
    const auto& allTripleIds = store.getAllTripleIds();
    
    std::cout << "=== All Triples in Store (" << allTripleIds.size() << " total) ===" << std::endl;
    
    for (size_t i = 0; i < allTripleIds.size(); ++i) {
        const auto& tripleId = allTripleIds[i];
        
        // 将ID转换回字符串
        std::string subject = store.getStringPool().getString(tripleId.subject_id);
        std::string predicate = store.getStringPool().getString(tripleId.predicate_id);
        std::string object = store.getStringPool().getString(tripleId.object_id);
        
        std::cout << i + 1 << ": " << subject << " " << predicate << " " << object << std::endl;
    }
    
    std::cout << "=== End of Triple List ===" << std::endl;
}
