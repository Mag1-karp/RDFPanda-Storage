# WSL环境下的性能分析指南

## 1. 安装必要的性能分析工具

### 1.1 安装 perf (推荐)

```bash
# 安装 perf
sudo apt-get update
sudo apt-get install linux-tools-common linux-tools-generic

# WSL2 特定：安装匹配内核版本的 perf
# 查看内核版本
uname -r  # 输出例如: 6.6.87.2-microsoft-standard-WSL2

# 尝试安装（如果上面的命令不行）
sudo apt-get install linux-tools-$(uname -r | cut -d'-' -f1)
# 或者
sudo apt-get install linux-tools-6.6.87-generic

# 如果还是找不到，安装最新版本
sudo apt-get install linux-tools-generic
```

**WSL2 perf 限制和解决方案**:

WSL2 中 perf 可能受限，如果遇到权限问题：

```bash
# 方案1: 提升权限（临时）
sudo sh -c 'echo 1 > /proc/sys/kernel/perf_event_paranoid'
sudo sh -c 'echo 0 > /proc/sys/kernel/kptr_restrict'

# 方案2: 使用 sudo 运行 perf
sudo perf record -g ./RDFPanda_Storage
sudo perf report

# 方案3: 如果 perf 在 WSL2 中完全不可用，使用替代方案（见下文）
```

### 1.2 安装 Valgrind (强烈推荐用于WSL)

Valgrind 在 WSL 中工作得很好，是**最可靠的选择**：

```bash
sudo apt-get install valgrind
```

### 1.3 安装火焰图工具

```bash
# 克隆火焰图生成工具
cd ~
git clone https://github.com/brendangregg/FlameGraph.git

# 添加到 PATH (可选)
echo 'export PATH=$PATH:~/FlameGraph' >> ~/.bashrc
source ~/.bashrc
```

## 2. 生成和查看火焰图 (WSL环境)

### 方法1: 使用 perf 生成火焰图（如果可用）

```bash
cd /mnt/c/projects/RDFPanda-Storage

# 1. 编译时添加调试信息
# 在 CMakeLists.txt 中添加：set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2")
cmake --build cmake-build-debug

# 2. 使用 perf 记录
sudo perf record -F 99 -g ./cmake-build-debug/RDFPanda_Storage.exe

# 3. 生成火焰图
sudo perf script | ~/FlameGraph/stackcollapse-perf.pl | ~/FlameGraph/flamegraph.pl > flamegraph.svg

# 4. 在 Windows 浏览器中打开
# 火焰图会保存在 /mnt/c/projects/RDFPanda-Storage/flamegraph.svg
# 在 Windows 中打开路径：C:\projects\RDFPanda-Storage\flamegraph.svg
```

**在Windows中打开火焰图**:
```powershell
# 在 PowerShell 中运行（或直接双击文件）
start chrome C:\projects\RDFPanda-Storage\flamegraph.svg
# 或
start firefox C:\projects\RDFPanda-Storage\flamegraph.svg
# 或直接用资源管理器打开该文件
```

### 方法2: 使用 Valgrind Callgrind (推荐用于WSL)

这是**WSL环境下最可靠的方法**：

```bash
# 1. 运行性能分析
valgrind --tool=callgrind \
         --callgrind-out-file=callgrind.out \
         --cache-sim=yes \
         --branch-sim=yes \
         ./cmake-build-debug/RDFPanda_Storage.exe

# 2. 生成文本报告
callgrind_annotate callgrind.out > performance_report.txt

# 3. 查看报告
less performance_report.txt
# 或在 Windows 中用记事本打开
notepad.exe performance_report.txt

# 4. 【可选】转换为火焰图
# 安装 gprof2dot
pip install gprof2dot

# 生成调用图
gprof2dot -f callgrind callgrind.out | dot -Tsvg -o callgraph.svg

# 在 Windows 浏览器中查看
```

**查看Callgrind结果**:
```bash
# 如果想要GUI（需要安装X Server）
# 1. 安装 VcXsrv 或 WSLg (Windows 11 自带)
# 2. 设置 DISPLAY
export DISPLAY=:0

# 3. 安装 kcachegrind
sudo apt-get install kcachegrind

# 4. 运行（如果X Server已配置）
kcachegrind callgrind.out
```

### 方法3: 使用自定义分析器（最简单）

这是**最简单且在WSL中100%可用的方法**：

```bash
# 直接运行你的程序，使用内置的 PerformanceProfiler
./cmake-build-debug/RDFPanda_Storage.exe

# 输出会直接显示性能统计
```

## 3. WSL特定的性能分析脚本

创建一个自动化脚本 `profile_wsl.sh`：

```bash
#!/bin/bash

echo "=== RDFPanda-Storage Performance Analysis (WSL) ==="
echo ""

PROJECT_DIR="/mnt/c/projects/RDFPanda-Storage"
cd "$PROJECT_DIR"

# 检测可用的性能工具
PERF_AVAILABLE=false
VALGRIND_AVAILABLE=false

if command -v perf &> /dev/null; then
    echo "✓ perf is available"
    PERF_AVAILABLE=true
else
    echo "✗ perf is not available"
fi

if command -v valgrind &> /dev/null; then
    echo "✓ valgrind is available"
    VALGRIND_AVAILABLE=true
else
    echo "✗ valgrind is not available"
fi

echo ""
echo "Choose analysis method:"
echo "1) Use Valgrind Callgrind (Recommended for WSL)"
echo "2) Use perf (if available)"
echo "3) Use built-in profiler only"
echo "4) Memory analysis with Valgrind Massif"
echo "5) Quick benchmark"
read -p "Enter choice [1-5]: " choice

case $choice in
    1)
        if [ "$VALGRIND_AVAILABLE" = true ]; then
            echo "Running Valgrind Callgrind..."
            valgrind --tool=callgrind \
                     --callgrind-out-file=callgrind.out.$(date +%Y%m%d_%H%M%S) \
                     ./cmake-build-debug/RDFPanda_Storage.exe

            latest=$(ls -t callgrind.out.* | head -1)
            echo ""
            echo "Analysis complete. Report:"
            callgrind_annotate "$latest" | head -100
            echo ""
            echo "Full report saved to: $latest"
            echo "View in Windows: notepad.exe $latest"
        else
            echo "Valgrind not installed. Run: sudo apt-get install valgrind"
        fi
        ;;

    2)
        if [ "$PERF_AVAILABLE" = true ]; then
            echo "Running perf..."
            sudo perf record -F 99 -g ./cmake-build-debug/RDFPanda_Storage.exe

            # 生成火焰图（如果可用）
            if [ -d ~/FlameGraph ]; then
                sudo perf script | ~/FlameGraph/stackcollapse-perf.pl | \
                    ~/FlameGraph/flamegraph.pl > flamegraph_$(date +%Y%m%d_%H%M%S).svg
                echo "Flamegraph generated! Open in Windows browser:"
                echo "C:\\projects\\RDFPanda-Storage\\flamegraph_*.svg"
            fi

            sudo perf report --stdio | head -100
        else
            echo "perf not available in WSL"
        fi
        ;;

    3)
        echo "Running program with built-in profiler..."
        ./cmake-build-debug/RDFPanda_Storage.exe
        ;;

    4)
        if [ "$VALGRIND_AVAILABLE" = true ]; then
            echo "Running memory analysis..."
            valgrind --tool=massif \
                     --massif-out-file=massif.out.$(date +%Y%m%d_%H%M%S) \
                     ./cmake-build-debug/RDFPanda_Storage.exe

            latest=$(ls -t massif.out.* | head -1)
            echo ""
            echo "Memory analysis complete:"
            ms_print "$latest" | head -100
        else
            echo "Valgrind not installed."
        fi
        ;;

    5)
        echo "Running quick benchmark..."
        time ./cmake-build-debug/RDFPanda_Storage.exe
        ;;

    *)
        echo "Invalid choice"
        ;;
esac

echo ""
echo "=== Analysis Complete ==="
```

保存并运行：
```bash
chmod +x profile_wsl.sh
./profile_wsl.sh
```

## 4. 在Windows中查看生成的文件

所有在 `/mnt/c/projects/RDFPanda-Storage/` 中生成的文件都可以在Windows中访问：

```
WSL路径: /mnt/c/projects/RDFPanda-Storage/flamegraph.svg
Windows路径: C:\projects\RDFPanda-Storage\flamegraph.svg
```

**打开方式**:

1. **火焰图 (.svg)**:
   - 直接在文件资源管理器中双击
   - 或拖入Chrome/Firefox/Edge浏览器

2. **性能报告 (.txt)**:
   ```bash
   # 在WSL中
   notepad.exe performance_report.txt
   # 或
   code performance_report.txt  # 如果安装了 VS Code
   ```

3. **使用VS Code**:
   ```bash
   # 在WSL中打开整个项目
   code .

   # 在VS Code中可以直接查看所有生成的文件
   ```

## 5. 推荐的WSL性能分析流程

### 快速诊断（5分钟）:

```bash
# 1. 使用内置profiler
./cmake-build-debug/RDFPanda_Storage.exe

# 2. 查看输出的性能统计
```

### 深度分析（20分钟）:

```bash
# 1. Valgrind 详细分析
valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./cmake-build-debug/RDFPanda_Storage.exe

# 2. 查看热点函数
callgrind_annotate callgrind.out | head -50

# 3. 内存分析
valgrind --tool=massif --massif-out-file=massif.out ./cmake-build-debug/RDFPanda_Storage.exe
ms_print massif.out | head -50
```

### 可视化分析（需要额外设置）:

**选项A: 使用 WSLg (Windows 11)**

Windows 11 自带 WSLg，可以直接运行Linux GUI应用：

```bash
# 安装GUI工具
sudo apt-get install kcachegrind

# 直接运行（Windows 11会自动显示窗口）
kcachegrind callgrind.out
```

**选项B: 在Windows中使用QCacheGrind**

1. 下载 QCacheGrind for Windows: https://sourceforge.net/projects/qcachegrindwin/
2. 在WSL中生成 callgrind.out
3. 在Windows中用QCacheGrind打开该文件

## 6. 常见问题和解决方案

### 问题1: perf 提示权限错误

```bash
# 临时解决
sudo sh -c 'echo 1 > /proc/sys/kernel/perf_event_paranoid'

# 或者直接用 sudo
sudo perf record -g ./program
sudo perf report
```

### 问题2: 火焰图是空白的

```bash
# 确保编译时包含了调试符号
# 在 CMakeLists.txt 中：
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fno-omit-frame-pointer")

# 重新编译
cmake --build cmake-build-debug --clean-first
```

### 问题3: Valgrind 运行太慢

```bash
# 使用采样模式（更快但精度稍低）
valgrind --tool=callgrind --cache-sim=no --branch-sim=no ./program

# 或者限制分析范围
valgrind --tool=callgrind --toggle-collect=main ./program
```

## 7. 最佳实践总结

**在WSL中推荐使用**:

1. **首选**: Valgrind (最可靠)
   ```bash
   valgrind --tool=callgrind ./program
   ```

2. **次选**: 内置 PerformanceProfiler (最简单)
   ```bash
   # 代码中已包含，直接运行即可
   ./program
   ```

3. **可选**: perf (如果可用)
   ```bash
   sudo perf record -g ./program
   ```

**文件访问**:
- 所有生成的报告都可以在Windows中直接访问
- 使用VS Code在WSL和Windows之间无缝切换
- SVG火焰图直接在浏览器中打开

**性能提示**:
- WSL2的I/O性能：尽量在 `/mnt/c/` 下工作（你已经这样做了✓）
- 如果分析结果不理想，考虑在Linux虚拟机中进行更准确的分析
- WSL2对CPU性能影响很小，分析结果基本准确

## 8. 一键安装所有工具

```bash
#!/bin/bash
# 保存为 setup_profiling_tools.sh

echo "Installing performance profiling tools for WSL..."

sudo apt-get update

# 基础工具
sudo apt-get install -y \
    linux-tools-common \
    linux-tools-generic \
    valgrind \
    graphviz \
    git

# Python工具
pip install gprof2dot

# 火焰图工具
if [ ! -d ~/FlameGraph ]; then
    git clone https://github.com/brendangregg/FlameGraph.git ~/FlameGraph
    echo 'export PATH=$PATH:~/FlameGraph' >> ~/.bashrc
fi

echo ""
echo "Installation complete!"
echo "Run 'source ~/.bashrc' to update PATH"
```

运行:
```bash
chmod +x setup_profiling_tools.sh
./setup_profiling_tools.sh
source ~/.bashrc
```
