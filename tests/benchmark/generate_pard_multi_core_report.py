#!/usr/bin/env python3
"""
生成PARD多核性能报告
"""

import sys
import os
import csv
from datetime import datetime

def read_csv_data(csv_file):
    """读取CSV性能数据"""
    data = {}
    if not os.path.exists(csv_file):
        return data
    
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
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
            except (ValueError, KeyError) as e:
                continue
    return data

def calculate_speedup(data, size, solver):
    """计算加速比（相对于1核）"""
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
    """计算并行效率"""
    efficiencies = {}
    for cores, speedup in speedups.items():
        if cores > 0:
            efficiencies[cores] = speedup / cores * 100.0
        else:
            efficiencies[cores] = 0.0
    return efficiencies

def generate_report(csv_file, report_file):
    """生成Markdown报告"""
    data = read_csv_data(csv_file)
    
    if not data:
        print(f"警告: CSV文件 {csv_file} 为空或不存在")
        return
    
    with open(report_file, 'w', encoding='utf-8') as f:
        f.write("# PARD 多核性能测试报告\n\n")
        f.write(f"**生成时间**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write("## 测试配置\n\n")
        f.write("- **测试矩阵**: 1000×1000\n")
        f.write("- **测试核心数**: 1, 2, 4, 8\n")
        f.write("- **矩阵类型**: 非对称矩阵 (type=11)\n\n")
        f.write("---\n\n")
        
        # 为每个矩阵大小生成报告
        for size in [1000]:
            f.write(f"## {size}×{size} 矩阵性能结果\n\n")
            
            # 性能数据表
            f.write("### 执行时间 (秒)\n\n")
            f.write("| 核心数 | 分析时间 | 分解时间 | 求解时间 | **总时间** |\n")
            f.write("|--------|----------|----------|----------|-----------|\n")
            
            has_data = False
            for cores in [1, 2, 4, 8]:
                pard_key = (size, cores, 'PARD')
                
                if pard_key in data:
                    pard_data = data[pard_key]
                    pard_a = pard_data.get('analysis', 0.0)
                    pard_f = pard_data.get('factor', 0.0)
                    pard_s = pard_data.get('solve', 0.0)
                    pard_t = pard_data.get('total', 0.0)
                    
                    if pard_t > 0:
                        has_data = True
                        f.write(f"| {cores} | {pard_a:.6f} | {pard_f:.6f} | {pard_s:.6f} | **{pard_t:.6f}** |\n")
            
            if not has_data:
                f.write("| - | 无数据 | 无数据 | 无数据 | 无数据 |\n")
            
            f.write("\n")
            
            # 加速比
            f.write("### PARD 加速比和效率 (相对于1核)\n\n")
            f.write("| 核心数 | 加速比 | 并行效率(%) |\n")
            f.write("|--------|--------|-------------|\n")
            
            pard_speedups = calculate_speedup(data, size, 'PARD')
            pard_efficiency = calculate_efficiency(pard_speedups)
            
            for cores in [1, 2, 4, 8]:
                pard_s = pard_speedups.get(cores, 0.0)
                pard_e = pard_efficiency.get(cores, 0.0)
                
                if cores == 1:
                    f.write(f"| {cores} | {pard_s:.2f}x | {pard_e:.1f}% |\n")
                elif pard_s > 0:
                    f.write(f"| {cores} | {pard_s:.2f}x | {pard_e:.1f}% |\n")
                else:
                    f.write(f"| {cores} | - | - |\n")
            
            f.write("\n---\n\n")
        
        # 总结
        f.write("## 性能总结\n\n")
        
        f.write("### PARD 性能分析\n\n")
        f.write("| 矩阵大小 | 最佳核心数 | 最佳时间(s) | 加速比 | 效率(%) |\n")
        f.write("|----------|------------|-------------|--------|--------|\n")
        
        for size in [1000]:
            best_cores = 1
            best_time = float('inf')
            best_speedup = 1.0
            
            # 找到最佳时间
            for cores in [1, 2, 4, 8]:
                key = (size, cores, 'PARD')
                if key in data:
                    total_time = data[key]['total']
                    if total_time > 0 and total_time < best_time:
                        best_time = total_time
                        best_cores = cores
            
            # 计算加速比
            base_key = (size, 1, 'PARD')
            if base_key in data and data[base_key]['total'] > 0 and best_time > 0:
                best_speedup = data[base_key]['total'] / best_time
            
            efficiency = (best_speedup / best_cores * 100.0) if best_cores > 0 else 0.0
            
            if best_time < float('inf'):
                f.write(f"| {size}×{size} | {best_cores} | {best_time:.6f} | {best_speedup:.2f}x | {efficiency:.1f}% |\n")
        
        f.write("\n")
        
        f.write("### 主要发现\n\n")
        f.write("1. **性能表现**: 不同核心数配置下的性能对比\n")
        f.write("2. **并行效率**: 评估并行计算的有效性\n")
        f.write("3. **最佳配置**: 根据测试结果选择最佳核心数\n")
        
        f.write("\n---\n\n")
        f.write("**报告生成完成**\n")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        csv_file = os.path.join(script_dir, "perf_results", "pard_1000x1000_multi_core.csv")
        report_file = os.path.join(script_dir, "perf_results", "PARD_MULTI_CORE_REPORT.md")
    else:
        csv_file = sys.argv[1]
        report_file = sys.argv[2] if len(sys.argv) > 2 else csv_file.replace('.csv', '_REPORT.md')
    
    if not os.path.exists(csv_file):
        print(f"错误: CSV文件不存在: {csv_file}")
        sys.exit(1)
    
    generate_report(csv_file, report_file)
    print(f"报告已生成: {report_file}")
