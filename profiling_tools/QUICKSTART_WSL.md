# 🚀 WSL环境下性能分析 - 快速开始

## 第一步：安装性能分析工具

运行以下命令一键安装所有工具（大约需要2-3分钟）：

```bash
cd /mnt/c/projects/RDFPanda-Storage
./setup_profiling_tools.sh
source ~/.bashrc
```

## 第二步：运行性能分析

### 方案1: 使用自动化脚本（最简单）

```bash
./profile_wsl.sh
```

然后选择：
- **1** - Valgrind分析（最推荐，找CPU热点）
- **4** - 内存分析（找内存泄漏）
- **5** - 快速计时（只看总耗时）

### 方案2: 手动运行（如果你想更精细控制）

**A. 找CPU瓶颈（推荐）:**

```bash
# 使用Valgrind Callgrind
valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./cmake-build-debug/RDFPanda_Storage.exe

# 查看哪些函数最慢
callgrind_annotate callgrind.out | head -50
```

**B. 找内存问题:**

```bash
# 使用Valgrind Massif
valgrind --tool=massif --massif-out-file=massif.out ./cmake-build-debug/RDFPanda_Storage.exe

# 查看内存使用曲线
ms_print massif.out | less
```

**C. 生成火焰图（可视化）:**

```bash
# 如果perf可用
sudo perf record -F 99 -g ./cmake-build-debug/RDFPanda_Storage.exe
sudo perf script | ~/FlameGraph/stackcollapse-perf.pl | ~/FlameGraph/flamegraph.pl > flamegraph.svg

# 在Windows中打开（双击文件或：）
start C:\projects\RDFPanda-Storage\flamegraph.svg
```

## 第三步：在Windows中查看结果

### 火焰图（.svg文件）

生成的火焰图在WSL路径：`/mnt/c/projects/RDFPanda-Storage/flamegraph.svg`

对应Windows路径：`C:\projects\RDFPanda-Storage\flamegraph.svg`

**查看方式**:
1. 直接双击该文件（会用默认浏览器打开）
2. 或在Windows PowerShell中运行：
   ```powershell
   start chrome C:\projects\RDFPanda-Storage\flamegraph.svg
   ```

### 性能报告（文本文件）

在WSL中用记事本打开：
```bash
notepad.exe callgrind.out
# 或
notepad.exe performance_report.txt
```

在VS Code中打开：
```bash
code callgrind.out
```

## 🔥 火焰图解读指南

### 什么是火焰图？

火焰图是一个交互式的SVG图片，显示程序运行时哪些函数消耗了最多CPU时间。

```
┌────────────────────────────────────────────────┐
│              main (100%)                        │  ← 最底层：程序入口
├──────────────┬───────────────┬─────────────────┤
│ parseFile    │ storeTriples  │ reasoning       │  ← 中间层：主要功能
│    (10%)     │     (20%)     │    (70%)        │
└──────────────┴───────────────┴─────────────────┘
                                    ▲
                                    │
                            这里最宽 = 性能瓶颈！
```

### 如何看火焰图？

1. **X轴（宽度）**: 表示CPU时间占比
   - 越宽 = 越慢 = **性能瓶颈**
   - 最宽的部分就是你要优化的地方

2. **Y轴（高度）**: 表示函数调用栈
   - 从下往上看：main → 子函数 → 孙函数
   - 高度本身不重要

3. **颜色**: 随机分配，只用于区分不同函数
   - 不代表快慢

4. **交互**:
   - 点击函数框可以放大查看
   - 搜索框可以高亮特定函数

### 示例分析

如果看到：
```
leapfrogTriejoin ███████████████████ 85%
```
说明：85%的时间都花在`leapfrogTriejoin`函数上，这就是瓶颈！

## 常见场景

### 场景1: "我想知道推理慢在哪里"

```bash
# 1. 运行Valgrind分析
valgrind --tool=callgrind ./cmake-build-debug/RDFPanda_Storage.exe

# 2. 查看报告
callgrind_annotate callgrind.out | grep -E "(leapfrog|reason|join)" | head -20
```

你会看到类似：
```
234,567,890  DatalogEngine::iterativeReason()
189,234,567  DatalogEngine::leapfrogTriejoin()
 98,765,432  DatalogEngine::join_by_variable()
```
数字越大 = 越慢

### 场景2: "我想知道内存用在哪里"

```bash
# 1. 运行内存分析
valgrind --tool=massif ./cmake-build-debug/RDFPanda_Storage.exe

# 2. 查看峰值内存
ms_print massif.out | head -100
```

会显示：
```
    MB
1.2^                                                 :@@@@@@@
    |                                    ::::::::::::@::::::@
    |                        ::::::::::::@::::::::::@::::::@
    |            ::::::::::::@::::::::::@::::::::::@::::::@
    |    ::::::::@::::::::::@::::::::::@::::::::::@::::::@
  0 +----------------------------------------------------------------------->
    0                                                                   100%
```

### 场景3: "程序跑得很慢，不知道原因"

```bash
# 方案A: 快速定位（5分钟）
./profile_wsl.sh
# 选择 3 (built-in profiler)
# 会直接输出各阶段耗时

# 方案B: 详细分析（20分钟）
./profile_wsl.sh
# 选择 1 (Valgrind)
# 会给出详细的函数级统计
```

## 性能优化清单

分析完之后，根据结果优化：

- [ ] **解析慢** → 优化InputParser，考虑并行解析
- [ ] **存储慢** → 优化Trie索引，考虑批量插入
- [ ] **推理慢** → 优化join顺序，增加缓存
- [ ] **查询慢** → 检查索引效率
- [ ] **内存高** → 检查StringPool，优化数据结构

## 需要帮助？

详细文档：
- `PERFORMANCE_WSL_GUIDE.md` - WSL完整指南
- `PERFORMANCE_GUIDE.md` - 通用性能分析指南

## 小贴士

✅ **推荐做法**:
- 先用快速benchmark看总体情况
- 再用Valgrind找具体瓶颈
- 优化后重新测试对比

❌ **避免**:
- 不要在没分析前就盲目优化
- 不要忽略占比小于5%的函数（先优化大头）
- 不要在WSL中使用Windows路径运行程序

⚡ **性能提示**:
- WSL2访问 `/mnt/c/` 的性能很好，你的项目位置没问题
- 编译时加 `-O2` 优化会更接近实际性能
- 大数据集测试能更好暴露瓶颈
