# PARD - 高性能稀疏矩阵求解器库

PARD是一个类似Panua-Pardiso的高性能稀疏矩阵直接求解器库，采用C语言编写，支持MPI并行计算。

## 特性

- **矩阵类型支持**：
  - 非对称实数矩阵（LU分解）
  - 对称不定实数矩阵（LDL^T分解，Bunch-Kaufman pivoting）
  - 对称正定实数矩阵（Cholesky分解）

- **核心功能**：
  - 符号分解和重排序（Minimum Degree、Nested Dissection）
  - 数值分解（LU、LDL^T、Cholesky）
  - 前向/后向替换求解
  - 迭代精化

- **并行支持**：
  - MPI分布式内存并行
  - 支持多进程并行分解和求解

- **存储格式**：
  - CSR（Compressed Sparse Row）格式

- **测试和基准**：
  - 支持SuiteSparse Matrix Collection标准测试矩阵集
  - 性能对比工具（与MUMPS对比）

## 构建

### 使用CMake

```bash
mkdir build
cd build
cmake ..
make
```

### 使用Makefile

```bash
make
```

## 依赖

- C编译器（支持C11标准）
- MPI实现（如OpenMPI、MPICH）
- 数学库（libm）

## 使用示例

```c
#include "pard.h"
#include <mpi.h>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    
    // 读取矩阵
    pard_csr_matrix_t *matrix = NULL;
    pard_matrix_read_mtx(&matrix, "matrix.mtx");
    
    // 初始化求解器
    pard_solver_t *solver = NULL;
    pardiso_init(&solver, PARD_MATRIX_TYPE_REAL_NONSYMMETRIC, MPI_COMM_WORLD);
    
    // 符号分解
    pardiso_symbolic(solver, matrix);
    
    // 数值分解
    pardiso_factor(solver);
    
    // 求解
    int n = matrix->n;
    double *rhs = (double *)malloc(n * sizeof(double));
    double *sol = (double *)malloc(n * sizeof(double));
    // ... 设置rhs ...
    pardiso_solve(solver, 1, rhs, sol);
    
    // 清理
    pardiso_cleanup(&solver);
    MPI_Finalize();
    return 0;
}
```

## 运行示例

```bash
# 串行运行
./build/bin/example matrix.mtx

# MPI并行运行
mpirun -np 4 ./build/bin/example matrix.mtx
```

## 测试

### 单元测试

```bash
./build/bin/test_unit
```

### 集成测试

```bash
mpirun -np 2 ./build/bin/test_integration
```

### 基准测试

```bash
# 下载测试矩阵
cd tests/benchmark
./download_matrices.sh

# 运行基准测试
mpirun -np 4 ./build/bin/benchmark test_matrices/bcsstk01.mtx 0 1
```

### 与MUMPS性能对比

```bash
mpirun -np 4 ./build/bin/mumps_compare test_matrices/bcsstk01.mtx
```

## API文档

主要API函数：

- `pardiso_init()`: 初始化求解器
- `pardiso_symbolic()`: 符号分解
- `pardiso_factor()`: 数值分解
- `pardiso_solve()`: 求解线性系统
- `pardiso_refine()`: 迭代精化
- `pardiso_cleanup()`: 清理资源

详细API文档请参考 `include/pard.h`。

## 性能目标

- 与MUMPS进行性能对比
- 并行效率目标：≥50-70%
- 支持strong scaling和weak scaling测试

## 项目结构

```
pard/
├── include/          # 头文件
├── src/             # 源代码
│   ├── core/        # 核心数据结构
│   ├── ordering/    # 重排序算法
│   ├── symbolic/    # 符号分解
│   ├── factorization/ # 数值分解
│   ├── solve/       # 求解器
│   ├── refinement/  # 迭代精化
│   └── mpi/         # MPI并行支持
├── tests/           # 测试程序
└── examples/        # 示例程序
```

## 许可证

本项目采用MIT许可证。

## 贡献

欢迎提交Issue和Pull Request。

## 参考

- Panua-Pardiso: https://panua.ch/pardiso/
- MUMPS: https://mumps-solver.org/
- SuiteSparse Matrix Collection: https://sparse.tamu.edu/
