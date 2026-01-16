#!/usr/bin/env python3
"""
生成性能测试报告和图表
如果matplotlib可用则生成图表，否则生成文本报告
"""
import csv
import os
import sys

def read_results(csv_file):
    """读取性能测试结果"""
    results = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            results.append({
                'size': int(row['Matrix_Size']),
                'cores': int(row['Num_Cores']),
                'solver': row['Solver'],
                'analysis': float(row['Analysis_Time']),
                'factor': float(row['Factor_Time']),
                'solve': float(row['Solve_Time']),
                'total': float(row['Total_Time'])
            })
    return results

def generate_text_report(results, output_file):
    """生成文本格式的性能报告"""
    with open(output_file, 'w') as f:
        f.write("=" * 80 + "\n")
        f.write("PARD Performance Test Report\n")
        f.write("=" * 80 + "\n\n")
        
        # 按矩阵大小分组
        sizes = sorted(set(r['size'] for r in results))
        
        for size in sizes:
            size_results = [r for r in results if r['size'] == size]
            f.write(f"\nMatrix Size: {size}x{size}\n")
            f.write("-" * 80 + "\n")
            f.write(f"{'Cores':<8} {'Analysis':<12} {'Factor':<12} {'Solve':<12} {'Total':<12} {'Speedup':<10}\n")
            f.write("-" * 80 + "\n")
            
            single_core_time = None
            for r in sorted(size_results, key=lambda x: x['cores']):
                if r['cores'] == 1:
                    single_core_time = r['total']
                    speedup = 1.0
                else:
                    speedup = single_core_time / r['total'] if single_core_time else 0
                
                f.write(f"{r['cores']:<8} {r['analysis']:<12.6f} {r['factor']:<12.6f} "
                       f"{r['solve']:<12.6f} {r['total']:<12.6f} {speedup:<10.2f}x\n")
        
        # 加速比分析
        f.write("\n" + "=" * 80 + "\n")
        f.write("Speedup Analysis\n")
        f.write("=" * 80 + "\n")
        
        for size in sizes:
            size_results = [r for r in results if r['size'] == size]
            single_core = next((r for r in size_results if r['cores'] == 1), None)
            if not single_core:
                continue
            
            f.write(f"\nMatrix {size}x{size}:\n")
            f.write(f"  Single core time: {single_core['total']:.6f}s\n")
            for r in sorted(size_results, key=lambda x: x['cores']):
                if r['cores'] > 1:
                    speedup = single_core['total'] / r['total']
                    efficiency = (speedup / r['cores']) * 100
                    f.write(f"  {r['cores']} cores: {r['total']:.6f}s, "
                           f"Speedup: {speedup:.2f}x, Efficiency: {efficiency:.1f}%\n")
    
    print(f"Text report saved to: {output_file}")

def try_generate_plot(csv_file, output_dir):
    """尝试生成图表（如果matplotlib可用）"""
    try:
        import pandas as pd
        import matplotlib
        matplotlib.use('Agg')  # 非交互式后端
        import matplotlib.pyplot as plt
        import numpy as np
        
        # 读取数据
        df = pd.read_csv(csv_file)
        
        # 创建图表
        fig = plt.figure(figsize=(16, 10))
        
        matrix_sizes = sorted(df['Matrix_Size'].unique())
        
        # 1. 总时间对比
        ax1 = plt.subplot(2, 2, 1)
        for size in matrix_sizes:
            size_data = df[df['Matrix_Size'] == size]
            cores = size_data['Num_Cores'].values
            times = size_data['Total_Time'].values
            ax1.plot(cores, times, marker='o', linewidth=2, markersize=8, 
                    label=f'{size}x{size}')
        ax1.set_xlabel('Number of Cores', fontsize=12)
        ax1.set_ylabel('Total Time (seconds)', fontsize=12)
        ax1.set_title('Total Time vs Number of Cores', fontsize=14, fontweight='bold')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        ax1.set_yscale('log')
        
        # 2. 加速比
        ax2 = plt.subplot(2, 2, 2)
        for size in matrix_sizes:
            size_data = df[df['Matrix_Size'] == size]
            cores = size_data['Num_Cores'].values
            times = size_data['Total_Time'].values
            single_time = times[0]
            speedup = single_time / times
            ax2.plot(cores, speedup, marker='s', linewidth=2, markersize=8,
                    label=f'{size}x{size}')
        ax2.plot([1, 2, 4], [1, 2, 4], 'k--', alpha=0.5, label='Ideal')
        ax2.set_xlabel('Number of Cores', fontsize=12)
        ax2.set_ylabel('Speedup', fontsize=12)
        ax2.set_title('Speedup vs Number of Cores', fontsize=14, fontweight='bold')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
        # 3. 时间分解
        ax3 = plt.subplot(2, 2, 3)
        x = np.arange(len(matrix_sizes))
        width = 0.25
        
        analysis_times = []
        factor_times = []
        solve_times = []
        
        for size in matrix_sizes:
            size_data = df[df['Matrix_Size'] == size]
            # 使用4核或最大可用核心数
            max_cores = size_data['Num_Cores'].max()
            data = size_data[size_data['Num_Cores'] == max_cores].iloc[0]
            analysis_times.append(data['Analysis_Time'])
            factor_times.append(data['Factor_Time'])
            solve_times.append(data['Solve_Time'])
        
        ax3.bar(x - width, analysis_times, width, label='Analysis', alpha=0.8)
        ax3.bar(x, factor_times, width, label='Factorization', alpha=0.8)
        ax3.bar(x + width, solve_times, width, label='Solve', alpha=0.8)
        
        ax3.set_xlabel('Matrix Size', fontsize=12)
        ax3.set_ylabel('Time (seconds)', fontsize=12)
        ax3.set_title('Time Breakdown by Phase', fontsize=14, fontweight='bold')
        ax3.set_xticks(x)
        ax3.set_xticklabels([f'{s}x{s}' for s in matrix_sizes])
        ax3.legend()
        ax3.grid(True, alpha=0.3, axis='y')
        ax3.set_yscale('log')
        
        # 4. 矩阵大小vs时间
        ax4 = plt.subplot(2, 2, 4)
        for cores in sorted(df['Num_Cores'].unique()):
            core_data = df[df['Num_Cores'] == cores]
            sizes = core_data['Matrix_Size'].values
            times = core_data['Total_Time'].values
            ax4.plot(sizes, times, marker='^', linewidth=2, markersize=8,
                    label=f'{cores} core(s)')
        ax4.set_xlabel('Matrix Size', fontsize=12)
        ax4.set_ylabel('Total Time (seconds)', fontsize=12)
        ax4.set_title('Time vs Matrix Size', fontsize=14, fontweight='bold')
        ax4.legend()
        ax4.grid(True, alpha=0.3)
        ax4.set_xscale('log')
        ax4.set_yscale('log')
        
        plt.tight_layout()
        
        output_file = os.path.join(output_dir, 'performance_comparison.png')
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        print(f"Performance plot saved to: {output_file}")
        return True
        
    except ImportError:
        return False

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))
    os.chdir(project_root)
    
    csv_file = "tests/benchmark/perf_results/performance_results.csv"
    output_dir = "tests/benchmark/perf_results"
    
    if not os.path.exists(csv_file):
        print(f"Error: Results file not found: {csv_file}")
        print("Please run the performance test first.")
        sys.exit(1)
    
    # 读取结果
    results = read_results(csv_file)
    
    # 生成文本报告
    report_file = os.path.join(output_dir, 'performance_report.txt')
    generate_text_report(results, report_file)
    
    # 尝试生成图表
    if not try_generate_plot(csv_file, output_dir):
        print("\nNote: matplotlib/pandas not available. Install with:")
        print("  pip install pandas matplotlib numpy")
        print("  or")
        print("  sudo apt install python3-pip && pip3 install pandas matplotlib numpy")
    
    print("\n" + "=" * 80)
    print("Performance report generation completed!")
    print("=" * 80)

if __name__ == "__main__":
    main()
