#!/bin/bash
# 便捷脚本：在profiling_tools目录内运行性能分析

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -f "profile_wsl.sh" ]; then
    echo "错误: 找不到 profile_wsl.sh"
    echo "请确保你在 profiling_tools 目录中运行此脚本。"
    exit 1
fi

./profile_wsl.sh
