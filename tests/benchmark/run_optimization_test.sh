#!/bin/bash

# PARD vs MUMPS 性能优化测试脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
MATRIX_FILE="$PROJECT_ROOT/tests/benchmark/test_matrices/perf_matrix_100x100.mtx"
RESULTS_DIR="$SCRIPT_DIR/perf_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_FILE="$RESULTS_DIR/performance_${TIMESTAMP}.csv"

mkdir -p "$RESULTS_DIR"

echo "=========================================="
echo "PARD vs MUMPS 性能优化测试"
echo "=========================================="
echo ""
echo "矩阵: $MATRIX_FILE"
echo "结果文件: $RESULTS_FILE"
echo ""

# CSV文件头
echo "Cores,Solver,AnalysisTime,FactorTime,SolveTime,TotalTime" > "$RESULTS_FILE"

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

CORES=(1 2 4)

# 测试MUMPS
echo "--- MUMPS 测试 ---"
for NP in "${CORES[@]}"; do
    echo -n "  MUMPS ${NP}核: "
    
    OUTPUT=$(timeout 300 mpirun -np $NP "$PROJECT_ROOT/build/mumps_benchmark" "$MATRIX_FILE" 2>&1 || echo "")
    
    if [ -z "$OUTPUT" ] || echo "$OUTPUT" | grep -qi "error\|fault\|fail"; then
        echo "失败"
        echo "$NP,MUMPS,0.0,0.0,0.0,0.0" >> "$RESULTS_FILE"
        continue
    fi
    
    A=$(extract_time "$OUTPUT" "Analysis time")
    F=$(extract_time "$OUTPUT" "Factorization time")
    S=$(extract_time "$OUTPUT" "Solve time")
    T=$(extract_time "$OUTPUT" "Total time")
    
    if [ "$T" = "0.0" ]; then
        T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0.0")
    fi
    
    if [ "$T" != "0.0" ]; then
        printf "总时间: %.6fs\n" "$T"
        echo "$NP,MUMPS,$A,$F,$S,$T" >> "$RESULTS_FILE"
    else
        echo "失败（无数据）"
        echo "$NP,MUMPS,0.0,0.0,0.0,0.0" >> "$RESULTS_FILE"
    fi
    
    sleep 1
done

# 测试PARD
echo ""
echo "--- PARD 测试 ---"
for NP in "${CORES[@]}"; do
    echo -n "  PARD ${NP}核: "
    
    OUTPUT=$(timeout 300 mpirun -np $NP "$PROJECT_ROOT/build/bin/benchmark" "$MATRIX_FILE" 11 1 2>&1 || echo "")
    
    if [ -z "$OUTPUT" ] || echo "$OUTPUT" | grep -qi "error\|fault\|fail"; then
        echo "失败"
        echo "$NP,PARD,0.0,0.0,0.0,0.0" >> "$RESULTS_FILE"
        continue
    fi
    
    A=$(extract_time "$OUTPUT" "Analysis time")
    F=$(extract_time "$OUTPUT" "Factorization time")
    S=$(extract_time "$OUTPUT" "Solve time")
    T=$(extract_time "$OUTPUT" "Total time")
    
    if [ "$T" = "0.0" ]; then
        T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0.0")
    fi
    
    if [ "$T" != "0.0" ]; then
        printf "总时间: %.6fs\n" "$T"
        echo "$NP,PARD,$A,$F,$S,$T" >> "$RESULTS_FILE"
    else
        echo "失败（无数据）"
        echo "$NP,PARD,0.0,0.0,0.0,0.0" >> "$RESULTS_FILE"
    fi
    
    sleep 1
done

echo ""
echo "=========================================="
echo "测试完成！结果: $RESULTS_FILE"
echo "=========================================="

# 生成对比
echo ""
echo "性能对比:"
echo "核心数 | MUMPS时间(s) | PARD时间(s) | 加速比 | 状态"
echo "------|-------------|-------------|--------|------"

for NP in "${CORES[@]}"; do
    MUMPS_T=$(grep "^$NP,MUMPS," "$RESULTS_FILE" | cut -d',' -f6)
    PARD_T=$(grep "^$NP,PARD," "$RESULTS_FILE" | cut -d',' -f6)
    
    if [ -n "$MUMPS_T" ] && [ -n "$PARD_T" ] && [ "$MUMPS_T" != "0.0" ] && [ "$PARD_T" != "0.0" ]; then
        RATIO=$(echo "scale=2; $PARD_T / $MUMPS_T" | bc 2>/dev/null || echo "0.00")
        SPEEDUP=$(echo "scale=2; $MUMPS_T / $PARD_T" | bc 2>/dev/null || echo "0.00")
        if (( $(echo "$PARD_T < $MUMPS_T" | bc -l) )); then
            STATUS="✅ PARD更快 (${SPEEDUP}x)"
        else
            STATUS="⚠️  MUMPS更快 (${RATIO}x)"
        fi
        printf "%5d | %11.6f | %11.6f | %6.2fx | %s\n" "$NP" "$MUMPS_T" "$PARD_T" "$RATIO" "$STATUS"
    fi
done
