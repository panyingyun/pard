#!/bin/bash

# PARD多尺寸矩阵性能测试脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

MATRIX_SIZES=(100 1000 10000)
CORES=1

echo "=========================================="
echo "PARD性能测试 - 多尺寸矩阵"
echo "=========================================="
echo ""
echo "测试核心数: ${CORES}"
echo "测试矩阵: ${MATRIX_SIZES[@]}"
echo ""

# CSV结果文件
RESULTS_DIR="$SCRIPT_DIR/perf_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_FILE="$RESULTS_DIR/multi_size_performance_${TIMESTAMP}.csv"

mkdir -p "$RESULTS_DIR"

# CSV文件头
echo "MatrixSize,Cores,AnalysisTime,FactorTime,SolveTime,TotalTime,Status" > "$RESULTS_FILE"

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
        echo "$SIZE,$CORES,0.0,0.0,0.0,0.0,FILE_NOT_FOUND" >> "$RESULTS_FILE"
        continue
    fi
    
    echo ""
    echo "=========================================="
    echo "测试矩阵: ${SIZE}×${SIZE}"
    echo "=========================================="
    echo ""
    
    echo "运行PARD测试..."
    OUTPUT=$(timeout 600 mpirun -np $CORES "$PROJECT_ROOT/build/bin/benchmark" "$MATRIX_FILE" 11 1 2>&1 || echo "")
    
    # 检查是否有错误
    if echo "$OUTPUT" | grep -qi "error\|fault\|fail\|abort"; then
        echo "❌ 测试失败！"
        echo "错误信息:"
        echo "$OUTPUT" | grep -iE "error|fault|fail|abort" | head -5
        echo "$SIZE,$CORES,0.0,0.0,0.0,0.0,ERROR" >> "$RESULTS_FILE"
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
        echo "$SIZE,$CORES,$A,$F,$S,$T,SUCCESS" >> "$RESULTS_FILE"
    else
        echo "⚠️ 无法提取时间信息"
        echo "$SIZE,$CORES,0.0,0.0,0.0,0.0,NO_TIME_DATA" >> "$RESULTS_FILE"
        echo "输出信息："
        echo "$OUTPUT" | tail -20
    fi
    
    sleep 1
done

echo ""
echo "=========================================="
echo "测试完成！"
echo "结果文件: $RESULTS_FILE"
echo "=========================================="

# 显示汇总
if command -v python3 &> /dev/null; then
    python3 << EOF
import csv
import os

csv_file = "$RESULTS_FILE"
if os.path.exists(csv_file):
    print("")
    print("性能汇总:")
    print("矩阵尺寸 | 分析时间(s) | 分解时间(s) | 求解时间(s) | 总时间(s) | 状态")
    print("--------|------------|------------|------------|----------|------")
    
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            status_icon = "✅" if row['Status'] == 'SUCCESS' else "❌"
            print(f"{row['MatrixSize']:8s} | {row['AnalysisTime']:10s} | {row['FactorTime']:10s} | {row['SolveTime']:10s} | {row['TotalTime']:8s} | {status_icon} {row['Status']}")
EOF
fi
