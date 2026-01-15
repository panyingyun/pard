#!/usr/bin/env python3
"""
性能测试脚本
测试PARD在不同矩阵大小和核心数下的性能
"""
import subprocess
import os
import sys
import csv
import time
import re

def run_benchmark(matrix_file, num_cores, matrix_type=11):
    """
    运行PARD benchmark
    Returns: (analysis_time, factor_time, solve_time, total_time) or None if failed
    """
    cmd = ["mpirun", "-np", str(num_cores), 
           "./build/bin/benchmark", matrix_file, str(matrix_type), "1"]
    
    try:
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, 
                                        timeout=3600, universal_newlines=True)
        
        # 解析输出
        analysis_time = None
        factor_time = None
        solve_time = None
        
        for line in output.split('\n'):
            if 'Analysis time' in line:
                match = re.search(r'(\d+\.?\d*e?-?\d*)', line)
                if match:
                    analysis_time = float(match.group(1))
            elif 'Factorization time' in line:
                match = re.search(r'(\d+\.?\d*e?-?\d*)', line)
                if match:
                    factor_time = float(match.group(1))
            elif 'Solve time' in line:
                match = re.search(r'(\d+\.?\d*e?-?\d*)', line)
                if match:
                    solve_time = float(match.group(1))
        
        if analysis_time is not None and factor_time is not None and solve_time is not None:
            total_time = analysis_time + factor_time + solve_time
            return (analysis_time, factor_time, solve_time, total_time)
        
    except subprocess.TimeoutExpired:
        print(f"  Timeout after 1 hour")
        return None
    except subprocess.CalledProcessError as e:
        print(f"  Error: {e}")
        return None
    except Exception as e:
        print(f"  Unexpected error: {e}")
        return None
    
    return None

def main():
    # 确保在项目根目录
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))
    os.chdir(project_root)
    
    # 矩阵大小
    matrix_sizes = [10, 1000, 100000]
    
    # 核心数
    num_cores_list = [1, 2, 4]
    
    # 创建结果目录
    results_dir = "tests/benchmark/perf_results"
    os.makedirs(results_dir, exist_ok=True)
    
    results_file = os.path.join(results_dir, "performance_results.csv")
    
    # 生成测试矩阵
    print("=" * 70)
    print("Generating test matrices...")
    print("=" * 70)
    subprocess.run(["python3", "tests/benchmark/create_perf_matrices.py"], check=True)
    
    print("\n" + "=" * 70)
    print("Running Performance Tests")
    print("=" * 70)
    
    # 写入CSV头
    with open(results_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Matrix_Size', 'Num_Cores', 'Solver', 'Analysis_Time', 
                        'Factor_Time', 'Solve_Time', 'Total_Time'])
    
    # 测试每个矩阵大小
    for size in matrix_sizes:
        matrix_file = f"tests/benchmark/test_matrices/perf_matrix_{size}x{size}.mtx"
        
        if not os.path.exists(matrix_file):
            print(f"\nWarning: Matrix file {matrix_file} not found, skipping size {size}")
            continue
        
        print(f"\nMatrix Size: {size}x{size}")
        print("-" * 70)
        
        # 测试每个核心数
        for num_cores in num_cores_list:
            print(f"  Testing with {num_cores} core(s)... ", end='', flush=True)
            
            result = run_benchmark(matrix_file, num_cores)
            
            if result:
                analysis_time, factor_time, solve_time, total_time = result
                print(f"Total: {total_time:.6f}s "
                      f"(Analysis: {analysis_time:.6f}s, "
                      f"Factor: {factor_time:.6f}s, "
                      f"Solve: {solve_time:.6f}s)")
                
                # 写入结果
                with open(results_file, 'a', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerow([size, num_cores, 'PARD', analysis_time, 
                                   factor_time, solve_time, total_time])
            else:
                print("FAILED")
            
            # 短暂休息
            time.sleep(0.5)
    
    print("\n" + "=" * 70)
    print(f"Performance testing completed!")
    print(f"Results saved to: {results_file}")
    print("=" * 70)

if __name__ == "__main__":
    main()
