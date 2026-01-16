#!/bin/bash
# 快速PARD vs MUMPS对比测试（带超时控制）

cd "$(dirname "$0")/../.."

RESULTS_FILE="tests/benchmark/perf_results/pard_mumps_comparison.csv"
echo "Matrix_Size,Num_Cores,Solver,Analysis_Time,Factor_Time,Solve_Time,Total_Time" > "$RESULTS_FILE"

MATRIX_CONFIGS=(
    "1000x10000 tests/benchmark/test_matrices/perf_matrix_1000x10000.mtx"
    "10000x10000 tests/benchmark/test_matrices/perf_matrix_10000x10000.mtx"
)

CORES=(1 2 4)

echo "Running PARD vs MUMPS comparison tests..."
echo "=========================================="

for config in "${MATRIX_CONFIGS[@]}"; do
    read -r size matrix_file <<< "$config"
    
    if [ ! -f "$matrix_file" ]; then
        echo "Warning: $matrix_file not found, skipping"
        continue
    fi
    
    echo ""
    echo "Matrix: $size"
    echo "----------------------------------------"
    
    # 测试PARD
    echo "PARD:"
    for cores in "${CORES[@]}"; do
        echo -n "  $cores core(s): "
        
        OUTPUT=$(timeout 120 mpirun -np $cores ./build/bin/benchmark "$matrix_file" 11 1 2>&1)
        
        A=$(echo "$OUTPUT" | grep -oP 'Analysis time:\s+\K[\d.]+' || echo "0")
        F=$(echo "$OUTPUT" | grep -oP 'Factorization time:\s+\K[\d.]+' || echo "0")
        S=$(echo "$OUTPUT" | grep -oP 'Solve time:\s+\K[\d.]+' || echo "0")
        T=$(echo "$OUTPUT" | grep -oP 'Total time:\s+\K[\d.]+' || echo "0")
        
        if [ "$T" = "0" ]; then
            T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0")
        fi
        
        echo "$size,$cores,PARD,$A,$F,$S,$T" >> "$RESULTS_FILE"
        echo "Total: ${T}s"
    done
    
    # 测试MUMPS
    if [ -f "./build/mumps_benchmark" ]; then
        echo "MUMPS:"
        for cores in "${CORES[@]}"; do
            echo -n "  $cores core(s): "
            
            OUTPUT=$(timeout 120 mpirun -np $cores ./build/mumps_benchmark "$matrix_file" 2>&1)
            
            A=$(echo "$OUTPUT" | grep -oP 'Analysis time:\s+\K[\d.]+' || echo "0")
            F=$(echo "$OUTPUT" | grep -oP 'Factorization time:\s+\K[\d.]+' || echo "0")
            S=$(echo "$OUTPUT" | grep -oP 'Solve time:\s+\K[\d.]+' || echo "0")
            T=$(echo "$OUTPUT" | grep -oP 'Total time:\s+\K[\d.]+' || echo "0")
            
            if [ "$T" = "0" ]; then
                T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0")
            fi
            
            echo "$size,$cores,MUMPS,$A,$F,$S,$T" >> "$RESULTS_FILE"
            echo "Total: ${T}s"
        done
    else
        echo "MUMPS: benchmark not available"
    fi
done

echo ""
echo "Results saved to: $RESULTS_FILE"
