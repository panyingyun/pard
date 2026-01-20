#!/bin/bash
# 综合性能测试脚本：PARD vs MUMPS

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

echo "=========================================="
echo "PARD vs MUMPS Performance Comparison"
echo "=========================================="
echo ""

# 测试配置
SIZES=(500 1000)
CORES=(1 2 4)
MTYPE=11

# 结果数组
declare -A PARD_TIMES
declare -A MUMPS_TIMES

for size in "${SIZES[@]}"; do
    echo "=========================================="
    echo "Matrix Size: ${size}x${size}"
    echo "=========================================="
    
    for cores in "${CORES[@]}"; do
        echo ""
        echo "--- Testing with $cores cores ---"
        
        # 测试PARD
        echo "PARD:"
        PARD_OUTPUT=$(mpirun -np $cores ./build/perf_test_with_create $size $MTYPE 2>&1)
        PARD_TOTAL=$(echo "$PARD_OUTPUT" | grep "Total time:" | grep -oE '[0-9]+\.[0-9]+' || echo "0")
        PARD_FACTOR=$(echo "$PARD_OUTPUT" | grep "Factorization time:" | grep -oE '[0-9]+\.[0-9]+' || echo "0")
        PARD_ANALYSIS=$(echo "$PARD_OUTPUT" | grep "Analysis time:" | grep -oE '[0-9]+\.[0-9]+' || echo "0")
        PARD_SOLVE=$(echo "$PARD_OUTPUT" | grep "Solve time:" | grep -oE '[0-9]+\.[0-9]+' || echo "0")
        
        if [ "$PARD_TOTAL" != "0" ]; then
            PARD_TIMES["${size}_${cores}"]="$PARD_TOTAL"
            echo "  Analysis: ${PARD_ANALYSIS}s, Factor: ${PARD_FACTOR}s, Solve: ${PARD_SOLVE}s, Total: ${PARD_TOTAL}s"
        else
            echo "  ERROR: PARD test failed"
        fi
        
        # 测试MUMPS
        echo "MUMPS:"
        MATRIX_FILE="tests/benchmark/test_matrices/perf_matrix_${size}x${size}.mtx"
        if [ -f "$MATRIX_FILE" ] && [ -f "build/mumps_benchmark" ]; then
            MUMPS_OUTPUT=$(timeout 120 mpirun -np $cores ./build/mumps_benchmark "$MATRIX_FILE" 2>&1 || echo "")
            # MUMPS输出格式可能不同，尝试多种提取方式
            MUMPS_TOTAL=$(echo "$MUMPS_OUTPUT" | grep -iE "total|elapsed|time" | grep -oE '[0-9]+\.[0-9]+' | tail -1 || echo "")
            
            if [ -z "$MUMPS_TOTAL" ] || [ "$MUMPS_TOTAL" = "0" ]; then
                # 尝试从所有数字中提取最后一个（可能是总时间）
                MUMPS_TOTAL=$(echo "$MUMPS_OUTPUT" | grep -oE '[0-9]+\.[0-9]+' | tail -1 || echo "0")
            fi
            
            if [ "$MUMPS_TOTAL" != "0" ] && [ -n "$MUMPS_TOTAL" ]; then
                MUMPS_TIMES["${size}_${cores}"]="$MUMPS_TOTAL"
                echo "  Total: ${MUMPS_TOTAL}s"
            else
                echo "  ERROR: Could not extract MUMPS time or test failed"
                echo "$MUMPS_OUTPUT" | tail -10
            fi
        else
            echo "  SKIP: Matrix file or MUMPS benchmark not found"
        fi
        
        # 比较性能
        if [ -n "${PARD_TIMES[${size}_${cores}]}" ] && [ -n "${MUMPS_TIMES[${size}_${cores}]}" ]; then
            PARD_VAL=${PARD_TIMES[${size}_${cores}]}
            MUMPS_VAL=${MUMPS_TIMES[${size}_${cores}]}
            
            # 使用awk进行浮点数比较
            COMPARE=$(echo "$PARD_VAL $MUMPS_VAL" | awk '{if ($1 < $2) print "FASTER"; else print "SLOWER"}')
            SPEEDUP=$(echo "$MUMPS_VAL $PARD_VAL" | awk '{printf "%.2f", $1/$2}')
            
            echo ""
            if [ "$COMPARE" = "FASTER" ]; then
                echo "  ✓ PARD is ${SPEEDUP}x FASTER than MUMPS!"
            else
                echo "  ✗ PARD is ${SPEEDUP}x SLOWER than MUMPS (needs optimization)"
            fi
        fi
    done
done

echo ""
echo "=========================================="
echo "Summary:"
echo "=========================================="
for size in "${SIZES[@]}"; do
    for cores in "${CORES[@]}"; do
        key="${size}_${cores}"
        if [ -n "${PARD_TIMES[$key]}" ] && [ -n "${MUMPS_TIMES[$key]}" ]; then
            echo "${size}x${size}, ${cores} cores: PARD=${PARD_TIMES[$key]}s, MUMPS=${MUMPS_TIMES[$key]}s"
        fi
    done
done
