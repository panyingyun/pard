#!/usr/bin/env python3
"""
生成PARD vs MUMPS性能对比曲线图
"""
import sys
import os
import csv

# 尝试导入matplotlib
try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError as e:
    print(f"Error: matplotlib not installed: {e}")
    print("Please install: sudo apt install python3-matplotlib python3-numpy")
    sys.exit(1)

plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial']
plt.rcParams['axes.unicode_minus'] = False

def read_csv_data(csv_file):
    """读取CSV数据"""
    data = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append({
                'size': row['Matrix_Size'],
                'cores': int(row['Num_Cores']),
                'solver': row['Solver'],
                'analysis': float(row['Analysis_Time']),
                'factor': float(row['Factor_Time']),
                'solve': float(row['Solve_Time']),
                'total': float(row['Total_Time'])
            })
    return data

def plot_comparison(csv_file, output_dir):
    """生成对比图"""
    data = read_csv_data(csv_file)
    
    os.makedirs(output_dir, exist_ok=True)
    
    # 按矩阵大小分组
    sizes = sorted(set(d['size'] for d in data))
    solvers = sorted(set(d['solver'] for d in data))
    
    # 创建图表
    fig = plt.figure(figsize=(18, 12))
    
    # 1. 总时间对比（不同核心数，按求解器分组）
    ax1 = plt.subplot(2, 3, 1)
    for solver in solvers:
        for size in sizes:
            size_data = [d for d in data if d['size'] == size and d['solver'] == solver]
            size_data.sort(key=lambda x: x['cores'])
            if size_data:
                cores = [d['cores'] for d in size_data]
                times = [d['total'] for d in size_data]
                ax1.plot(cores, times, marker='o', linewidth=2, markersize=8, 
                        label=f'{solver} {size}')
    ax1.set_xlabel('Number of Cores', fontsize=11)
    ax1.set_ylabel('Total Time (seconds)', fontsize=11)
    ax1.set_title('Total Time vs Number of Cores', fontsize=13, fontweight='bold')
    ax1.legend(fontsize=9)
    ax1.grid(True, alpha=0.3)
    ax1.set_yscale('log')
    
    # 2. 加速比对比
    ax2 = plt.subplot(2, 3, 2)
    for solver in solvers:
        for size in sizes:
            size_data = [d for d in data if d['size'] == size and d['solver'] == solver]
            size_data.sort(key=lambda x: x['cores'])
            if size_data:
                cores = [d['cores'] for d in size_data]
                times = [d['total'] for d in size_data]
                single_time = times[0]
                speedup = [single_time / t for t in times]
                ax2.plot(cores, speedup, marker='s', linewidth=2, markersize=8,
                        label=f'{solver} {size}')
    ax2.plot([1, 2, 4], [1, 2, 4], 'k--', alpha=0.5, label='Ideal')
    ax2.set_xlabel('Number of Cores', fontsize=11)
    ax2.set_ylabel('Speedup', fontsize=11)
    ax2.set_title('Speedup vs Number of Cores', fontsize=13, fontweight='bold')
    ax2.legend(fontsize=9)
    ax2.grid(True, alpha=0.3)
    
    # 3. PARD vs MUMPS 直接对比（按矩阵大小）
    ax3 = plt.subplot(2, 3, 3)
    x = np.arange(len(sizes))
    width = 0.35
    
    for i, solver in enumerate(solvers):
        times_1core = []
        for size in sizes:
            size_data = [d for d in data if d['size'] == size and d['solver'] == solver and d['cores'] == 1]
            if size_data:
                times_1core.append(size_data[0]['total'])
            else:
                times_1core.append(0)
        offset = (i - len(solvers)/2 + 0.5) * width
        ax3.bar(x + offset, times_1core, width, label=f'{solver} (1 core)', alpha=0.8)
    
    ax3.set_xlabel('Matrix Size', fontsize=11)
    ax3.set_ylabel('Total Time (seconds)', fontsize=11)
    ax3.set_title('PARD vs MUMPS (1 core)', fontsize=13, fontweight='bold')
    ax3.set_xticks(x)
    ax3.set_xticklabels(sizes)
    ax3.legend()
    ax3.grid(True, alpha=0.3, axis='y')
    ax3.set_yscale('log')
    
    # 4. 各阶段时间分解（PARD）
    ax4 = plt.subplot(2, 3, 4)
    pard_data = [d for d in data if d['solver'] == 'PARD']
    if pard_data:
        sizes_pard = sorted(set(d['size'] for d in pard_data))
        x = np.arange(len(sizes_pard))
        width = 0.25
        
        analysis_times = []
        factor_times = []
        solve_times = []
        
        for size in sizes_pard:
            size_data = [d for d in pard_data if d['size'] == size]
            data_4core = next((d for d in size_data if d['cores'] == 4), None)
            if not data_4core:
                data_4core = next((d for d in size_data if d['cores'] == 2), None)
            if not data_4core:
                data_4core = size_data[0]
            
            analysis_times.append(data_4core['analysis'])
            factor_times.append(data_4core['factor'])
            solve_times.append(data_4core['solve'])
        
        ax4.bar(x - width, analysis_times, width, label='Analysis', alpha=0.8)
        ax4.bar(x, factor_times, width, label='Factorization', alpha=0.8)
        ax4.bar(x + width, solve_times, width, label='Solve', alpha=0.8)
        
        ax4.set_xlabel('Matrix Size', fontsize=11)
        ax4.set_ylabel('Time (seconds)', fontsize=11)
        ax4.set_title('PARD Time Breakdown', fontsize=13, fontweight='bold')
        ax4.set_xticks(x)
        ax4.set_xticklabels(sizes_pard)
        ax4.legend()
        ax4.grid(True, alpha=0.3, axis='y')
        ax4.set_yscale('log')
    
    # 5. 各阶段时间分解（MUMPS）
    ax5 = plt.subplot(2, 3, 5)
    mumps_data = [d for d in data if d['solver'] == 'MUMPS']
    if mumps_data:
        sizes_mumps = sorted(set(d['size'] for d in mumps_data))
        x = np.arange(len(sizes_mumps))
        width = 0.25
        
        analysis_times = []
        factor_times = []
        solve_times = []
        
        for size in sizes_mumps:
            size_data = [d for d in mumps_data if d['size'] == size]
            data_4core = next((d for d in size_data if d['cores'] == 4), None)
            if not data_4core:
                data_4core = next((d for d in size_data if d['cores'] == 2), None)
            if not data_4core:
                data_4core = size_data[0]
            
            analysis_times.append(data_4core['analysis'])
            factor_times.append(data_4core['factor'])
            solve_times.append(data_4core['solve'])
        
        ax5.bar(x - width, analysis_times, width, label='Analysis', alpha=0.8)
        ax5.bar(x, factor_times, width, label='Factorization', alpha=0.8)
        ax5.bar(x + width, solve_times, width, label='Solve', alpha=0.8)
        
        ax5.set_xlabel('Matrix Size', fontsize=11)
        ax5.set_ylabel('Time (seconds)', fontsize=11)
        ax5.set_title('MUMPS Time Breakdown', fontsize=13, fontweight='bold')
        ax5.set_xticks(x)
        ax5.set_xticklabels(sizes_mumps)
        ax5.legend()
        ax5.grid(True, alpha=0.3, axis='y')
        ax5.set_yscale('log')
    
    # 6. 性能比（MUMPS/PARD）
    ax6 = plt.subplot(2, 3, 6)
    cores_list = sorted(set(d['cores'] for d in data))
    for cores in cores_list:
        ratios = []
        labels = []
        for size in sizes:
            pard_data = [d for d in data if d['size'] == size and d['solver'] == 'PARD' and d['cores'] == cores]
            mumps_data = [d for d in data if d['size'] == size and d['solver'] == 'MUMPS' and d['cores'] == cores]
            if pard_data and mumps_data:
                ratio = mumps_data[0]['total'] / pard_data[0]['total']
                ratios.append(ratio)
                labels.append(size)
        
        if ratios:
            x_pos = np.arange(len(ratios))
            ax6.bar(x_pos + (cores-1)*0.25, ratios, 0.25, label=f'{cores} core(s)', alpha=0.8)
    
    ax6.axhline(y=1.0, color='r', linestyle='--', alpha=0.5, label='Equal Performance')
    ax6.set_xlabel('Matrix Size', fontsize=11)
    ax6.set_ylabel('Time Ratio (MUMPS/PARD)', fontsize=11)
    ax6.set_title('Performance Ratio: MUMPS/PARD', fontsize=13, fontweight='bold')
    ax6.set_xticks(np.arange(len(labels)))
    ax6.set_xticklabels(labels)
    ax6.legend()
    ax6.grid(True, alpha=0.3, axis='y')
    
    plt.tight_layout()
    
    # 保存图片
    output_file = os.path.join(output_dir, 'pard_mumps_comparison.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Comparison plot saved to: {output_file}")
    
    # 打印摘要
    print("\n" + "=" * 80)
    print("Performance Comparison Summary")
    print("=" * 80)
    print(f"{'Size':<15} {'Cores':<8} {'Solver':<10} {'Total Time':<15}")
    print("-" * 80)
    for d in sorted(data, key=lambda x: (x['size'], x['solver'], x['cores'])):
        print(f"{d['size']:<15} {d['cores']:<8} {d['solver']:<10} {d['total']:<15.6f}")
    print("=" * 80)

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))
    
    csv_file = os.path.join(project_root, "tests/benchmark/perf_results/pard_mumps_comparison.csv")
    output_dir = os.path.join(project_root, "tests/benchmark/perf_results")
    
    if not os.path.exists(csv_file):
        print(f"Error: Results file not found: {csv_file}")
        print("Please run the comparison test first.")
        sys.exit(1)
    
    plot_comparison(csv_file, output_dir)
