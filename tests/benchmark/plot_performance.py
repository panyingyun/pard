#!/usr/bin/env python3
"""
生成性能对比曲线图
需要安装: pip install pandas matplotlib
"""
import sys
import os

# 尝试导入必需的库
try:
    import pandas as pd
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError as e:
    print(f"Error: Required library not installed: {e}")
    print("Please install: pip install pandas matplotlib numpy")
    sys.exit(1)

# 设置中文字体
plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial']
plt.rcParams['axes.unicode_minus'] = False

def plot_performance_comparison(csv_file, output_dir):
    """
    生成性能对比图
    """
    # 读取数据
    df = pd.read_csv(csv_file)
    
    # 创建输出目录
    os.makedirs(output_dir, exist_ok=True)
    
    # 按矩阵大小分组
    matrix_sizes = sorted(df['Matrix_Size'].unique())
    
    # 创建多个图表
    fig = plt.figure(figsize=(16, 10))
    
    # 1. 总时间对比（不同核心数）
    ax1 = plt.subplot(2, 2, 1)
    for size in matrix_sizes:
        size_data = df[df['Matrix_Size'] == size]
        cores = size_data['Num_Cores'].values
        total_times = size_data['Total_Time'].values
        ax1.plot(cores, total_times, marker='o', linewidth=2, markersize=8, 
                label=f'{size}x{size}')
    ax1.set_xlabel('Number of Cores', fontsize=12)
    ax1.set_ylabel('Total Time (seconds)', fontsize=12)
    ax1.set_title('Total Time vs Number of Cores', fontsize=14, fontweight='bold')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    ax1.set_yscale('log')
    
    # 2. 加速比（相对于单核）
    ax2 = plt.subplot(2, 2, 2)
    for size in matrix_sizes:
        size_data = df[df['Matrix_Size'] == size]
        cores = size_data['Num_Cores'].values
        total_times = size_data['Total_Time'].values
        single_core_time = total_times[0]  # 第一个应该是单核
        speedup = single_core_time / total_times
        ax2.plot(cores, speedup, marker='s', linewidth=2, markersize=8,
                label=f'{size}x{size}')
    ax2.plot([1, 2, 4], [1, 2, 4], 'k--', alpha=0.5, label='Ideal Speedup')
    ax2.set_xlabel('Number of Cores', fontsize=12)
    ax2.set_ylabel('Speedup', fontsize=12)
    ax2.set_title('Speedup vs Number of Cores', fontsize=14, fontweight='bold')
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    
    # 3. 各阶段时间分解（柱状图）
    ax3 = plt.subplot(2, 2, 3)
    x = np.arange(len(matrix_sizes))
    width = 0.25
    
    analysis_times = []
    factor_times = []
    solve_times = []
    
    for size in matrix_sizes:
        size_data = df[df['Matrix_Size'] == size]
        # 使用4核的数据（如果存在）
        if 4 in size_data['Num_Cores'].values:
            data_4core = size_data[size_data['Num_Cores'] == 4].iloc[0]
        elif 2 in size_data['Num_Cores'].values:
            data_4core = size_data[size_data['Num_Cores'] == 2].iloc[0]
        else:
            data_4core = size_data.iloc[0]
        
        analysis_times.append(data_4core['Analysis_Time'])
        factor_times.append(data_4core['Factor_Time'])
        solve_times.append(data_4core['Solve_Time'])
    
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
    
    # 4. 矩阵大小vs时间（不同核心数）
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
    
    # 保存图片
    output_file = os.path.join(output_dir, 'performance_comparison.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Performance comparison plot saved to: {output_file}")
    
    # 创建详细的性能表格
    print("\n" + "=" * 80)
    print("Performance Summary")
    print("=" * 80)
    print(df.to_string(index=False))
    print("=" * 80)
    
    # 保存详细数据
    summary_file = os.path.join(output_dir, 'performance_summary.txt')
    with open(summary_file, 'w') as f:
        f.write("PARD Performance Test Results\n")
        f.write("=" * 80 + "\n\n")
        f.write(df.to_string(index=False))
        f.write("\n\n")
        
        # 计算加速比
        f.write("Speedup Analysis\n")
        f.write("-" * 80 + "\n")
        for size in matrix_sizes:
            size_data = df[df['Matrix_Size'] == size]
            if len(size_data) > 1:
                single_core = size_data[size_data['Num_Cores'] == 1]['Total_Time'].values[0]
                f.write(f"\nMatrix {size}x{size}:\n")
                for _, row in size_data.iterrows():
                    cores = row['Num_Cores']
                    if cores > 1:
                        speedup = single_core / row['Total_Time']
                        efficiency = speedup / cores * 100
                        f.write(f"  {cores} cores: Speedup = {speedup:.2f}x, "
                               f"Efficiency = {efficiency:.1f}%\n")
    
    print(f"\nDetailed summary saved to: {summary_file}")

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))
    
    csv_file = os.path.join(project_root, "tests/benchmark/perf_results/performance_results.csv")
    output_dir = os.path.join(project_root, "tests/benchmark/perf_results")
    
    if not os.path.exists(csv_file):
        print(f"Error: Results file not found: {csv_file}")
        print("Please run the performance test first.")
        sys.exit(1)
    
    plot_performance_comparison(csv_file, output_dir)
