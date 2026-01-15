#!/bin/bash
# 性能测试脚本
# 测试不同大小的矩阵在不同核心数下的性能

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/../.."

# 输出文件
OUTPUT_DIR="tests/benchmark/perf_results"
mkdir -p "$OUTPUT_DIR"

RESULTS_FILE="$OUTPUT_DIR/performance_results.csv"

# 初始化结果文件
echo "Matrix_Size,Num_Cores,Solver,Analysis_Time,Factor_Time,Solve_Time,Total_Time" > "$RESULTS_FILE"

# 矩阵大小和对应的文件
declare -A MATRIX_FILES=(
    ["10"]="tests/benchmark/test_matrices/perf_matrix_10x10.mtx"
    ["1000"]="tests/benchmark/test_matrices/perf_matrix_1000x1000.mtx"
    ["100000"]="tests/benchmark/test_matrices/perf_matrix_100000x100000.mtx"
)

# 核心数
CORES=(1 2 4)

echo "=========================================="
echo "  PARD Performance Benchmark"
echo "=========================================="
echo ""

# 生成测试矩阵
echo "Generating test matrices..."
python3 tests/benchmark/create_perf_matrices.py

echo ""
echo "Starting performance tests..."
echo ""

# 对每个矩阵大小进行测试
for SIZE in 10 1000 100000; do
    MATRIX_FILE="${MATRIX_FILES[$SIZE]}"
    
    # 检查矩阵文件是否存在
    if [ ! -f "$MATRIX_FILE" ]; then
        echo "Warning: Matrix file $MATRIX_FILE not found, skipping size $SIZE"
        continue
    fi
    
    echo "Testing matrix size: ${SIZE}x${SIZE}"
    echo "----------------------------------------"
    
    # 对每个核心数进行测试
    for CORES in 1 2 4; do
        echo -n "  Testing with $CORES core(s)... "
        
        # 运行PARD benchmark
        OUTPUT=$(mpirun -np $CORES ./build/bin/benchmark "$MATRIX_FILE" 11 1 2>&1)
        
        # 提取时间信息
        ANALYSIS_TIME=$(echo "$OUTPUT" | grep "Analysis time" | awk '{print $3}')
        FACTOR_TIME=$(echo "$OUTPUT" | grep "Factorization time" | awk '{print $3}')
        SOLVE_TIME=$(echo "$OUTPUT" | grep "Solve time" | awk '{print $3}')
        TOTAL_TIME=$(echo "$OUTPUT" | grep "Total time" | awk '{print $3}')
        
        # 如果提取失败，尝试其他方式
        if [ -z "$ANALYSIS_TIME" ]; then
            ANALYSIS_TIME=$(echo "$OUTPUT" | grep -oP 'Analysis time:\s+\K[\d.]+' || echo "0.0")
        fi
        if [ -z "$FACTOR_TIME" ]; then
            FACTOR_TIME=$(echo "$OUTPUT" | grep -oP 'Factorization time:\s+\K[\d.]+' || echo "0.0")
        fi
        if [ -z "$SOLVE_TIME" ]; then
            SOLVE_TIME=$(echo "$OUTPUT" | grep -oP 'Solve time:\s+\K[\d.]+' || echo "0.0")
        fi
        if [ -z "$TOTAL_TIME" ]; then
            TOTAL_TIME=$(echo "$OUTPUT" | grep -oP 'Total time:\s+\K[\d.]+' || echo "0.0")
        fi
        
        # 写入结果
        echo "$SIZE,$CORES,PARD,$ANALYSIS_TIME,$FACTOR_TIME,$SOLVE_TIME,$TOTAL_TIME" >> "$RESULTS_FILE"
        
        echo "Done (Total: ${TOTAL_TIME}s)"
        
        # 短暂休息，避免系统负载影响
        sleep 1
    done
    
    echo ""
done

echo "=========================================="
echo "Performance testing completed!"
echo "Results saved to: $RESULTS_FILE"
echo "=========================================="
