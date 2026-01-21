#!/bin/bash

echo "════════════════════════════════════════════════════════════"
echo "  RDFPanda-Storage Performance Profiling Tools Setup (WSL)"
echo "════════════════════════════════════════════════════════════"
echo ""

# 检查是否在WSL中
if ! grep -qi microsoft /proc/version; then
    echo "WARNING: This doesn't appear to be WSL. Continue anyway? [y/N]"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo "This script will install:"
echo "  • Valgrind (callgrind, massif)"
echo "  • Linux perf tools"
echo "  • FlameGraph tools"
echo "  • Graphviz (for visualization)"
echo ""
read -p "Continue with installation? [Y/n] " -r
if [[ "$REPLY" =~ ^[Nn]$ ]]; then
    exit 0
fi

echo ""
echo "Updating package lists..."
sudo apt-get update

echo ""
echo "[1/4] Installing Valgrind..."
sudo apt-get install -y valgrind

echo ""
echo "[2/4] Installing perf tools..."
sudo apt-get install -y linux-tools-common linux-tools-generic

echo ""
echo "[3/4] Installing graphviz..."
sudo apt-get install -y graphviz

echo ""
echo "[4/4] Installing FlameGraph..."
if [ ! -d ~/FlameGraph ]; then
    git clone https://github.com/brendangregg/FlameGraph.git ~/FlameGraph
    echo 'export PATH=$PATH:~/FlameGraph' >> ~/.bashrc
    echo "✓ FlameGraph installed to ~/FlameGraph"
else
    echo "✓ FlameGraph already installed"
fi

echo ""
echo "════════════════════════════════════════════════════════════"
echo "  Installation Summary"
echo "════════════════════════════════════════════════════════════"

# 检查安装结果
ALL_OK=true

if command -v valgrind &> /dev/null; then
    echo "✓ Valgrind: $(valgrind --version | head -1)"
else
    echo "✗ Valgrind: FAILED"
    ALL_OK=false
fi

if command -v perf &> /dev/null; then
    echo "✓ perf: Installed"
else
    echo "✗ perf: FAILED (may need manual configuration in WSL)"
fi

if command -v dot &> /dev/null; then
    echo "✓ Graphviz: $(dot -V 2>&1 | head -1)"
else
    echo "✗ Graphviz: FAILED"
    ALL_OK=false
fi

if [ -d ~/FlameGraph ]; then
    echo "✓ FlameGraph: ~/FlameGraph"
else
    echo "✗ FlameGraph: FAILED"
    ALL_OK=false
fi

echo ""
echo "════════════════════════════════════════════════════════════"

if [ "$ALL_OK" = true ]; then
    echo "✓ Installation completed successfully!"
else
    echo "⚠ Some tools failed to install. Check errors above."
fi

echo ""
echo "Next steps:"
echo "  1. Run: source ~/.bashrc"
echo "  2. Test with: ./profile_wsl.sh"
echo "  3. Read: PERFORMANCE_WSL_GUIDE.md for detailed usage"
echo ""
echo "════════════════════════════════════════════════════════════"
