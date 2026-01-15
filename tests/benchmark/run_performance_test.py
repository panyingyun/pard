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

def generate_matrix(n, sparsity, output_file):
    """使用C程序生成矩阵"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))
    # create_matrix可能在build/或build/bin/下
    create_matrix_bin = os.path.join(project_root, "build/create_matrix")
    if not os.path.exists(create_matrix_bin):
        create_matrix_bin = os.path.join(project_root, "build/bin/create_matrix")
    
    if not os.path.exists(create_matrix_bin):
        print(f"Error: create_matrix binary not found. Please build the project first.")
        return False
    
    cmd = [create_matrix_bin, str(n), str(sparsity), output_file]
    try:
        result = subprocess.run(cmd, check=True, capture_output=True, text=True, cwd=project_root)
        if result.stdout:
            print(result.stdout.strip())
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error generating matrix {n}x{n}: {e.stderr if e.stderr else e}")
        return False
    except FileNotFoundError:
        print(f"Error: create_matrix binary not found")
        return False

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
    
    # 矩阵大小和对应的稀疏度
    matrix_configs = [
        (10, 0.3),      # 10x10: 30%稀疏度
        (1000, 0.01),   # 1000x1000: 1%稀疏度
        (100000, 0.001) # 100000x100000: 0.1%稀疏度（非常稀疏）
    ]
    
    # 核心数
    num_cores_list = [1, 2, 4]
    
    # 创建结果目录和矩阵目录
    results_dir = "tests/benchmark/perf_results"
    matrix_dir = "tests/benchmark/test_matrices"
    os.makedirs(results_dir, exist_ok=True)
    os.makedirs(matrix_dir, exist_ok=True)
    
    results_file = os.path.join(results_dir, "performance_results.csv")
    
    # 生成测试矩阵
    print("=" * 70)
    print("Generating test matrices...")
    print("=" * 70)
    
    for n, sparsity in matrix_configs:
        matrix_file = f"{matrix_dir}/perf_matrix_{n}x{n}.mtx"
        if not os.path.exists(matrix_file):
            print(f"Generating {n}x{n} matrix (sparsity: {sparsity})... ", end='', flush=True)
            if generate_matrix(n, sparsity, matrix_file):
                print("Done")
            else:
                print("Failed")
        else:
            print(f"Matrix {n}x{n} already exists, skipping generation")
    
    print("\n" + "=" * 70)
    print("Running Performance Tests")
    print("=" * 70)
    
    # 写入CSV头
    with open(results_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Matrix_Size', 'Num_Cores', 'Solver', 'Analysis_Time', 
                        'Factor_Time', 'Solve_Time', 'Total_Time'])
    
    # 测试每个矩阵大小
    for n, sparsity in matrix_configs:
        matrix_file = f"{matrix_dir}/perf_matrix_{n}x{n}.mtx"
        
        if not os.path.exists(matrix_file):
            print(f"\nWarning: Matrix file {matrix_file} not found, skipping size {n}")
            continue
        
        print(f"\nMatrix Size: {n}x{n}")
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
                    writer.writerow([n, num_cores, 'PARD', analysis_time, 
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
