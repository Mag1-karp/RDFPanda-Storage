# 🔧 在WSL中进行性能分析 - 完整指南

## 问题说明

你遇到的问题是：**当前的可执行文件是Windows格式的**，无法在WSL中运行Valgrind或perf。

```
当前状况:
cmake-build-debug/RDFPanda_Storage.exe  ← Windows PE格式（MinGW/MSVC编译）
                                        ← 无法在WSL中分析 ❌

需要:
build-wsl/RDFPanda_Storage              ← Linux ELF格式（g++编译）
                                        ← 可以在WSL中分析 ✅
```

## 🚀 快速解决方案（3个命令）

### Step 1: 安装WSL构建工具
```bash
./setup_wsl_build.sh
```
这会安装：
- g++, cmake, make（C++编译工具）
- libsqlite3-dev, libmysqlclient-dev（数据库库）
- valgrind（性能分析工具）

### Step 2: 在WSL中编译项目
```bash
./build_for_wsl.sh
```
这会：
- 创建 `build-wsl/` 目录
- 用Linux工具链编译项目
- 生成可在WSL中分析的Linux可执行文件

### Step 3: 运行性能分析
```bash
cd profiling_tools
./profile_wsl.sh
```
脚本会自动检测并使用WSL编译的版本。

---

## 📋 详细步骤

### 1. 安装WSL构建环境

```bash
cd /mnt/c/projects/RDFPanda-Storage
./setup_wsl_build.sh
```

安装过程会提示输入sudo密码。安装完成后会显示：
```
✓ g++: g++ (Ubuntu ...) ...
✓ cmake: cmake version ...
✓ make: GNU Make ...
✓ valgrind: valgrind-...
```

### 2. 编译项目

```bash
./build_for_wsl.sh
```

编译过程：
```
创建构建目录: build-wsl
配置CMake...
编译项目（使用 N 个并行任务）...
✓ 可执行文件: build-wsl/RDFPanda_Storage
✓ 文件类型: ELF 64-bit LSB executable
```

**注意**:
- 首次编译可能需要几分钟
- 如果看到MySQL相关警告可以忽略（不影响核心功能）
- 编译好的文件在 `build-wsl/` 目录

### 3. 验证可执行文件

```bash
file build-wsl/RDFPanda_Storage
# 应该输出: ELF 64-bit LSB executable, x86-64, ...
```

如果看到"ELF"字样，说明编译成功！

### 4. 运行性能分析

```bash
cd profiling_tools
./profile_wsl.sh
```

会看到：
```
✓ 使用WSL编译的版本: build-wsl/RDFPanda_Storage
✓ valgrind is available (推荐)

Choose analysis method:
1) Use Valgrind Callgrind (Recommended for WSL)  ← 推荐选这个
2) Use perf (可能不可用)
3) Use built-in profiler only
4) Memory analysis with Valgrind Massif
5) Quick benchmark
```

选择 **1** 开始分析。

---

## 🎯 关于perf的说明

**为什么perf不可用？**

perf工具需要匹配内核版本的工具包，但WSL2使用的是Microsoft定制的内核：
```
你的内核: 6.6.87.2-microsoft-standard-WSL2
apt包仓库: 没有匹配这个特定版本的perf包
```

**解决方案**:

1. **推荐：使用Valgrind**（100%可用，功能强大）
   ```bash
   valgrind --tool=callgrind ./build-wsl/RDFPanda_Storage
   callgrind_annotate callgrind.out
   ```

2. 如果确实需要火焰图，可以：
   - 方案A: 从Valgrind输出转换（使用gprof2dot）
   - 方案B: 在真实Linux环境中运行perf
   - 方案C: 使用内置的PerformanceProfiler

---

## 📊 两个编译版本对比

| 特性 | Windows版本 | WSL版本 |
|------|------------|---------|
| 编译器 | MinGW/MSVC | g++ |
| 可执行格式 | PE32+ (.exe) | ELF |
| 运行环境 | Windows原生 | WSL2 |
| 性能分析 | ❌ 不支持Valgrind/perf | ✅ 完全支持 |
| 用途 | 日常开发调试 | 性能分析优化 |
| 位置 | `cmake-build-debug/` | `build-wsl/` |

**建议**:
- 日常开发和调试：使用Windows版本（CLion/Visual Studio）
- 性能分析和优化：使用WSL版本（Valgrind）

---

## 🔄 常见工作流

### 开发流程
```bash
# 1. 在Windows中用IDE编写代码（CLion/VS）
# 2. 在Windows中编译测试
cmake-build-debug/RDFPanda_Storage.exe

# 3. 准备性能分析时，在WSL中重新编译
./build_for_wsl.sh

# 4. 运行性能分析
cd profiling_tools && ./profile_wsl.sh

# 5. 根据分析结果优化代码
# 6. 回到步骤1
```

### 快速测试
```bash
# 只想快速看看程序运行时间
cd build-wsl
time ./RDFPanda_Storage
```

### 详细性能分析
```bash
# CPU热点分析
cd build-wsl
valgrind --tool=callgrind ./RDFPanda_Storage
callgrind_annotate callgrind.out.* | less

# 内存使用分析
valgrind --tool=massif ./RDFPanda_Storage
ms_print massif.out.* | less
```

---

## ❓ 常见问题

### Q: 需要删除Windows编译的版本吗？
**A**: 不需要！两个版本可以并存：
- `cmake-build-debug/` - Windows版本（日常使用）
- `build-wsl/` - WSL版本（性能分析）

### Q: 每次修改代码都要重新编译两次吗？
**A**: 看情况：
- 日常开发：只在Windows中编译即可
- 需要性能分析时：在WSL中也编译一次

### Q: WSL编译很慢怎么办？
**A**: 使用并行编译：
```bash
cd build-wsl
make -j$(nproc)  # 使用所有CPU核心
```

### Q: 编译时找不到MySQL/SQLite？
**A**: 安装开发库：
```bash
sudo apt-get install libsqlite3-dev libmysqlclient-dev
```

### Q: 还是想用perf怎么办？
**A**: 可以尝试这个（但不保证成功）：
```bash
# 降低权限要求
sudo sh -c 'echo -1 > /proc/sys/kernel/perf_event_paranoid'

# 然后运行
sudo perf record -g ./build-wsl/RDFPanda_Storage
```

如果还是失败，请使用Valgrind，它在WSL中100%可用且功能更强大。

---

## 📚 相关文档

- `profiling_tools/QUICKSTART_WSL.md` - WSL性能分析快速指南
- `profiling_tools/PERFORMANCE_WSL_GUIDE.md` - 完整WSL性能分析手册
- `profiling_tools/TROUBLESHOOTING.md` - 故障排除
- `PROFILING.md` - 性能分析工具总览

---

## ✅ 总结

**要在WSL中进行性能分析，关键是：**

1. ✅ 安装WSL构建工具：`./setup_wsl_build.sh`
2. ✅ 在WSL中编译项目：`./build_for_wsl.sh`
3. ✅ 使用Valgrind分析：`cd profiling_tools && ./profile_wsl.sh`

**不需要担心perf不可用** - Valgrind在WSL中工作完美，而且提供的信息更详细！
