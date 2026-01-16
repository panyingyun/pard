#!/bin/bash
# 逐步运行测试，收集真实数据

cd "$(dirname "$0")/../.."

RESULTS_FILE="tests/benchmark/perf_results/pard_mumps_comparison.csv"
echo "Matrix_Size,Num_Cores,Solver,Analysis_Time,Factor_Time,Solve_Time,Total_Time" > "$RESULTS_FILE"

echo "Running PARD vs MUMPS comparison tests..."
echo "=========================================="

# 测试1000x1000
echo ""
echo "Matrix: 1000x1000"
echo "----------------------------------------"

# PARD 1核
echo -n "PARD 1 core: "
OUTPUT=$(timeout 60 ./build/bin/benchmark tests/benchmark/test_matrices/perf_matrix_1000x1000.mtx 11 0 2>&1)
A=$(echo "$OUTPUT" | grep -oP 'Analysis time:\s+\K[\d.]+' || echo "0")
F=$(echo "$OUTPUT" | grep -oP 'Factorization time:\s+\K[\d.]+' || echo "0")
S=$(echo "$OUTPUT" | grep -oP 'Solve time:\s+\K[\d.]+' || echo "0")
T=$(echo "$OUTPUT" | grep -oP 'Total time:\s+\K[\d.]+' || echo "0")
if [ "$T" = "0" ] && [ "$A" != "0" ]; then
    T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0")
fi
if [ "$T" != "0" ]; then
    echo "1000x1000,1,PARD,$A,$F,$S,$T" >> "$RESULTS_FILE"
    echo "Total: ${T}s"
else
    echo "FAILED"
fi

# MUMPS 1核
echo -n "MUMPS 1 core: "
OUTPUT=$(timeout 60 ./build/mumps_benchmark tests/benchmark/test_matrices/perf_matrix_1000x1000.mtx 2>&1)
A=$(echo "$OUTPUT" | grep -oP 'Analysis time:\s+\K[\d.]+' || echo "0")
F=$(echo "$OUTPUT" | grep -oP 'Factorization time:\s+\K[\d.]+' || echo "0")
S=$(echo "$OUTPUT" | grep -oP 'Solve time:\s+\K[\d.]+' || echo "0")
T=$(echo "$OUTPUT" | grep -oP 'Total time:\s+\K[\d.]+' || echo "0")
if [ "$T" = "0" ] && [ "$A" != "0" ]; then
    T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0")
fi
if [ "$T" != "0" ]; then
    echo "1000x1000,1,MUMPS,$A,$F,$S,$T" >> "$RESULTS_FILE"
    echo "Total: ${T}s"
else
    echo "FAILED"
fi

# 测试2核和4核（如果时间允许）
for cores in 2 4; do
    echo -n "PARD $cores cores: "
    OUTPUT=$(timeout 120 mpirun -np $cores ./build/bin/benchmark tests/benchmark/test_matrices/perf_matrix_1000x1000.mtx 11 1 2>&1)
    A=$(echo "$OUTPUT" | grep -oP 'Analysis time:\s+\K[\d.]+' || echo "0")
    F=$(echo "$OUTPUT" | grep -oP 'Factorization time:\s+\K[\d.]+' || echo "0")
    S=$(echo "$OUTPUT" | grep -oP 'Solve time:\s+\K[\d.]+' || echo "0")
    T=$(echo "$OUTPUT" | grep -oP 'Total time:\s+\K[\d.]+' || echo "0")
    if [ "$T" = "0" ] && [ "$A" != "0" ]; then
        T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0")
    fi
    if [ "$T" != "0" ]; then
        echo "1000x1000,$cores,PARD,$A,$F,$S,$T" >> "$RESULTS_FILE"
        echo "Total: ${T}s"
    else
        echo "FAILED"
    fi
    
    echo -n "MUMPS $cores cores: "
    OUTPUT=$(timeout 120 mpirun -np $cores ./build/mumps_benchmark tests/benchmark/test_matrices/perf_matrix_1000x1000.mtx 2>&1)
    A=$(echo "$OUTPUT" | grep -oP 'Analysis time:\s+\K[\d.]+' || echo "0")
    F=$(echo "$OUTPUT" | grep -oP 'Factorization time:\s+\K[\d.]+' || echo "0")
    S=$(echo "$OUTPUT" | grep -oP 'Solve time:\s+\K[\d.]+' || echo "0")
    T=$(echo "$OUTPUT" | grep -oP 'Total time:\s+\K[\d.]+' || echo "0")
    if [ "$T" = "0" ] && [ "$A" != "0" ]; then
        T=$(echo "$A + $F + $S" | bc 2>/dev/null || echo "0")
    fi
    if [ "$T" != "0" ]; then
        echo "1000x1000,$cores,MUMPS,$A,$F,$S,$T" >> "$RESULTS_FILE"
        echo "Total: ${T}s"
    else
        echo "FAILED"
    fi
done

echo ""
echo "Results saved to: $RESULTS_FILE"
cat "$RESULTS_FILE"
