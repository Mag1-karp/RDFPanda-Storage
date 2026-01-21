#!/bin/bash

echo "=== RDFPanda-Storage Performance Analysis (WSL) ==="
echo ""

PROJECT_DIR="/mnt/c/projects/RDFPanda-Storage"
cd "$PROJECT_DIR"

# 自动检测可执行文件
EXECUTABLE=""

# 优先使用WSL编译的版本
if [ -f "build-wsl/RDFPanda_Storage" ]; then
    EXECUTABLE="build-wsl/RDFPanda_Storage"
    echo "✓ 使用WSL编译的版本: $EXECUTABLE"
elif [ -f "cmake-build-debug/RDFPanda_Storage" ]; then
    EXECUTABLE="cmake-build-debug/RDFPanda_Storage"
    echo "✓ 使用Linux可执行文件: $EXECUTABLE"
elif [ -f "cmake-build-debug/RDFPanda_Storage.exe" ]; then
    echo "⚠ 检测到Windows编译的.exe文件"
    echo "  Windows编译的程序无法在WSL中进行性能分析"
    echo ""
    echo "解决方法："
    echo "  1. 安装WSL构建工具: ./setup_wsl_build.sh"
    echo "  2. 在WSL中编译项目: ./build_for_wsl.sh"
    echo "  3. 然后再次运行此脚本"
    echo ""
    exit 1
else
    echo "❌ 错误: 找不到可执行文件"
    echo ""
    echo "请先编译项目："
    echo "  方案A (WSL): ./build_for_wsl.sh"
    echo "  方案B (手动): mkdir build-wsl && cd build-wsl && cmake .. && make"
    exit 1
fi

# 验证是否为Linux可执行文件
FILE_TYPE=$(file "$EXECUTABLE")
if echo "$FILE_TYPE" | grep -q "PE32"; then
    echo "❌ 错误: $EXECUTABLE 是Windows可执行文件，无法在WSL中分析"
    echo "请在WSL中重新编译: ./build_for_wsl.sh"
    exit 1
fi

echo ""

# 检测可用的性能工具
PERF_AVAILABLE=false
VALGRIND_AVAILABLE=false

if command -v perf &> /dev/null; then
    echo "✓ perf is available (但在WSL2中可能受限)"
    PERF_AVAILABLE=true
else
    echo "✗ perf is not available"
fi

if command -v valgrind &> /dev/null; then
    echo "✓ valgrind is available (推荐)"
    VALGRIND_AVAILABLE=true
else
    echo "✗ valgrind is not available (install: sudo apt-get install valgrind)"
fi

echo ""
echo "Choose analysis method:"
echo "1) Use Valgrind Callgrind (Recommended for WSL)"
echo "2) Use perf (可能不可用)"
echo "3) Use built-in profiler only"
echo "4) Memory analysis with Valgrind Massif"
echo "5) Quick benchmark"
read -p "Enter choice [1-5]: " choice

case $choice in
    1)
        if [ "$VALGRIND_AVAILABLE" = true ]; then
            echo ""
            echo "Running Valgrind Callgrind..."
            echo "(This will be slow - 10-20x slower than normal execution)"
            echo ""

            OUTPUT_FILE="callgrind.out.$(date +%Y%m%d_%H%M%S)"
            valgrind --tool=callgrind \
                     --callgrind-out-file="$OUTPUT_FILE" \
                     ./"$EXECUTABLE"

            echo ""
            echo "═══════════════════════════════════════════════════════"
            echo "Analysis complete! Top 30 hotspot functions:"
            echo "═══════════════════════════════════════════════════════"
            callgrind_annotate "$OUTPUT_FILE" | head -50

            echo ""
            echo "Full report saved to: $OUTPUT_FILE"
            echo ""
            echo "View in Windows:"
            echo "  notepad.exe $OUTPUT_FILE"
            echo "  or open: C:\\projects\\RDFPanda-Storage\\$OUTPUT_FILE"
        else
            echo "ERROR: Valgrind not installed."
            echo "Install with: sudo apt-get install valgrind"
        fi
        ;;

    2)
        echo ""
        echo "⚠ 注意: perf在WSL2中通常不可用"
        echo "推荐使用选项1 (Valgrind) 代替"
        echo ""
        read -p "仍要尝试perf？[y/N] " -r
        if [[ "$REPLY" =~ ^[Yy]$ ]] && [ "$PERF_AVAILABLE" = true ]; then
            echo "Running perf..."
            sudo perf record -F 99 -g ./"$EXECUTABLE" 2>&1

            echo ""
            echo "═══════════════════════════════════════════════════════"
            echo "Top CPU hotspots:"
            echo "═══════════════════════════════════════════════════════"
            sudo perf report --stdio 2>&1 | head -100

            # 生成火焰图（如果可用）
            if [ -d ~/FlameGraph ]; then
                FLAME_FILE="flamegraph_$(date +%Y%m%d_%H%M%S).svg"
                sudo perf script 2>&1 | ~/FlameGraph/stackcollapse-perf.pl | \
                    ~/FlameGraph/flamegraph.pl > "$FLAME_FILE"

                echo ""
                echo "Flamegraph generated: $FLAME_FILE"
                echo "Open in Windows browser:"
                echo "  C:\\projects\\RDFPanda-Storage\\$FLAME_FILE"
            fi
        else
            echo "已取消"
        fi
        ;;

    3)
        echo ""
        echo "Running program with built-in profiler..."
        echo ""
        ./"$EXECUTABLE"
        ;;

    4)
        if [ "$VALGRIND_AVAILABLE" = true ]; then
            echo ""
            echo "Running memory analysis..."
            echo "(This will be slow)"
            echo ""

            OUTPUT_FILE="massif.out.$(date +%Y%m%d_%H%M%S)"
            valgrind --tool=massif \
                     --massif-out-file="$OUTPUT_FILE" \
                     ./"$EXECUTABLE"

            echo ""
            echo "═══════════════════════════════════════════════════════"
            echo "Memory usage report:"
            echo "═══════════════════════════════════════════════════════"
            ms_print "$OUTPUT_FILE" | head -100

            echo ""
            echo "Full report saved to: $OUTPUT_FILE"
            echo "View with: ms_print $OUTPUT_FILE | less"
        else
            echo "ERROR: Valgrind not installed."
            echo "Install with: sudo apt-get install valgrind"
        fi
        ;;

    5)
        echo ""
        echo "Running quick benchmark..."
        echo ""
        time ./"$EXECUTABLE"
        ;;

    *)
        echo "Invalid choice"
        exit 1
        ;;
esac

echo ""
echo "═══════════════════════════════════════════════════════════"
echo "Analysis Complete"
echo "═══════════════════════════════════════════════════════════"
