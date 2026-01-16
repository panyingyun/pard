#!/bin/bash
# 快速性能测试（跳过100000x100000，因为太大）
set -e

cd "$(dirname "$0")/../.."

echo "=========================================="
echo "  PARD Quick Performance Test"
echo "=========================================="
echo ""

mkdir -p tests/benchmark/test_matrices
mkdir -p tests/benchmark/perf_results

RESULTS_FILE="tests/benchmark/perf_results/performance_results.csv"
echo "Matrix_Size,Num_Cores,Solver,Analysis_Time,Factor_Time,Solve_Time,Total_Time" > "$RESULTS_FILE"

# 只测试10x10和1000x1000
MATRIX_SIZES=(10 1000)
SPARSITIES=(0.3 0.01)
CORES=(1 2 4)

# 生成矩阵
echo "Generating test matrices..."
for i in "${!MATRIX_SIZES[@]}"; do
    SIZE=${MATRIX_SIZES[$i]}
    SPARSITY=${SPARSITIES[$i]}
    MATRIX_FILE="tests/benchmark/test_matrices/perf_matrix_${SIZE}x${SIZE}.mtx"
    
    if [ ! -f "$MATRIX_FILE" ]; then
        echo -n "  ${SIZE}x${SIZE}... "
        ./build/create_matrix $SIZE $SPARSITY "$MATRIX_FILE" > /dev/null 2>&1
        echo "Done"
    fi
done

echo ""
echo "Running tests..."
echo "=========================================="

for i in "${!MATRIX_SIZES[@]}"; do
    SIZE=${MATRIX_SIZES[$i]}
    MATRIX_FILE="tests/benchmark/test_matrices/perf_matrix_${SIZE}x${SIZE}.mtx"
    
    echo ""
    echo "Matrix: ${SIZE}x${SIZE}"
    
    for CORES in "${CORES[@]}"; do
        echo -n "  $CORES core(s): "
        
        OUTPUT=$(timeout 60 mpirun -np $CORES ./build/bin/benchmark "$MATRIX_FILE" 11 1 2>&1)
        
        ANALYSIS_TIME=$(echo "$OUTPUT" | grep -oP 'Analysis time:\s+\K[\d.]+' || echo "0")
        FACTOR_TIME=$(echo "$OUTPUT" | grep -oP 'Factorization time:\s+\K[\d.]+' || echo "0")
        SOLVE_TIME=$(echo "$OUTPUT" | grep -oP 'Solve time:\s+\K[\d.]+' || echo "0")
        TOTAL_TIME=$(echo "$OUTPUT" | grep -oP 'Total time:\s+\K[\d.]+' || echo "0")
        
        if [ "$TOTAL_TIME" = "0" ]; then
            TOTAL_TIME=$(echo "$ANALYSIS_TIME + $FACTOR_TIME + $SOLVE_TIME" | bc 2>/dev/null || echo "0")
        fi
        
        echo "$SIZE,$CORES,PARD,$ANALYSIS_TIME,$FACTOR_TIME,$SOLVE_TIME,$TOTAL_TIME" >> "$RESULTS_FILE"
        echo "Total: ${TOTAL_TIME}s"
    done
done

echo ""
echo "=========================================="
echo "Test completed! Results: $RESULTS_FILE"
echo "=========================================="
