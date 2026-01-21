# RDFPanda-Storage 性能瓶颈识别指南

## 工具概览

本项目提供了多种性能分析工具，帮助你识别系统中的性能瓶颈。

## 1. 代码级性能分析

### 1.1 使用 PerformanceProfiler

**文件**: `PerformanceProfiler.h`

**用途**: 细粒度的函数级性能分析

**使用方法**:
```cpp
#include "PerformanceProfiler.h"

void myFunction() {
    PROFILE_FUNCTION();  // 自动记录整个函数
    // ... 你的代码
}

void complexOperation() {
    {
        PROFILE_SCOPE("Phase 1: Data Loading");
        // 阶段1代码
    }
    {
        PROFILE_SCOPE("Phase 2: Processing");
        // 阶段2代码
    }

    GlobalProfiler::getInstance().printReport();
}
```

**输出示例**:
```
=== Performance Profile Report ===
Operation                                Calls       Total(s)         Avg(s)         Min(s)         Max(s)
----------------------------------------------------------------------------------------------------------------
Parse Turtle File                            1       2.345600       2.345600       2.345600       2.345600
Store Triples - Total                        1       5.678900       5.678900       5.678900       5.678900
Datalog Reasoning                            1      15.432100      15.432100      15.432100      15.432100
```

### 1.2 使用 MemoryMonitor

**文件**: `MemoryMonitor.h`

**用途**: 追踪内存使用情况

**使用方法**:
```cpp
#include "MemoryMonitor.h"

void testMemoryUsage() {
    MemoryMonitor::printMemoryUsage("Before loading data");

    // ... 加载大量数据

    MemoryMonitor::printMemoryUsage("After loading data");
}
```

**输出示例**:
```
=== Memory Usage [After loading data] ===
  Current RSS:   2.34 GB
  Peak RSS:      2.45 GB
  Virtual Mem:   3.12 GB
```

### 1.3 运行综合基准测试

**文件**: `performance_benchmark.cpp`

**编译**:
```bash
# 在 CMakeLists.txt 中添加:
add_executable(performance_benchmark
    performance_benchmark.cpp
    InputParser.cpp
    TripleStore.cpp
    DatalogEngine.cpp
    Trie.cpp
    LRUCache.cpp
)

target_link_libraries(performance_benchmark libmysql sqlite3)
```

**运行**:
```bash
./cmake-build-debug/performance_benchmark
```

## 2. 系统级性能分析

### 2.1 使用 Linux Perf (推荐用于CPU分析)

**安装**:
```bash
sudo apt-get install linux-tools-common linux-tools-generic
```

**使用步骤**:

1. **编译时添加调试符号**:
   ```bash
   # 修改 CMakeLists.txt
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2")
   ```

2. **记录性能数据**:
   ```bash
   # 基本记录
   perf record -g ./RDFPanda_Storage

   # 记录特定函数的调用栈 (更详细)
   perf record -g -e cycles:pp ./RDFPanda_Storage

   # 限制采样频率 (降低开销)
   perf record -F 99 -g ./RDFPanda_Storage
   ```

3. **查看报告**:
   ```bash
   # 交互式查看
   perf report

   # 按函数排序
   perf report --sort=symbol

   # 查看热点函数
   perf report --stdio | head -50
   ```

4. **生成火焰图** (推荐):
   ```bash
   # 下载火焰图工具
   git clone https://github.com/brendangregg/FlameGraph.git

   # 生成火焰图
   perf script | FlameGraph/stackcollapse-perf.pl | FlameGraph/flamegraph.pl > flamegraph.svg

   # 在浏览器中打开查看
   firefox flamegraph.svg
   ```

**如何解读**:
- 宽度表示CPU时间占比
- 颜色随机，只用于区分
- 从下往上看调用栈
- 找最宽的"平台"区域 = 性能瓶颈

### 2.2 使用 Valgrind Callgrind (详细的函数调用分析)

**安装**:
```bash
sudo apt-get install valgrind kcachegrind
```

**使用**:
```bash
# 运行分析 (会使程序变慢10-20倍)
valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./RDFPanda_Storage

# 可视化分析结果
kcachegrind callgrind.out

# 或查看文本报告
callgrind_annotate callgrind.out
```

**关键指标**:
- **Ir (Instructions Read)**: 执行的指令数
- **Dr/Dw (Data Read/Write)**: 数据读写次数
- **Self cost**: 函数自身消耗
- **Incl cost**: 包含子函数的总消耗

### 2.3 使用 Valgrind Massif (内存分配分析)

**用途**: 找到内存泄漏和过度分配

```bash
# 运行内存分析
valgrind --tool=massif --massif-out-file=massif.out ./RDFPanda_Storage

# 查看报告
ms_print massif.out | less

# 可视化 (可选)
massif-visualizer massif.out
```

### 2.4 使用 gprof (传统GNU profiler)

**步骤**:
```bash
# 1. 编译时添加 -pg 标志
# 在 CMakeLists.txt 中:
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")

# 2. 重新编译
cmake --build cmake-build-debug

# 3. 运行程序 (会生成 gmon.out)
./cmake-build-debug/RDFPanda_Storage

# 4. 生成报告
gprof ./cmake-build-debug/RDFPanda_Storage gmon.out > analysis.txt

# 5. 查看报告
less analysis.txt
```

## 3. 针对你项目的关键性能指标

### 3.1 需要重点关注的组件

根据你的代码结构，以下是可能的性能瓶颈:

1. **InputParser** (`InputParser.cpp`)
   - Turtle/N-Triples 解析速度
   - 字符串处理开销
   - 文件I/O效率

2. **TripleStore** (`TripleStore.cpp`)
   - Trie索引构建时间
   - 三元组插入性能
   - 查询性能 (SPO/POS/OPS索引)

3. **DatalogEngine** (`DatalogEngine.cpp`)
   - Leapfrog join 性能
   - 规则匹配效率
   - LRU缓存命中率
   - StringPool 效率

4. **StringPool** (`StringPool.h`)
   - 字符串intern性能
   - 哈希冲突
   - 内存占用

5. **LRU Cache** (`LRUCache.cpp`)
   - 缓存命中率
   - 锁竞争 (如果多线程)

### 3.2 性能测试清单

针对每个组件添加以下监控:

```cpp
// 示例: 在 DatalogEngine.cpp 中添加
void DatalogEngine::iterativeReason() {
    PROFILE_FUNCTION();

    size_t iteration = 0;
    size_t newFactsCount = 0;

    do {
        PROFILE_SCOPE("Reasoning Iteration");

        // 记录每轮迭代的统计
        std::cout << "Iteration " << iteration++
                  << ": New facts = " << newFactsCount << std::endl;

        // ... 推理逻辑

    } while (newFactsCount > 0);
}
```

## 4. 推荐的性能测试流程

### Step 1: 运行综合基准测试
```bash
./performance_benchmark > benchmark_results.txt
```

### Step 2: 使用 Perf 找出热点函数
```bash
perf record -g ./RDFPanda_Storage
perf report --stdio | head -50 > hotspots.txt
```

### Step 3: 针对性优化

根据发现的瓶颈:

- **如果解析慢**: 考虑并行解析、使用mmap、优化字符串处理
- **如果存储慢**: 优化Trie结构、批量插入、使用更高效的索引
- **如果推理慢**:
  - 优化join顺序
  - 增加缓存容量
  - 使用更高效的数据结构
  - 考虑并行推理
- **如果内存占用高**: 优化StringPool、使用压缩、减少冗余存储

### Step 4: 重复测试验证优化效果

```bash
# 优化前
./performance_benchmark > before.txt

# 优化后
./performance_benchmark > after.txt

# 对比
diff before.txt after.txt
```

## 5. 常见性能问题和解决方案

### 问题1: Datalog推理非常慢

**诊断**:
```bash
perf record -g ./RDFPanda_Storage
perf report
# 检查 leapfrogTriejoin, join_by_variable 等函数的时间占比
```

**可能的优化**:
- 优化变量选择顺序 (selectivity优化)
- 增大缓存容量
- 使用布隆过滤器减少无效join
- 并行化不同规则的推理

### 问题2: 内存占用过高

**诊断**:
```bash
valgrind --tool=massif ./RDFPanda_Storage
ms_print massif.out
```

**可能的优化**:
- StringPool去重效果不佳 → 改进哈希函数
- Trie索引冗余 → 使用更紧凑的存储结构
- 缓存过大 → 调整LRU缓存大小

### 问题3: 解析速度慢

**诊断**:
```bash
perf record -g -e cache-misses ./RDFPanda_Storage
perf report
```

**可能的优化**:
- 使用内存映射文件 (mmap)
- 批量处理减少系统调用
- 使用更快的字符串处理库 (如 string_view)

## 6. 快速诊断脚本

创建 `quick_profile.sh`:

```bash
#!/bin/bash

echo "=== Quick Performance Profile ==="
echo "1. Running benchmark..."
time ./cmake-build-debug/RDFPanda_Storage > /dev/null

echo ""
echo "2. CPU profiling..."
perf record -g -o perf.data ./cmake-build-debug/RDFPanda_Storage 2>&1 > /dev/null
perf report --stdio -i perf.data | head -30

echo ""
echo "3. Memory profiling..."
valgrind --tool=massif --massif-out-file=massif.out ./cmake-build-debug/RDFPanda_Storage 2>&1 > /dev/null
ms_print massif.out | head -50

echo ""
echo "=== Profile Complete ==="
echo "Detailed reports saved to: perf.data, massif.out"
```

使用:
```bash
chmod +x quick_profile.sh
./quick_profile.sh
```

## 总结

1. **先使用内置的性能分析工具** (PerformanceProfiler, MemoryMonitor) 快速定位大致问题
2. **再使用系统工具深入分析** (perf, valgrind) 找到具体热点
3. **针对性优化后重新测试**
4. **持续监控关键指标**

主要关注:
- CPU时间分布 → 用 perf/gprof
- 内存使用 → 用 massif/MemoryMonitor
- 缓存效率 → 添加统计代码
- I/O性能 → 用 strace/iostat
