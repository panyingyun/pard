#!/usr/bin/env python3
"""
创建性能测试用的稀疏矩阵（Matrix Market格式）
生成不同大小的矩阵：10x10, 1000x1000, 100000x100000
"""
import numpy as np
import scipy.sparse as sp
import sys
import os

def create_sparse_matrix(n, sparsity=0.05, filename=None):
    """
    创建稀疏测试矩阵（非对称）
    Args:
        n: 矩阵维度
        sparsity: 稀疏度（非零元素比例）
    """
    if filename is None:
        filename = f"test_matrices/perf_matrix_{n}x{n}.mtx"
    
    # 创建目录
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    
    # 计算目标非零元素数
    target_nnz = int(n * n * sparsity)
    
    # 确保至少每行有一个非零元素（对角线）
    if target_nnz < n:
        target_nnz = n
    
    # 创建稀疏矩阵：三对角结构 + 随机非零元素
    # 1. 对角线元素（确保矩阵非奇异）
    data = []
    rows = []
    cols = []
    
    for i in range(n):
        rows.append(i)
        cols.append(i)
        data.append(float(n + 1))  # 对角占优
    
    # 2. 上下对角线
    for i in range(n - 1):
        rows.append(i)
        cols.append(i + 1)
        data.append(-1.0)
        rows.append(i + 1)
        cols.append(i)
        data.append(-0.5)
    
    # 3. 随机添加更多非零元素直到达到目标nnz
    current_nnz = len(data)
    remaining_nnz = target_nnz - current_nnz
    
    if remaining_nnz > 0:
        np.random.seed(42)  # 可重复性
        added = 0
        max_attempts = remaining_nnz * 10
        
        for _ in range(max_attempts):
            if added >= remaining_nnz:
                break
            i = np.random.randint(0, n)
            j = np.random.randint(0, n)
            # 检查是否已存在
            exists = False
            for k in range(len(rows)):
                if rows[k] == i and cols[k] == j:
                    exists = True
                    break
            if not exists:
                rows.append(i)
                cols.append(j)
                data.append(np.random.uniform(-1.0, 1.0))
                added += 1
    
    # 创建稀疏矩阵
    A = sp.coo_matrix((data, (rows, cols)), shape=(n, n))
    A = A.tocsr()
    
    # 保存为Matrix Market格式
    sp.io.mmwrite(filename, A, field='real')
    
    print(f"Created matrix: {filename} ({n}x{n}, {A.nnz} non-zeros, "
          f"sparsity: {A.nnz/(n*n)*100:.4f}%)")
    
    return filename

if __name__ == "__main__":
    # 矩阵大小列表
    sizes = [10, 1000, 100000]
    
    # 为每个大小设置合适的稀疏度
    sparsities = {
        10: 0.3,        # 10x10: 30%稀疏度
        1000: 0.01,     # 1000x1000: 1%稀疏度
        100000: 0.001   # 100000x100000: 0.1%稀疏度（非常稀疏）
    }
    
    print("Generating performance test matrices...")
    print("=" * 60)
    
    for n in sizes:
        sparsity = sparsities.get(n, 0.01)
        try:
            create_sparse_matrix(n, sparsity=sparsity)
        except Exception as e:
            print(f"Error creating {n}x{n} matrix: {e}")
            # 对于100000x100000，可能需要特殊处理
            if n == 100000:
                print(f"Warning: {n}x{n} matrix may be too large. Creating with minimal pattern.")
                # 只创建三对角结构
                filename = f"test_matrices/perf_matrix_{n}x{n}.mtx"
                os.makedirs("test_matrices", exist_ok=True)
                # 使用更简单的模式
                create_sparse_matrix(n, sparsity=0.0001)
    
    print("=" * 60)
    print("Matrix generation completed!")
