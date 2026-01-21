# 性能分析工具 - 文件索引

## 📚 文档

1. **QUICKSTART_WSL.md** ⭐ **从这里开始！**
   - WSL环境快速入门指南
   - 5分钟上手
   - 包含火焰图解读教程

2. **PERFORMANCE_WSL_GUIDE.md**
   - WSL环境完整性能分析指南
   - 详细的工具使用说明
   - 常见问题解决方案

3. **PERFORMANCE_GUIDE.md**
   - 通用性能分析指南
   - 适用于所有Linux环境
   - 包含高级性能优化技巧

## 🛠️ 工具和脚本

1. **setup_profiling_tools.sh**
   - 一键安装所有性能分析工具
   - 包括：Valgrind, perf, FlameGraph等
   ```bash
   ./setup_profiling_tools.sh
   ```

2. **profile_wsl.sh**
   - 交互式性能分析脚本
   - 支持多种分析模式
   ```bash
   ./profile_wsl.sh
   ```

## 📦 代码文件

1. **PerformanceProfiler.h**
   - 细粒度性能分析器
   - 使用方法：
     ```cpp
     #include "PerformanceProfiler.h"

     void myFunction() {
         PROFILE_FUNCTION();
         // 你的代码
     }
     ```

2. **MemoryMonitor.h**
   - 内存使用监控器
   - 使用方法：
     ```cpp
     #include "MemoryMonitor.h"

     MemoryMonitor::printMemoryUsage("检查点1");
     ```

3. **performance_benchmark.cpp**
   - 综合性能测试套件
   - 需要在CMakeLists.txt中添加编译配置

## 🚀 快速开始（3步）

### Step 1: 安装工具
```bash
cd /mnt/c/projects/RDFPanda-Storage
./setup_profiling_tools.sh
source ~/.bashrc
```

### Step 2: 运行分析
```bash
# 方式1: 使用自动化脚本（推荐）
./profile_wsl.sh

# 方式2: 直接运行Valgrind
valgrind --tool=callgrind ./cmake-build-debug/RDFPanda_Storage.exe
callgrind_annotate callgrind.out | head -50
```

### Step 3: 查看结果

**方式A: 在终端查看文本报告**
```bash
callgrind_annotate callgrind.out | less
```

**方式B: 在Windows中查看火焰图**
```bash
# 生成火焰图（如果perf可用）
sudo perf record -g ./cmake-build-debug/RDFPanda_Storage.exe
sudo perf script | ~/FlameGraph/stackcollapse-perf.pl | ~/FlameGraph/flamegraph.pl > flame.svg

# Windows中打开：
# 路径: C:\projects\RDFPanda-Storage\flame.svg
# 直接双击文件，浏览器会自动打开
```

**方式C: 使用VS Code**
```bash
code .  # 打开整个项目
# 所有生成的报告文件都可以在VS Code中直接查看
```

## 🔥 火焰图示例

生成的SVG火焰图可以：
- ✅ 在任何浏览器中打开（Chrome, Firefox, Edge）
- ✅ 交互式：可以点击放大、搜索函数
- ✅ 直观显示性能瓶颈（最宽的部分）

## 📊 输出示例

运行分析后，你会看到类似这样的报告：

```
=== Performance Profile Report ===
Operation                                Calls       Total(s)         Avg(s)
---------------------------------------------------------------------------
Parse Turtle File                            1       2.345600       2.345600
Store Triples                                1       5.678900       5.678900
Datalog Reasoning                            1      15.432100      15.432100
  leapfrogTriejoin                         234      12.345678       0.052736
  join_by_variable                        1024       8.765432       0.008559

=== Memory Usage ===
  Current RSS:   2.34 GB
  Peak RSS:      2.45 GB
```

## ⚡ 常见场景

| 场景 | 使用工具 | 命令 |
|------|---------|------|
| 程序很慢，不知道原因 | Valgrind Callgrind | `./profile_wsl.sh` → 选择1 |
| 想看可视化性能分析 | perf + FlameGraph | `./profile_wsl.sh` → 选择2 |
| 内存占用太高 | Valgrind Massif | `./profile_wsl.sh` → 选择4 |
| 快速看各阶段耗时 | 内置Profiler | `./profile_wsl.sh` → 选择3 |
| 只想看总执行时间 | time命令 | `./profile_wsl.sh` → 选择5 |

## 🎯 重点关注的组件

根据你的代码结构，性能瓶颈最可能出现在：

1. **DatalogEngine::iterativeReason()** - 推理引擎主循环
2. **DatalogEngine::leapfrogTriejoin()** - Join算法
3. **TripleStore::addTriple()** - 三元组存储
4. **InputParser::parseTurtle()** - 文件解析

## 💡 提示

- 所有在 `/mnt/c/projects/RDFPanda-Storage/` 中生成的文件都可以在Windows中访问
- WSL路径 `/mnt/c/projects/...` = Windows路径 `C:\projects\...`
- 火焰图SVG文件可以直接在Windows资源管理器中双击打开
- 使用VS Code可以在WSL和Windows之间无缝切换

## 需要帮助？

1. 查看 **QUICKSTART_WSL.md** 了解基础用法
2. 查看 **PERFORMANCE_WSL_GUIDE.md** 了解详细说明
3. 遇到问题可以参考各文档中的"常见问题"章节
