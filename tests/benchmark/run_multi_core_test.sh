#!/bin/bash

# PARD和MUMPS多核性能对比测试脚本
# 测试矩阵: 1000×1000, 10000×10000
# 测试核心数: 1, 2, 4, 8

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RESULTS_DIR="$SCRIPT_DIR/perf_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_FILE="$RESULTS_DIR/multi_core_performance_${TIMESTAMP}.csv"
REPORT_FILE="$RESULTS_DIR/MULTI_CORE_PERFORMANCE_REPORT.md"

# 创建结果目录
mkdir -p "$RESULTS_DIR"

# 测试配置
MATRIX_SIZES=(1000 10000)
CORES=(1 2 4 8)

echo "=========================================="
echo "PARD vs MUMPS 多核性能对比测试"
echo "=========================================="
echo ""
echo "测试矩阵: ${MATRIX_SIZES[@]}"
echo "测试核心数: ${CORES[@]}"
echo "结果文件: $RESULTS_FILE"
echo ""

# CSV文件头
echo "MatrixSize,Cores,Solver,AnalysisTime,FactorTime,SolveTime,TotalTime" > "$RESULTS_FILE"

# 提取时间的辅助函数
extract_time() {
    local output="$1"
    local pattern="$2"
    local time=$(echo "$output" | grep -i "$pattern" | grep -oE '[0-9]+\.[0-9]+' | head -1)
    if [ -z "$time" ]; then
        echo "0.0"
    else
        echo "$time"
    fi
}

# 测试每个矩阵
for SIZE in "${MATRIX_SIZES[@]}"; do
    MATRIX_FILE="$PROJECT_ROOT/tests/benchmark/test_matrices/perf_matrix_${SIZE}x${SIZE}.mtx"
    
    if [ ! -f "$MATRIX_FILE" ]; then
        echo "警告: 矩阵文件 $MATRIX_FILE 不存在，跳过"
        continue
    fi
    
    echo ""
    echo "=========================================="
    echo "测试矩阵: ${SIZE}×${SIZE}"
    echo "=========================================="
    
    # 测试PARD
    echo ""
    echo "--- PARD 测试 ---"
    for NP in "${CORES[@]}"; do
        echo -n "  PARD ${NP}核: "
        
        OUTPUT=$(timeout 600 mpirun -np $NP "$PROJECT_ROOT/build/bin/benchmark" "$MATRIX_FILE" 11 1 2>&1 || echo "")
        
        # 检查是否有错误
        if echo "$OUTPUT" | grep -qi "error\|fault\|fail"; then
            echo "失败（运行错误）"
            echo "$SIZE,$NP,PARD,0.0,0.0,0.0,0.0" >> "$RESULTS_FILE"
            continue
        fi
        
        # 提取时间信息
        ANALYSIS_TIME=$(extract_time "$OUTPUT" "Analysis time")
        FACTOR_TIME=$(extract_time "$OUTPUT" "Factorization time")
        SOLVE_TIME=$(extract_time "$OUTPUT" "Solve time")
        TOTAL_TIME=$(extract_time "$OUTPUT" "Total time")
        
        # 如果总时间为0或空，计算总和
        if [ "$TOTAL_TIME" = "0.0" ] || [ -z "$TOTAL_TIME" ]; then
            if [ "$ANALYSIS_TIME" != "0.0" ] && [ "$FACTOR_TIME" != "0.0" ] && [ "$SOLVE_TIME" != "0.0" ]; then
                TOTAL_TIME=$(echo "$ANALYSIS_TIME + $FACTOR_TIME + $SOLVE_TIME" | bc 2>/dev/null || echo "0.0")
            fi
        fi
        
        # 如果所有时间都是0，可能程序崩溃了
        if [ "$TOTAL_TIME" = "0.0" ] && [ "$ANALYSIS_TIME" = "0.0" ] && [ "$FACTOR_TIME" = "0.0" ] && [ "$SOLVE_TIME" = "0.0" ]; then
            echo "失败（无输出或崩溃）"
        else
            echo "总时间: ${TOTAL_TIME}s"
        fi
        
        echo "$SIZE,$NP,PARD,$ANALYSIS_TIME,$FACTOR_TIME,$SOLVE_TIME,$TOTAL_TIME" >> "$RESULTS_FILE"
        
        sleep 1  # 短暂休息
    done
    
    # 测试MUMPS
    echo ""
    echo "--- MUMPS 测试 ---"
    for NP in "${CORES[@]}"; do
        echo -n "  MUMPS ${NP}核: "
        
        OUTPUT=$(timeout 600 mpirun -np $NP "$PROJECT_ROOT/build/mumps_benchmark" "$MATRIX_FILE" 2>&1 || echo "")
        
        # 检查是否有错误
        if echo "$OUTPUT" | grep -qi "error\|fault\|fail"; then
            echo "失败（运行错误）"
            echo "$SIZE,$NP,MUMPS,0.0,0.0,0.0,0.0" >> "$RESULTS_FILE"
            continue
        fi
        
        # 提取时间信息
        ANALYSIS_TIME=$(extract_time "$OUTPUT" "Analysis time")
        FACTOR_TIME=$(extract_time "$OUTPUT" "Factorization time")
        SOLVE_TIME=$(extract_time "$OUTPUT" "Solve time")
        TOTAL_TIME=$(extract_time "$OUTPUT" "Total time")
        
        # 如果总时间为0或空，计算总和
        if [ "$TOTAL_TIME" = "0.0" ] || [ -z "$TOTAL_TIME" ]; then
            if [ "$ANALYSIS_TIME" != "0.0" ] && [ "$FACTOR_TIME" != "0.0" ] && [ "$SOLVE_TIME" != "0.0" ]; then
                TOTAL_TIME=$(echo "$ANALYSIS_TIME + $FACTOR_TIME + $SOLVE_TIME" | bc 2>/dev/null || echo "0.0")
            fi
        fi
        
        # 如果所有时间都是0，可能程序崩溃了
        if [ "$TOTAL_TIME" = "0.0" ] && [ "$ANALYSIS_TIME" = "0.0" ] && [ "$FACTOR_TIME" = "0.0" ] && [ "$SOLVE_TIME" = "0.0" ]; then
            echo "失败（无输出或崩溃）"
        else
            echo "总时间: ${TOTAL_TIME}s"
        fi
        
        echo "$SIZE,$NP,MUMPS,$ANALYSIS_TIME,$FACTOR_TIME,$SOLVE_TIME,$TOTAL_TIME" >> "$RESULTS_FILE"
        
        sleep 1  # 短暂休息
    done
    
    echo ""
done

echo ""
echo "=========================================="
echo "测试完成！"
echo "结果文件: $RESULTS_FILE"
echo "=========================================="

# 生成报告
if command -v python3 &> /dev/null; then
    echo ""
    echo "生成性能报告..."
    python3 << 'EOF'
import sys
import os
import csv
import math
from datetime import datetime

def read_csv_data(csv_file):
    data = {}
    try:
        with open(csv_file, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                size = int(row['MatrixSize'])
                cores = int(row['Cores'])
                solver = row['Solver']
                analysis = float(row['AnalysisTime']) if row['AnalysisTime'] else 0.0
                factor = float(row['FactorTime']) if row['FactorTime'] else 0.0
                solve = float(row['SolveTime']) if row['SolveTime'] else 0.0
                total = float(row['TotalTime']) if row['TotalTime'] else 0.0
                
                key = (size, cores, solver)
                data[key] = {
                    'analysis': analysis,
                    'factor': factor,
                    'solve': solve,
                    'total': total
                }
    except Exception as e:
        print(f"读取CSV文件出错: {e}")
    return data

def calculate_speedup(data, size, solver):
    base_key = (size, 1, solver)
    if base_key not in data or data[base_key]['total'] <= 0:
        return {}
    
    base_time = data[base_key]['total']
    speedups = {}
    
    for cores in [1, 2, 4, 8]:
        key = (size, cores, solver)
        if key in data and data[key]['total'] > 0:
            speedups[cores] = base_time / data[key]['total']
        else:
            speedups[cores] = 0.0
    
    return speedups

def calculate_efficiency(speedups):
    efficiencies = {}
    for cores, speedup in speedups.items():
        if cores > 0:
            efficiencies[cores] = speedup / cores * 100.0
        else:
            efficiencies[cores] = 0.0
    return efficiencies

def generate_report(csv_file, report_file):
    data = read_csv_data(csv_file)
    
    with open(report_file, 'w', encoding='utf-8') as f:
        f.write("# PARD vs MUMPS 多核性能对比报告\n\n")
        f.write(f"**生成时间**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write("## 测试配置\n\n")
        f.write("- **测试矩阵**: 1000×1000, 10000×10000\n")
        f.write("- **测试核心数**: 1, 2, 4, 8\n")
        f.write("- **MUMPS版本**: 5.8.1 (使用OpenBLAS)\n\n")
        f.write("---\n\n")
        
        # 为每个矩阵大小生成报告
        for size in [1000, 10000]:
            f.write(f"## {size}×{size} 矩阵性能结果\n\n")
            
            # 性能数据表
            f.write("### 执行时间 (秒)\n\n")
            f.write("| 核心数 | PARD分析 | PARD分解 | PARD求解 | PARD总计 | MUMPS分析 | MUMPS分解 | MUMPS求解 | MUMPS总计 |\n")
            f.write("|--------|----------|----------|----------|----------|-----------|-----------|-----------|----------|\n")
            
            for cores in [1, 2, 4, 8]:
                pard_key = (size, cores, 'PARD')
                mumps_key = (size, cores, 'MUMPS')
                
                pard_data = data.get(pard_key, {})
                mumps_data = data.get(mumps_key, {})
                
                pard_a = pard_data.get('analysis', 0.0)
                pard_f = pard_data.get('factor', 0.0)
                pard_s = pard_data.get('solve', 0.0)
                pard_t = pard_data.get('total', 0.0)
                
                mumps_a = mumps_data.get('analysis', 0.0)
                mumps_f = mumps_data.get('factor', 0.0)
                mumps_s = mumps_data.get('solve', 0.0)
                mumps_t = mumps_data.get('total', 0.0)
                
                f.write(f"| {cores} | {pard_a:.6f} | {pard_f:.6f} | {pard_s:.6f} | **{pard_t:.6f}** | {mumps_a:.6f} | {mumps_f:.6f} | {mumps_s:.6f} | **{mumps_t:.6f}** |\n")
            
            f.write("\n")
            
            # 加速比
            f.write("### 加速比 (相对于1核)\n\n")
            f.write("| 核心数 | PARD加速比 | PARD效率(%) | MUMPS加速比 | MUMPS效率(%) |\n")
            f.write("|--------|------------|-------------|-------------|--------------|\n")
            
            pard_speedups = calculate_speedup(data, size, 'PARD')
            mumps_speedups = calculate_speedup(data, size, 'MUMPS')
            pard_efficiency = calculate_efficiency(pard_speedups)
            mumps_efficiency = calculate_efficiency(mumps_speedups)
            
            for cores in [1, 2, 4, 8]:
                pard_s = pard_speedups.get(cores, 0.0)
                mumps_s = mumps_speedups.get(cores, 0.0)
                pard_e = pard_efficiency.get(cores, 0.0)
                mumps_e = mumps_efficiency.get(cores, 0.0)
                
                f.write(f"| {cores} | {pard_s:.2f}x | {pard_e:.1f}% | {mumps_s:.2f}x | {mumps_e:.1f}% |\n")
            
            f.write("\n")
            
            # PARD vs MUMPS 对比
            f.write("### PARD vs MUMPS 性能对比\n\n")
            f.write("| 核心数 | PARD时间(s) | MUMPS时间(s) | PARD更快(倍) | 状态 |\n")
            f.write("|--------|-------------|---------------|-------------|------|\n")
            
            for cores in [1, 2, 4, 8]:
                pard_key = (size, cores, 'PARD')
                mumps_key = (size, cores, 'MUMPS')
                
                pard_t = data.get(pard_key, {}).get('total', 0.0)
                mumps_t = data.get(mumps_key, {}).get('total', 0.0)
                
                if pard_t > 0 and mumps_t > 0:
                    ratio = mumps_t / pard_t
                    status = "✅ PARD更快" if pard_t < mumps_t else "⚠️ MUMPS更快"
                    f.write(f"| {cores} | {pard_t:.6f} | {mumps_t:.6f} | {ratio:.2f}x | {status} |\n")
                else:
                    f.write(f"| {cores} | - | - | - | ❌ 数据缺失 |\n")
            
            f.write("\n---\n\n")
        
        f.write("**报告生成完成**\n")

# 主程序
if __name__ == "__main__":
    csv_file = "$RESULTS_FILE"
    report_file = "$REPORT_FILE"
    
    if os.path.exists(csv_file):
        generate_report(csv_file, report_file)
        print(f"报告已生成: {report_file}")
    else:
        print(f"CSV文件不存在: {csv_file}")
EOF
fi
