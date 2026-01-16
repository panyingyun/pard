#!/bin/bash
# PARD vs MUMPS 性能对比测试脚本

set -e

cd "$(dirname "$0")/../.."

echo "=========================================="
echo "  PARD vs MUMPS Performance Comparison"
echo "=========================================="
echo ""

mkdir -p tests/benchmark/test_matrices
mkdir -p tests/benchmark/perf_results

RESULTS_FILE="tests/benchmark/perf_results/pard_mumps_comparison.csv"
echo "Matrix_Size,Num_Cores,Solver,Analysis_Time,Factor_Time,Solve_Time,Total_Time" > "$RESULTS_FILE"

# 矩阵配置
MATRIX_CONFIGS=(
    "1000 10000 0.01 perf_matrix_1000x10000.mtx"
    "10000 10000 0.01 perf_matrix_10000x10000.mtx"
)

CORES=(1 2 4)

# 生成测试矩阵
echo "Generating test matrices..."
echo "----------------------------------------"

for config in "${MATRIX_CONFIGS[@]}"; do
    read -r m n sparsity filename <<< "$config"
    MATRIX_FILE="tests/benchmark/test_matrices/$filename"
    
    if [ ! -f "$MATRIX_FILE" ]; then
        echo -n "  Generating ${m}x${n} matrix (sparsity: ${sparsity})... "
        if [ "$m" -eq "$n" ]; then
            ./build/create_matrix $m $sparsity "$MATRIX_FILE" > /dev/null 2>&1
        else
            ./build/create_rectangular_matrix $m $n $sparsity "$MATRIX_FILE" > /dev/null 2>&1
        fi
        echo "Done"
    else
        echo "  Matrix ${m}x${n} already exists, skipping"
    fi
done

echo ""
echo "Running performance tests..."
echo "=========================================="

# 测试每个矩阵
for config in "${MATRIX_CONFIGS[@]}"; do
    read -r m n sparsity filename <<< "$config"
    MATRIX_FILE="tests/benchmark/test_matrices/$filename"
    MATRIX_SIZE="${m}x${n}"
    
    if [ ! -f "$MATRIX_FILE" ]; then
        echo "Warning: Matrix file $MATRIX_FILE not found, skipping"
        continue
    fi
    
    echo ""
    echo "Matrix: ${MATRIX_SIZE}"
    echo "----------------------------------------"
    
    # 测试PARD
    echo "PARD:"
    for CORES in "${CORES[@]}"; do
        echo -n "  $CORES core(s): "
        
        OUTPUT=$(timeout 300 mpirun -np $CORES ./build/bin/benchmark "$MATRIX_FILE" 11 1 2>&1)
        
        ANALYSIS_TIME=$(echo "$OUTPUT" | grep -oP 'Analysis time:\s+\K[\d.]+' || echo "0")
        FACTOR_TIME=$(echo "$OUTPUT" | grep -oP 'Factorization time:\s+\K[\d.]+' || echo "0")
        SOLVE_TIME=$(echo "$OUTPUT" | grep -oP 'Solve time:\s+\K[\d.]+' || echo "0")
        TOTAL_TIME=$(echo "$OUTPUT" | grep -oP 'Total time:\s+\K[\d.]+' || echo "0")
        
        if [ "$TOTAL_TIME" = "0" ]; then
            TOTAL_TIME=$(echo "$ANALYSIS_TIME + $FACTOR_TIME + $SOLVE_TIME" | bc 2>/dev/null || echo "0")
        fi
        
        echo "$MATRIX_SIZE,$CORES,PARD,$ANALYSIS_TIME,$FACTOR_TIME,$SOLVE_TIME,$TOTAL_TIME" >> "$RESULTS_FILE"
        echo "Total: ${TOTAL_TIME}s"
    done
    
    # 测试MUMPS
    if [ -f "./build/mumps_benchmark" ]; then
        echo "MUMPS:"
        for CORES in "${CORES[@]}"; do
            echo -n "  $CORES core(s): "
            
            OUTPUT=$(timeout 300 mpirun -np $CORES ./build/mumps_benchmark "$MATRIX_FILE" 2>&1)
            
            ANALYSIS_TIME=$(echo "$OUTPUT" | grep -oP 'Analysis time:\s+\K[\d.]+' || echo "0")
            FACTOR_TIME=$(echo "$OUTPUT" | grep -oP 'Factorization time:\s+\K[\d.]+' || echo "0")
            SOLVE_TIME=$(echo "$OUTPUT" | grep -oP 'Solve time:\s+\K[\d.]+' || echo "0")
            TOTAL_TIME=$(echo "$OUTPUT" | grep -oP 'Total time:\s+\K[\d.]+' || echo "0")
            
            if [ "$TOTAL_TIME" = "0" ]; then
                TOTAL_TIME=$(echo "$ANALYSIS_TIME + $FACTOR_TIME + $SOLVE_TIME" | bc 2>/dev/null || echo "0")
            fi
            
            echo "$MATRIX_SIZE,$CORES,MUMPS,$ANALYSIS_TIME,$FACTOR_TIME,$SOLVE_TIME,$TOTAL_TIME" >> "$RESULTS_FILE"
            echo "Total: ${TOTAL_TIME}s"
        done
    else
        echo "MUMPS: benchmark not available (build/mumps_benchmark not found)"
    fi
done

echo ""
echo "=========================================="
echo "Performance testing completed!"
echo "Results saved to: $RESULTS_FILE"
echo "=========================================="
