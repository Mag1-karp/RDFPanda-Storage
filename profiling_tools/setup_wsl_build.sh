#!/bin/bash

echo "════════════════════════════════════════════════════════════"
echo "  WSL构建环境设置 - RDFPanda-Storage"
echo "════════════════════════════════════════════════════════════"
echo ""

# 检查是否在WSL中
if ! grep -qi microsoft /proc/version; then
    echo "警告: 这不像是在WSL环境中运行"
    read -p "继续？[y/N] " -r
    if [[ ! "$REPLY" =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo "这个脚本会安装以下工具："
echo "  • C++编译工具链 (g++, cmake, make)"
echo "  • SQLite开发库"
echo "  • MySQL客户端开发库"
echo "  • 性能分析工具 (valgrind, 可选perf)"
echo ""
read -p "继续安装？[Y/n] " -r
if [[ "$REPLY" =~ ^[Nn]$ ]]; then
    exit 0
fi

echo ""
echo "正在更新软件包列表..."
sudo apt-get update

echo ""
echo "[1/4] 安装C++编译工具链..."
sudo apt-get install -y \
    build-essential \
    cmake \
    g++ \
    make

echo ""
echo "[2/4] 安装SQLite开发库..."
sudo apt-get install -y \
    libsqlite3-dev

echo ""
echo "[3/4] 安装MySQL客户端开发库..."
sudo apt-get install -y \
    libmysqlclient-dev

echo ""
echo "[4/4] 安装性能分析工具..."
sudo apt-get install -y \
    valgrind \
    linux-tools-common \
    linux-tools-generic

echo ""
echo "════════════════════════════════════════════════════════════"
echo "  安装完成 - 工具版本信息"
echo "════════════════════════════════════════════════════════════"

echo "✓ g++: $(g++ --version | head -1)"
echo "✓ cmake: $(cmake --version | head -1)"
echo "✓ make: $(make --version | head -1)"
echo "✓ valgrind: $(valgrind --version 2>&1 | head -1)"

echo ""
echo "════════════════════════════════════════════════════════════"
echo "  下一步："
echo "════════════════════════════════════════════════════════════"
echo ""
echo "1. 在WSL中创建构建目录："
echo "   mkdir -p build-wsl && cd build-wsl"
echo ""
echo "2. 配置CMake（会自动寻找WSL中的库）："
echo "   cmake .."
echo ""
echo "3. 编译项目："
echo "   make -j\$(nproc)"
echo ""
echo "4. 运行性能分析："
echo "   cd ../profiling_tools"
echo "   ./profile_wsl.sh"
echo ""
echo "或者运行自动构建脚本："
echo "   ./build_for_wsl.sh"
echo ""
