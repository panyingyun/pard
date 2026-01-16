#!/usr/bin/env python3
"""
PARD vs MUMPS 性能对比测试
测试不同大小的矩阵在不同核心数下的性能
"""
import subprocess
import os
import sys
import csv
import time
import re

def generate_matrix(m, n, sparsity, output_file, is_rectangular=False):
    """生成测试矩阵"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))
    
    if is_rectangular:
        create_bin = os.path.join(project_root, "build/create_rectangular_matrix")
        if not os.path.exists(create_bin):
            create_bin = os.path.join(project_root, "build/bin/create_rectangular_matrix")
        cmd = [create_bin, str(m), str(n), str(sparsity), output_file]
    else:
        create_bin = os.path.join(project_root, "build/create_matrix")
        if not os.path.exists(create_bin):
            create_bin = os.path.join(project_root, "build/bin/create_matrix")
        cmd = [create_bin, str(n), str(sparsity), output_file]
    
    if not os.path.exists(create_bin):
        print(f"Error: Matrix generator not found at {create_bin}")
        return False
    
    try:
        result = subprocess.run(cmd, check=True, capture_output=True, text=True, cwd=project_root)
        if result.stdout:
            print(result.stdout.strip())
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error generating matrix: {e.stderr if e.stderr else e}")
        return False

def run_pard_benchmark(matrix_file, num_cores, matrix_type=11):
    """运行PARD benchmark"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))
    benchmark_bin = os.path.join(project_root, "build/bin/benchmark")
    
    cmd = ["mpirun", "-np", str(num_cores), benchmark_bin, matrix_file, str(matrix_type), "1"]
    
    try:
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, 
                                        timeout=3600, universal_newlines=True, cwd=project_root)
        
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
        return None
    except subprocess.CalledProcessError:
        return None
    except Exception:
        return None
    
    return None

def run_mumps_benchmark(matrix_file, num_cores):
    """运行MUMPS benchmark（如果可用）"""
    # MUMPS通常需要特定的接口，这里先返回None表示未实现
    # 实际使用时需要根据MUMPS的安装和接口进行调整
    return None

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))
    os.chdir(project_root)
    
    # 矩阵配置: (m, n, sparsity, description)
    matrix_configs = [
        (1000, 10000, 0.01, "1000x10000"),   # 矩形矩阵
        (10000, 10000, 0.01, "10000x10000"), # 方阵
    ]
    
    num_cores_list = [1, 2, 4]
    
    # 创建目录
    results_dir = "tests/benchmark/perf_results"
    matrix_dir = "tests/benchmark/test_matrices"
    os.makedirs(results_dir, exist_ok=True)
    os.makedirs(matrix_dir, exist_ok=True)
    
    results_file = os.path.join(results_dir, "pard_mumps_comparison.csv")
    
    # 生成测试矩阵
    print("=" * 70)
    print("Generating test matrices...")
    print("=" * 70)
    
    for m, n, sparsity, desc in matrix_configs:
        if m == n:
            matrix_file = f"{matrix_dir}/perf_matrix_{m}x{n}.mtx"
            if not os.path.exists(matrix_file):
                print(f"Generating {desc} matrix (sparsity: {sparsity})... ", end='', flush=True)
                if generate_matrix(m, n, sparsity, matrix_file, is_rectangular=False):
                    print("Done")
                else:
                    print("Failed")
            else:
                print(f"Matrix {desc} already exists, skipping generation")
        else:
            matrix_file = f"{matrix_dir}/perf_matrix_{m}x{n}.mtx"
            if not os.path.exists(matrix_file):
                print(f"Generating {desc} matrix (sparsity: {sparsity})... ", end='', flush=True)
                if generate_matrix(m, n, sparsity, matrix_file, is_rectangular=True):
                    print("Done")
                else:
                    print("Failed")
            else:
                print(f"Matrix {desc} already exists, skipping generation")
    
    print("\n" + "=" * 70)
    print("Running Performance Tests")
    print("=" * 70)
    
    # 写入CSV头
    with open(results_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Matrix_Size', 'Num_Cores', 'Solver', 'Analysis_Time', 
                        'Factor_Time', 'Solve_Time', 'Total_Time'])
    
    # 测试每个矩阵
    for m, n, sparsity, desc in matrix_configs:
        matrix_file = f"{matrix_dir}/perf_matrix_{m}x{n}.mtx"
        
        if not os.path.exists(matrix_file):
            print(f"\nWarning: Matrix file {matrix_file} not found, skipping {desc}")
            continue
        
        print(f"\nMatrix: {desc}")
        print("-" * 70)
        
        # 测试PARD
        for num_cores in num_cores_list:
            print(f"  PARD with {num_cores} core(s)... ", end='', flush=True)
            
            result = run_pard_benchmark(matrix_file, num_cores)
            
            if result:
                analysis_time, factor_time, solve_time, total_time = result
                print(f"Total: {total_time:.6f}s")
                
                with open(results_file, 'a', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerow([desc, num_cores, 'PARD', analysis_time, 
                                   factor_time, solve_time, total_time])
            else:
                print("FAILED")
            
            time.sleep(0.5)
        
        # 测试MUMPS（如果可用）
        # 这里先跳过，因为需要MUMPS的具体实现
        print(f"  MUMPS: Not implemented in this test")
    
    print("\n" + "=" * 70)
    print(f"Performance testing completed!")
    print(f"Results saved to: {results_file}")
    print("=" * 70)

if __name__ == "__main__":
    main()
