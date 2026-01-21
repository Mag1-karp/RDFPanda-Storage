# 性能分析工具 (Performance Profiling Tools)

性能分析相关的工具和文档已移至 `profiling_tools/` 目录。

## 快速开始

### 1. 安装性能分析工具（首次运行）
```bash
cd profiling_tools
./setup_profiling_tools.sh
source ~/.bashrc
```

### 2. 运行性能分析
```bash
cd profiling_tools
./profile_wsl.sh
```

或者在项目根目录直接运行：
```bash
./run_profiling.sh
```

## 📁 目录结构

```
profiling_tools/
├── README_PROFILING.md          # 总览和索引
├── QUICKSTART_WSL.md            # WSL快速开始指南 ⭐
├── PERFORMANCE_WSL_GUIDE.md     # WSL完整性能分析指南
├── PERFORMANCE_GUIDE.md         # 通用性能分析指南
├── TROUBLESHOOTING.md           # 故障排除
│
├── setup_profiling_tools.sh     # 一键安装工具
├── profile_wsl.sh               # 交互式性能分析脚本
├── fix_line_endings.sh          # 修复行尾符问题
│
├── PerformanceProfiler.h        # 代码级性能分析器
├── MemoryMonitor.h              # 内存监控器
└── performance_benchmark.cpp    # 综合基准测试
```

## 📖 文档

- **新手？** 先读 `profiling_tools/QUICKSTART_WSL.md`
- **详细指南** 见 `profiling_tools/PERFORMANCE_WSL_GUIDE.md`
- **遇到问题？** 查看 `profiling_tools/TROUBLESHOOTING.md`

## 🔧 代码中使用性能分析器

在你的C++代码中：

```cpp
#include "profiling_tools/PerformanceProfiler.h"
#include "profiling_tools/MemoryMonitor.h"

void myFunction() {
    PROFILE_FUNCTION();  // 自动记录函数执行时间

    MemoryMonitor::printMemoryUsage("检查点1");

    // 你的代码...
}
```

## 📊 性能分析结果

所有生成的性能报告（火焰图、callgrind输出等）会保存在项目根目录，可以在Windows中直接访问：
- `C:\projects\RDFPanda-Storage\flamegraph.svg`
- `C:\projects\RDFPanda-Storage\callgrind.out.*`
- `C:\projects\RDFPanda-Storage\massif.out.*`
