# 性能分析工具 (Performance Profiling Tools)

欢迎使用RDFPanda-Storage性能分析工具集！

## 🚀 快速开始（3步）

```bash
# 1. 安装WSL构建工具（首次运行，约3-5分钟）
./setup_wsl_build.sh

# 2. 在WSL中编译项目（约1-2分钟）
./build_for_wsl.sh

# 3. 运行性能分析
./profile_wsl.sh
# 选择 1 (Valgrind Callgrind - 推荐)
```

## 📖 主要文档

- **WSL_PROFILING_SETUP.md** ⭐ - WSL完整设置指南（新手必读）
- **QUICKSTART_WSL.md** - 5分钟快速上手
- **TROUBLESHOOTING.md** - 常见问题解决
- **PERFORMANCE_WSL_GUIDE.md** - 详细使用教程

## 📁 工具列表

### 脚本工具
- `setup_wsl_build.sh` - 安装WSL编译工具链
- `build_for_wsl.sh` - 在WSL中编译项目
- `profile_wsl.sh` - 性能分析主脚本

### 代码工具
- `PerformanceProfiler.h` - C++性能分析器
- `MemoryMonitor.h` - 内存监控器

详见 `README_PROFILING.md`
