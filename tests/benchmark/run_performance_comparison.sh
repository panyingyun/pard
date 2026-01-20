#!/bin/bash
# PARD vs MUMPS性能对比测试脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

# 测试矩阵
MATRICES=(
    "tests/benchmark/test_matrices/perf_matrix_500x500.mtx"
    "tests/benchmark/test_matrices/perf_matrix_1000x1000.mtx"
)

# 核数
CORES=(1 2 4)

# 矩阵类型（非对称）
MTYPE=11

# 结果文件
RESULTS_FILE="tests/benchmark/performance_results_$(date +%Y%m%d_%H%M%S).csv"

echo "Matrix,Size,Cores,Solver,Analysis_Time,Factor_Time,Solve_Time,Total_Time,Fill_in_nnz,Max_Residual" > "$RESULTS_FILE"

for matrix_file in "${MATRICES[@]}"; do
    if [ ! -f "$matrix_file" ]; then
        echo "Warning: Matrix file not found: $matrix_file"
        continue
    fi
    
    # 获取矩阵大小（从文件名提取）
    matrix_size=$(basename "$matrix_file" | grep -oE '[0-9]+' | head -1)
    
    for cores in "${CORES[@]}"; do
        echo ""
        echo "=========================================="
        echo "Testing: $matrix_file, Cores: $cores"
        echo "=========================================="
        
        # 测试PARD（使用矩阵创建逻辑，因为读取Matrix Market文件有问题）
        echo "--- PARD ($cores cores) ---"
        if [ -f "build/perf_test_with_create" ]; then
            PARD_OUTPUT=$(mpirun -np $cores ./build/perf_test_with_create $matrix_size $MTYPE 2>&1)
            PARD_ANALYSIS=$(echo "$PARD_OUTPUT" | grep "Analysis time:" | grep -oE '[0-9]+\.[0-9]+' || echo "0")
            PARD_FACTOR=$(echo "$PARD_OUTPUT" | grep "Factorization time:" | grep -oE '[0-9]+\.[0-9]+' || echo "0")
            PARD_SOLVE=$(echo "$PARD_OUTPUT" | grep "Solve time:" | grep -oE '[0-9]+\.[0-9]+' || echo "0")
            PARD_TOTAL=$(echo "$PARD_OUTPUT" | grep "Total time:" | grep -oE '[0-9]+\.[0-9]+' || echo "0")
            PARD_FILLIN=$(echo "$PARD_OUTPUT" | grep "Fill-in nnz:" | grep -oE '[0-9]+' || echo "0")
            PARD_RESIDUAL=$(echo "$PARD_OUTPUT" | grep "Max residual:" | grep -oE '[0-9]+\.[0-9]+e[+-][0-9]+' || echo "0")
            
            echo "$matrix_file,$matrix_size,$cores,PARD,$PARD_ANALYSIS,$PARD_FACTOR,$PARD_SOLVE,$PARD_TOTAL,$PARD_FILLIN,$PARD_RESIDUAL" >> "$RESULTS_FILE"
            echo "PARD: Total=$PARD_TOTAL s"
        else
            echo "Error: PARD test program not found"
        fi
        
        # 测试MUMPS
        echo "--- MUMPS ($cores cores) ---"
        if [ -f "build/mumps_benchmark" ]; then
            MUMPS_OUTPUT=$(mpirun -np $cores ./build/mumps_benchmark "$matrix_file" 2>&1)
            MUMPS_TOTAL=$(echo "$MUMPS_OUTPUT" | grep -E "Total time|Elapsed time" | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0")
            MUMPS_ANALYSIS=$(echo "$MUMPS_OUTPUT" | grep -E "Analysis time|Symbolic" | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0")
            MUMPS_FACTOR=$(echo "$MUMPS_OUTPUT" | grep -E "Factorization time|Numeric" | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0")
            MUMPS_SOLVE=$(echo "$MUMPS_OUTPUT" | grep -E "Solve time" | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0")
            
            if [ "$MUMPS_TOTAL" = "0" ]; then
                # 尝试从MUMPS输出中提取时间信息
                MUMPS_TIME=$(echo "$MUMPS_OUTPUT" | grep -oE '[0-9]+\.[0-9]+' | tail -1 || echo "0")
                if [ "$MUMPS_TIME" != "0" ]; then
                    MUMPS_TOTAL="$MUMPS_TIME"
                fi
            fi
            
            echo "$matrix_file,$matrix_size,$cores,MUMPS,$MUMPS_ANALYSIS,$MUMPS_FACTOR,$MUMPS_SOLVE,$MUMPS_TOTAL,0,0" >> "$RESULTS_FILE"
            echo "MUMPS: Total=$MUMPS_TOTAL s"
        else
            echo "Warning: MUMPS benchmark program not found"
        fi
    done
done

echo ""
echo "=========================================="
echo "Performance comparison complete!"
echo "Results saved to: $RESULTS_FILE"
echo "=========================================="
cat "$RESULTS_FILE"
