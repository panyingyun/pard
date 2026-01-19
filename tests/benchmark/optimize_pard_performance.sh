#!/bin/bash

# PARD vs MUMPS性能对比和优化测试脚本
# 目标：优化PARD使其性能超过MUMPS

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RESULTS_DIR="$SCRIPT_DIR/perf_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_FILE="$RESULTS_DIR/performance_optimization_${TIMESTAMP}.csv"

mkdir -p "$RESULTS_DIR"

# 测试配置
MATRIX_SIZES=(1000 10000)
CORES=(1 2 4)

echo "=========================================="
echo "PARD vs MUMPS 性能优化测试"
echo "=========================================="
echo ""
echo "目标: 优化PARD使其性能超过MUMPS"
echo "测试矩阵: ${MATRIX_SIZES[@]}"
echo "测试核心数: ${CORES[@]}"
echo "结果文件: $RESULTS_FILE"
echo ""

# CSV文件头
echo "MatrixSize,Cores,Solver,AnalysisTime,FactorTime,SolveTime,TotalTime" > "$RESULTS_FILE"

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
    
    # 测试MUMPS
    echo ""
    echo "--- MUMPS 测试 ---"
    for NP in "${CORES[@]}"; do
        echo -n "  MUMPS ${NP}核: "
        
        OUTPUT=$(timeout 600 mpirun -np $NP "$PROJECT_ROOT/build/mumps_benchmark" "$MATRIX_FILE" 2>&1 || echo "")
        
        if [ -z "$OUTPUT" ] || echo "$OUTPUT" | grep -qi "error\|fault\|fail"; then
            echo "失败（超时或错误）"
            echo "$SIZE,$NP,MUMPS,0.0,0.0,0.0,0.0" >> "$RESULTS_FILE"
            continue
        fi
        
        A=$(extract_time "$OUTPUT" "Analysis time")
        F=$(extract_time "$OUTPUT" "Factorization time")
        S=$(extract_time "$OUTPUT" "Solve time")
        T=$(extract_time "$OUTPUT" "Total time")
        
        if [ "$T" = "0.0" ] || [ -z "$T" ]; then
            if [ "$A" != "0.0" ] && [ "$F" != "0.0" ] && [ "$S" != "0.0" ]; then
                T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0.0")
            fi
        fi
        
        if [ "$T" != "0.0" ]; then
            echo "总时间: ${T}s"
            echo "$SIZE,$NP,MUMPS,$A,$F,$S,$T" >> "$RESULTS_FILE"
        else
            echo "失败（无数据）"
            echo "$SIZE,$NP,MUMPS,0.0,0.0,0.0,0.0" >> "$RESULTS_FILE"
        fi
        
        sleep 1
    done
    
    # 测试PARD
    echo ""
    echo "--- PARD 测试 ---"
    for NP in "${CORES[@]}"; do
        echo -n "  PARD ${NP}核: "
        
        OUTPUT=$(timeout 600 mpirun -np $NP "$PROJECT_ROOT/build/bin/benchmark" "$MATRIX_FILE" 11 1 2>&1 || echo "")
        
        if [ -z "$OUTPUT" ] || echo "$OUTPUT" | grep -qi "error\|fault\|fail"; then
            echo "失败（超时或错误）"
            echo "$SIZE,$NP,PARD,0.0,0.0,0.0,0.0" >> "$RESULTS_FILE"
            continue
        fi
        
        A=$(extract_time "$OUTPUT" "Analysis time")
        F=$(extract_time "$OUTPUT" "Factorization time")
        S=$(extract_time "$OUTPUT" "Solve time")
        T=$(extract_time "$OUTPUT" "Total time")
        
        if [ "$T" = "0.0" ] || [ -z "$T" ]; then
            if [ "$A" != "0.0" ] && [ "$F" != "0.0" ] && [ "$S" != "0.0" ]; then
                T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0.0")
            fi
        fi
        
        if [ "$T" != "0.0" ]; then
            echo "总时间: ${T}s"
            echo "$SIZE,$NP,PARD,$A,$F,$S,$T" >> "$RESULTS_FILE"
        else
            echo "失败（无数据）"
            echo "$SIZE,$NP,PARD,0.0,0.0,0.0,0.0" >> "$RESULTS_FILE"
        fi
        
        sleep 1
    done
    
    echo ""
done

echo ""
echo "=========================================="
echo "测试完成！"
echo "结果文件: $RESULTS_FILE"
echo "=========================================="

# 生成对比报告
if command -v python3 &> /dev/null; then
    python3 << EOF
import sys
import os
import csv

csv_file = "$RESULTS_FILE"
if os.path.exists(csv_file):
    print("")
    print("==========================================")
    print("  性能对比分析")
    print("==========================================")
    print("")
    
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        data = list(reader)
    
    for size in [1000, 10000]:
        print(f"\n{size}×{size} 矩阵:")
        print("核心数 | MUMPS时间(s) | PARD时间(s) | PARD/MUMPS | 状态")
        print("------|-------------|-------------|-----------|------")
        
        for cores in [1, 2, 4]:
            mumps_t = 0.0
            pard_t = 0.0
            
            for row in data:
                if int(row['MatrixSize']) == size and int(row['Cores']) == cores:
                    if row['Solver'] == 'MUMPS':
                        mumps_t = float(row['TotalTime'])
                    elif row['Solver'] == 'PARD':
                        pard_t = float(row['TotalTime'])
            
            if mumps_t > 0 and pard_t > 0:
                ratio = pard_t / mumps_t
                status = "✅ PARD更快" if pard_t < mumps_t else "⚠️ MUMPS更快"
                print(f"{cores:5d} | {mumps_t:11.6f} | {pard_t:11.6f} | {ratio:8.2f}x | {status}")
            elif mumps_t > 0:
                print(f"{cores:5d} | {mumps_t:11.6f} |      - |      - | ❌ PARD失败")
            elif pard_t > 0:
                print(f"{cores:5d} |      - | {pard_t:11.6f} |      - | ⚠️ MUMPS失败")
    
    print("")
EOF
fi
