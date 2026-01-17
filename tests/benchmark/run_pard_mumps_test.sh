#!/bin/bash
# PARD vs MUMPS 性能对比测试（1000x1000和10000x10000）

set -e

cd "$(dirname "$0")/../.."

RESULTS_FILE="tests/benchmark/perf_results/pard_mumps_comparison.csv"
echo "Matrix_Size,Num_Cores,Solver,Analysis_Time,Factor_Time,Solve_Time,Total_Time" > "$RESULTS_FILE"

MATRIX_CONFIGS=(
    "1000x1000 tests/benchmark/test_matrices/perf_matrix_1000x1000.mtx"
    "10000x10000 tests/benchmark/test_matrices/perf_matrix_10000x10000.mtx"
)

CORES=(1 2 4 8)

echo "=========================================="
echo "  PARD vs MUMPS Performance Test"
echo "=========================================="
echo ""

# 确保矩阵存在
for config in "${MATRIX_CONFIGS[@]}"; do
    read -r size matrix_file <<< "$config"
    if [ ! -f "$matrix_file" ]; then
        echo "Error: Matrix file $matrix_file not found"
        exit 1
    fi
done

echo "Running performance tests..."
echo "=========================================="

for config in "${MATRIX_CONFIGS[@]}"; do
    read -r size matrix_file <<< "$config"
    
    echo ""
    echo "Matrix: $size"
    echo "----------------------------------------"
    
    # 测试PARD
    echo "PARD:"
    for cores in "${CORES[@]}"; do
        echo -n "  $cores core(s): "
        
        OUTPUT=$(timeout 300 mpirun -np $cores ./build/bin/benchmark "$matrix_file" 11 1 2>&1)
        
        # 提取时间（匹配"Analysis time:      0.002997 seconds"格式）
        A=$(echo "$OUTPUT" | grep -i "Analysis time" | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0")
        F=$(echo "$OUTPUT" | grep -i "Factorization time" | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0")
        S=$(echo "$OUTPUT" | grep -i "Solve time" | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0")
        T=$(echo "$OUTPUT" | grep -i "Total time" | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0")
        
        # 如果总时间为0或空，且其他时间不为0，计算总和
        if [ -z "$T" ] || [ "$T" = "0" ] || [ "$T" = "0.0" ]; then
            if [ -n "$A" ] && [ -n "$F" ] && [ -n "$S" ] && [ "$A" != "0" ]; then
                T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0")
            fi
        fi
        
        if [ "$T" != "0" ]; then
            echo "$size,$cores,PARD,$A,$F,$S,$T" >> "$RESULTS_FILE"
            echo "Total: ${T}s (Analysis: ${A}s, Factor: ${F}s, Solve: ${S}s)"
        else
            echo "FAILED"
        fi
    done
    
    # 测试MUMPS
    if [ -f "./build/mumps_benchmark" ]; then
        echo "MUMPS:"
        for cores in "${CORES[@]}"; do
            echo -n "  $cores core(s): "
            
            OUTPUT=$(timeout 300 mpirun -np $cores ./build/mumps_benchmark "$matrix_file" 2>&1)
            
            # 提取时间（匹配"Analysis time:      0.002997 seconds"格式）
            A=$(echo "$OUTPUT" | grep -i "Analysis time" | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0")
            F=$(echo "$OUTPUT" | grep -i "Factorization time" | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0")
            S=$(echo "$OUTPUT" | grep -i "Solve time" | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0")
            T=$(echo "$OUTPUT" | grep -i "Total time" | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0")
            
            # 如果总时间为0或空，且其他时间不为0，计算总和
            if [ -z "$T" ] || [ "$T" = "0" ] || [ "$T" = "0.0" ]; then
                if [ -n "$A" ] && [ -n "$F" ] && [ -n "$S" ] && [ "$A" != "0" ]; then
                    T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0")
                fi
            fi
            
            if [ "$T" != "0" ]; then
                echo "$size,$cores,MUMPS,$A,$F,$S,$T" >> "$RESULTS_FILE"
                echo "Total: ${T}s (Analysis: ${A}s, Factor: ${F}s, Solve: ${S}s)"
            else
                echo "FAILED"
            fi
        done
    else
        echo "MUMPS: benchmark not available"
    fi
done

echo ""
echo "=========================================="
echo "Test completed! Results: $RESULTS_FILE"
echo "=========================================="
