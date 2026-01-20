#!/bin/bash
# 收集性能数据脚本

set -e

cd "$(dirname "$0")/../.."

echo "=========================================="
echo "PARD vs MUMPS Performance Collection"
echo "=========================================="
echo ""

# 测试配置
SIZES=(500 1000)
CORES=(1)
MTYPE=11

RESULTS_FILE="tests/benchmark/performance_results_$(date +%Y%m%d_%H%M%S).txt"

echo "Matrix_Size Cores Solver Analysis Factor Solve Total" > "$RESULTS_FILE"

for size in "${SIZES[@]}"; do
    for cores in "${CORES[@]}"; do
        echo ""
        echo "=== Testing ${size}x${size} with $cores cores ==="
        
        # PARD测试
        echo "PARD:"
        PARD_OUTPUT=$(mpirun -np $cores ./build/perf_test_with_create $size $MTYPE 2>&1)
        echo "$PARD_OUTPUT" | grep -E "Performance Statistics|Analysis time|Factorization time|Solve time|Total time"
        
        PARD_ANALYSIS=$(echo "$PARD_OUTPUT" | grep "Analysis time:" | awk '{print $3}')
        PARD_FACTOR=$(echo "$PARD_OUTPUT" | grep "Factorization time:" | awk '{print $3}')
        PARD_SOLVE=$(echo "$PARD_OUTPUT" | grep "Solve time:" | awk '{print $3}')
        PARD_TOTAL=$(echo "$PARD_OUTPUT" | grep "Total time:" | awk '{print $3}')
        
        echo "${size} ${cores} PARD ${PARD_ANALYSIS} ${PARD_FACTOR} ${PARD_SOLVE} ${PARD_TOTAL}" >> "$RESULTS_FILE"
        
        # MUMPS测试
        echo "MUMPS:"
        MATRIX_FILE="tests/benchmark/test_matrices/perf_matrix_${size}x${size}.mtx"
        if [ -f "$MATRIX_FILE" ] && [ -f "build/mumps_benchmark" ]; then
            MUMPS_OUTPUT=$(mpirun -np $cores ./build/mumps_benchmark "$MATRIX_FILE" 2>&1)
            echo "$MUMPS_OUTPUT" | grep -E "Performance Statistics|Analysis time|Factorization time|Solve time|Total time"
            
            MUMPS_ANALYSIS=$(echo "$MUMPS_OUTPUT" | grep "Analysis time:" | awk '{print $3}')
            MUMPS_FACTOR=$(echo "$MUMPS_OUTPUT" | grep "Factorization time:" | awk '{print $3}')
            MUMPS_SOLVE=$(echo "$MUMPS_OUTPUT" | grep "Solve time:" | awk '{print $3}')
            MUMPS_TOTAL=$(echo "$MUMPS_OUTPUT" | grep "Total time:" | awk '{print $3}')
            
            echo "${size} ${cores} MUMPS ${MUMPS_ANALYSIS} ${MUMPS_FACTOR} ${MUMPS_SOLVE} ${MUMPS_TOTAL}" >> "$RESULTS_FILE"
            
            # 比较
            if [ -n "$PARD_TOTAL" ] && [ -n "$MUMPS_TOTAL" ]; then
                RATIO=$(echo "$MUMPS_TOTAL $PARD_TOTAL" | awk '{printf "%.2f", $1/$2}')
                echo "  Ratio (MUMPS/PARD): ${RATIO}x"
                if (( $(echo "$PARD_TOTAL < $MUMPS_TOTAL" | bc -l) )); then
                    echo "  ✓ PARD is FASTER!"
                else
                    echo "  ✗ PARD needs optimization (${RATIO}x slower)"
                fi
            fi
        fi
    done
done

echo ""
echo "Results saved to: $RESULTS_FILE"
cat "$RESULTS_FILE"
