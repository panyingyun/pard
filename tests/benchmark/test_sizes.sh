#!/bin/bash

# PARD多尺寸矩阵性能测试脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

MATRIX_SIZES=(100 500)
CORES=1

echo "=========================================="
echo "PARD性能测试 - 多尺寸矩阵"
echo "=========================================="
echo ""
echo "测试核心数: ${CORES}"
echo "测试矩阵: ${MATRIX_SIZES[@]}"
echo ""

# 提取时间的辅助函数
extract_time() {
    local output="$1"
    local pattern="$2"
    local time=$(echo "$output" | grep -i "$pattern" | grep -oE '[0-9]+\.[0-9]+' | head -1)
    if [ -z "$time" ]; then
        echo "0.0"
    else
        echo "$time"
    fi
}

# 测试每个矩阵
for SIZE in "${MATRIX_SIZES[@]}"; do
    MATRIX_FILE="$PROJECT_ROOT/tests/benchmark/test_matrices/perf_matrix_${SIZE}x${SIZE}.mtx"
    
    if [ ! -f "$MATRIX_FILE" ]; then
        echo "警告: 矩阵文件 $MATRIX_FILE 不存在，跳过"
        continue
    fi
    
    echo ""
    echo "=========================================="
    echo "测试矩阵: ${SIZE}×${SIZE}"
    echo "=========================================="
    echo ""
    
    echo "运行PARD测试..."
    OUTPUT=$(timeout 300 mpirun -np $CORES "$PROJECT_ROOT/build/bin/benchmark" "$MATRIX_FILE" 11 1 2>&1 || echo "")
    
    # 检查是否有错误
    if echo "$OUTPUT" | grep -qi "error\|fault\|fail\|abort"; then
        echo "❌ 测试失败！"
        echo "错误信息:"
        echo "$OUTPUT" | grep -iE "error|fault|fail|abort" | head -10
        echo ""
        echo "完整输出（最后30行）:"
        echo "$OUTPUT" | tail -30
        continue
    fi
    
    # 提取时间
    A=$(extract_time "$OUTPUT" "Analysis time")
    F=$(extract_time "$OUTPUT" "Factorization time")
    S=$(extract_time "$OUTPUT" "Solve time")
    T=$(extract_time "$OUTPUT" "Total time")
    
    if [ "$T" = "0.0" ]; then
        # 如果没有总时间，尝试计算
        if [ "$A" != "0.0" ] && [ "$F" != "0.0" ] && [ "$S" != "0.0" ]; then
            T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0.0")
        fi
    fi
    
    if [ "$T" != "0.0" ]; then
        echo "✅ 测试成功"
        echo "  分析时间: ${A}s"
        echo "  分解时间: ${F}s"
        echo "  求解时间: ${S}s"
        echo "  总时间: ${T}s"
    else
        echo "⚠️ 无法提取时间信息"
        echo "输出信息："
        echo "$OUTPUT" | tail -20
    fi
    
    sleep 1
done

echo ""
echo "=========================================="
echo "测试完成！"
echo "=========================================="
