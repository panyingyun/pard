#!/usr/bin/env python3
"""
创建测试矩阵（Matrix Market格式）
"""
import numpy as np
import scipy.sparse as sp

def create_test_matrix_symmetric(n, filename):
    """创建对称测试矩阵"""
    # 创建一个简单的对称正定矩阵（对角占优）
    A = sp.eye(n) * (n + 1)
    A += sp.diags([-1] * (n-1), 1)
    A += sp.diags([-1] * (n-1), -1)
    
    # 转换为对称格式
    A = (A + A.T) / 2
    
    # 保存为Matrix Market格式
    sp.io.mmwrite(filename, A, field='real', symmetry='symmetric')
    print(f"Created symmetric matrix: {filename} ({n}x{n}, {A.nnz} non-zeros)")

def create_test_matrix_nonsymmetric(n, filename):
    """创建非对称测试矩阵"""
    # 创建一个简单的非对称矩阵
    A = sp.eye(n) * (n + 1)
    A += sp.diags([-1] * (n-1), 1)
    A += sp.diags([-0.5] * (n-1), -1)
    
    # 保存为Matrix Market格式
    sp.io.mmwrite(filename, A, field='real')
    print(f"Created non-symmetric matrix: {filename} ({n}x{n}, {A.nnz} non-zeros)")

if __name__ == "__main__":
    import os
    import sys
    
    # 创建测试矩阵目录
    os.makedirs("test_matrices", exist_ok=True)
    
    # 创建不同规模的测试矩阵
    sizes = [10, 50, 100, 200, 500]
    
    for n in sizes:
        create_test_matrix_symmetric(n, f"test_matrices/test_sym_{n}.mtx")
        create_test_matrix_nonsymmetric(n, f"test_matrices/test_nonsym_{n}.mtx")
    
    print("\nTest matrices created successfully!")
