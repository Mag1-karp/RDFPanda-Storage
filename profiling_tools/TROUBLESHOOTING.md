# 🚀 WSL性能分析 - 快速故障排除

## 问题1: "cannot execute: required file not found"

**原因**: 脚本文件使用了Windows行尾符（CRLF）而不是Linux行尾符（LF）

**解决方法（3选1）**:

### 方法A: 使用提供的修复脚本（最简单）
```bash
./fix_line_endings.sh
```

### 方法B: 手动修复单个文件
```bash
sed -i 's/\r$//' setup_profiling_tools.sh
sed -i 's/\r$//' profile_wsl.sh
```

### 方法C: 安装dos2unix工具
```bash
sudo apt-get install dos2unix
dos2unix *.sh
```

---

## 问题2: perf 权限错误

```bash
# 临时提升权限
sudo sh -c 'echo 1 > /proc/sys/kernel/perf_event_paranoid'

# 或者用sudo运行
sudo perf record -g ./program
```

---

## 问题3: 没有sudo权限

使用不需要sudo的工具：
```bash
# 使用Valgrind（不需要sudo）
valgrind --tool=callgrind ./cmake-build-debug/RDFPanda_Storage.exe

# 或使用内置profiler
./cmake-build-debug/RDFPanda_Storage.exe
```

---

## 问题4: 脚本找不到bash

检查shebang行：
```bash
head -1 setup_profiling_tools.sh
# 应该输出: #!/bin/bash
```

如果不对，重新创建文件或手动修复。

---

## 问题5: Valgrind运行很慢

这是正常的！Valgrind会让程序慢10-20倍。

**加速方法**:
```bash
# 关闭缓存模拟（更快但信息略少）
valgrind --tool=callgrind --cache-sim=no --branch-sim=no ./program
```

---

## 避免将来出现行尾符问题

### 配置Git自动转换（推荐）
在Windows PowerShell或Git Bash中运行：
```bash
# 在Windows中克隆仓库时自动转换为CRLF，提交时转换为LF
git config --global core.autocrlf true
```

在WSL中：
```bash
# 保持LF不变
git config --global core.autocrlf input
```

### 或者在项目中添加.gitattributes
创建 `.gitattributes` 文件：
```
# 强制.sh文件使用LF
*.sh text eol=lf
```

---

## 快速诊断命令

```bash
# 检查文件格式
file setup_profiling_tools.sh

# 如果输出包含"CRLF"，需要转换
# 输出包含"LF"或只有"text"则正常

# 查看行尾符
od -c setup_profiling_tools.sh | head -2
# 看到 \r\n 就是CRLF（需要修复）
# 只看到 \n 就是LF（正常）
```

---

## 现在可以运行了！

```bash
# 安装工具
./setup_profiling_tools.sh

# 运行分析
./profile_wsl.sh
```
