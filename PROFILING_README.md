# 性能分析工具

性能分析相关的所有工具和文档都在 `profiling_tools/` 目录中。

## 快速开始

```bash
cd profiling_tools

# 1. 安装WSL构建工具（首次运行）
./setup_wsl_build.sh

# 2. 在WSL中编译项目
./build_for_wsl.sh

# 3. 运行性能分析
./profile_wsl.sh
```

## 📖 文档

详细文档请查看：
- `profiling_tools/README_PROFILING.md` - 总览
- `profiling_tools/WSL_PROFILING_SETUP.md` - WSL设置指南
- `profiling_tools/QUICKSTART_WSL.md` - 快速开始

## 📁 目录说明

```
profiling_tools/
├── 📖 文档
│   ├── README_PROFILING.md          # 总览和索引
│   ├── WSL_PROFILING_SETUP.md       # WSL性能分析完整设置指南
│   ├── QUICKSTART_WSL.md            # 快速开始
│   ├── PERFORMANCE_WSL_GUIDE.md     # WSL详细指南
│   ├── PERFORMANCE_GUIDE.md         # 通用性能分析指南
│   ├── TROUBLESHOOTING.md           # 故障排除
│   └── PROJECT_STRUCTURE.md         # 项目结构说明
│
├── 🛠️ 脚本工具
│   ├── setup_wsl_build.sh           # 安装WSL编译工具
│   ├── build_for_wsl.sh             # 在WSL中编译项目
│   ├── profile_wsl.sh               # 性能分析脚本
│   ├── setup_profiling_tools.sh     # 安装性能分析工具
│   ├── run_profiling.sh             # 便捷启动脚本
│   └── fix_line_endings.sh          # 修复行尾符
│
└── 💻 代码工具
    ├── PerformanceProfiler.h        # 代码级性能分析器
    ├── MemoryMonitor.h              # 内存监控器
    └── performance_benchmark.cpp    # 综合基准测试
```

详见 `profiling_tools/PROFILING.md`
