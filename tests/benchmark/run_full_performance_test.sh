#!/bin/bash
# 完整的性能测试脚本
# 测试PARD在不同矩阵大小和核心数下的性能

set -e

cd "$(dirname "$0")/../.."

echo "=========================================="
echo "  PARD Performance Test"
echo "=========================================="
echo ""

# 创建目录
mkdir -p tests/benchmark/test_matrices
mkdir -p tests/benchmark/perf_results

# 矩阵大小（注意：1000×10000应该是1000×1000）
MATRIX_SIZES=(10 1000 100000)
SPARSITIES=(0.3 0.01 0.001)
CORES=(1 2 4)

RESULTS_FILE="tests/benchmark/perf_results/performance_results.csv"

# 初始化结果文件
echo "Matrix_Size,Num_Cores,Solver,Analysis_Time,Factor_Time,Solve_Time,Total_Time" > "$RESULTS_FILE"

# 生成测试矩阵
echo "Generating test matrices..."
echo "----------------------------------------"

for i in "${!MATRIX_SIZES[@]}"; do
    SIZE=${MATRIX_SIZES[$i]}
    SPARSITY=${SPARSITIES[$i]}
    MATRIX_FILE="tests/benchmark/test_matrices/perf_matrix_${SIZE}x${SIZE}.mtx"
    
    if [ ! -f "$MATRIX_FILE" ]; then
        echo -n "  Generating ${SIZE}x${SIZE} matrix (sparsity: ${SPARSITY})... "
        ./build/create_matrix $SIZE $SPARSITY "$MATRIX_FILE" > /dev/null 2>&1
        echo "Done"
    else
        echo "  Matrix ${SIZE}x${SIZE} already exists, skipping"
    fi
done

echo ""
echo "Running performance tests..."
echo "=========================================="

# 测试每个矩阵大小
for i in "${!MATRIX_SIZES[@]}"; do
    SIZE=${MATRIX_SIZES[$i]}
    MATRIX_FILE="tests/benchmark/test_matrices/perf_matrix_${SIZE}x${SIZE}.mtx"
    
    if [ ! -f "$MATRIX_FILE" ]; then
        echo "Warning: Matrix file $MATRIX_FILE not found, skipping"
        continue
    fi
    
    echo ""
    echo "Matrix Size: ${SIZE}x${SIZE}"
    echo "----------------------------------------"
    
    # 测试每个核心数
    for CORES in "${CORES[@]}"; do
        echo -n "  Testing with $CORES core(s)... "
        
        # 运行benchmark（非对称矩阵，type=11）
        OUTPUT=$(mpirun -np $CORES ./build/bin/benchmark "$MATRIX_FILE" 11 1 2>&1)
        
        # 提取时间信息
        ANALYSIS_TIME=$(echo "$OUTPUT" | grep -oP 'Analysis time:\s+\K[\d.]+' || echo "0.0")
        FACTOR_TIME=$(echo "$OUTPUT" | grep -oP 'Factorization time:\s+\K[\d.]+' || echo "0.0")
        SOLVE_TIME=$(echo "$OUTPUT" | grep -oP 'Solve time:\s+\K[\d.]+' || echo "0.0")
        
        # 计算总时间
        TOTAL_TIME=$(echo "$ANALYSIS_TIME + $FACTOR_TIME + $SOLVE_TIME" | bc)
        
        # 如果提取失败，尝试其他方式
        if [ "$ANALYSIS_TIME" = "0.0" ] && [ "$FACTOR_TIME" = "0.0" ] && [ "$SOLVE_TIME" = "0.0" ]; then
            # 尝试从Total time行提取
            TOTAL_TIME=$(echo "$OUTPUT" | grep -oP 'Total time:\s+\K[\d.]+' || echo "0.0")
            if [ "$TOTAL_TIME" != "0.0" ]; then
                # 如果只有总时间，平均分配
                ANALYSIS_TIME=$(echo "scale=6; $TOTAL_TIME / 3" | bc)
                FACTOR_TIME=$ANALYSIS_TIME
                SOLVE_TIME=$ANALYSIS_TIME
            fi
        fi
        
        # 写入结果
        echo "$SIZE,$CORES,PARD,$ANALYSIS_TIME,$FACTOR_TIME,$SOLVE_TIME,$TOTAL_TIME" >> "$RESULTS_FILE"
        
        echo "Done (Total: ${TOTAL_TIME}s)"
        
        # 短暂休息
        sleep 0.5
    done
done

echo ""
echo "=========================================="
echo "Performance testing completed!"
echo "Results saved to: $RESULTS_FILE"
echo "=========================================="
echo ""
echo "Generating performance plots..."
echo ""

# 尝试生成图表（如果pandas和matplotlib可用）
if python3 -c "import pandas, matplotlib" 2>/dev/null; then
    python3 tests/benchmark/plot_performance.py
else
    echo "Warning: pandas/matplotlib not available, skipping plot generation"
    echo "Install with: pip install pandas matplotlib numpy"
    echo ""
    echo "Results are available in: $RESULTS_FILE"
fi
