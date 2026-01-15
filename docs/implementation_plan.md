# 稀疏矩阵求解器库实现计划

## 项目结构

```
pard/
├── include/
│   └── pard.h              # 主头文件，定义公共API
├── src/
│   ├── core/               # 核心数据结构
│   │   ├── csr_matrix.c    # CSR矩阵存储格式
│   │   └── matrix_utils.c  # 矩阵工具函数
│   ├── ordering/           # 重排序算法
│   │   ├── minimum_degree.c    # Minimum Degree算法
│   │   ├── nested_dissection.c # Nested Dissection算法
│   │   └── ordering_utils.c    # 重排序工具函数
│   ├── symbolic/           # 符号分解
│   │   ├── elimination_tree.c  # 消元树构建
│   │   └── symbolic_factor.c   # 符号分解主函数
│   ├── factorization/      # 数值分解
│   │   ├── lu_factor.c     # LU分解（非对称矩阵）
│   │   ├── cholesky_factor.c # Cholesky分解（对称正定）
│   │   └── ldlt_factor.c   # LDL^T分解（对称不定）
│   ├── solve/              # 求解器
│   │   ├── forward_sub.c   # 前向替换
│   │   ├── backward_sub.c  # 后向替换
│   │   └── solve.c         # 求解主函数
│   ├── refinement/         # 迭代精化
│   │   └── iterative_refinement.c
│   ├── mpi/                # MPI并行支持
│   │   ├── mpi_distribute.c    # 矩阵分布
│   │   ├── mpi_factor.c        # 并行分解
│   │   └── mpi_solve.c         # 并行求解
│   └── pard.c              # 主API实现
├── tests/
│   ├── unit/               # 单元测试
│   ├── integration/        # 集成测试
│   └── benchmark/          # 性能测试
│       ├── benchmark.c     # 基准测试主程序
│       ├── mumps_compare.c # 与MUMPS的性能对比程序
│       ├── download_matrices.sh # 从SuiteSparse下载测试矩阵
│       └── test_matrices/  # 测试矩阵数据（Matrix Market格式）
├── examples/               # 示例程序
│   └── example.c
├── CMakeLists.txt          # CMake构建配置
├── Makefile                # Makefile构建配置
└── README.md               # 项目文档
```

## 核心模块设计

### 1. 数据结构 (`include/pard.h`, `src/core/`)

- **CSR矩阵结构**：存储稀疏矩阵（行指针、列索引、数值数组）
- **分解结果结构**：存储L、U因子或L、D、L^T因子
- **求解器句柄**：封装符号分解和数值分解结果

### 2. 重排序模块 (`src/ordering/`)

- **Minimum Degree**：实现近似最小度算法（AMD）
- **Nested Dissection**：实现嵌套剖分算法（METIS风格）
- 目标：减少fill-in，降低分解复杂度

### 3. 符号分解模块 (`src/symbolic/`)

- **消元树构建**：基于矩阵结构构建消元树
- **符号分解**：确定L和U的非零结构
- 为数值分解提供内存分配和并行化信息

### 4. 数值分解模块 (`src/factorization/`)

- **LU分解**：非对称矩阵的LU分解（带部分主元选择）
- **LDL^T分解**：对称不定矩阵的LDL^T分解（Bunch-Kaufman pivoting）
- **Cholesky分解**：对称正定矩阵的Cholesky分解（LL^T）

### 5. 求解模块 (`src/solve/`)

- **前向/后向替换**：基于分解结果求解线性系统
- **多右端项支持**：支持同时求解多个右端项

### 6. 迭代精化模块 (`src/refinement/`)

- **迭代精化**：提高求解精度，处理数值不稳定情况

### 7. MPI并行模块 (`src/mpi/`)

- **矩阵分布**：将矩阵和右端项分布到多个进程
- **并行分解**：基于消元树的并行LU/LDL^T分解
- **并行求解**：分布式前向/后向替换

### 8. 主API (`src/pard.c`)

提供类似Pardiso的接口：

- `pardiso_init()`: 初始化求解器
- `pardiso_symbolic()`: 符号分解
- `pardiso_factor()`: 数值分解
- `pardiso_solve()`: 求解
- `pardiso_refine()`: 迭代精化
- `pardiso_cleanup()`: 清理资源

## 测试策略

### 单元测试 (`tests/unit/`)

- CSR矩阵操作测试
- 重排序算法正确性测试
- 符号分解正确性测试
- 分解和求解精度测试

### 集成测试 (`tests/integration/`)

- 完整求解流程测试
- 不同矩阵类型测试（对称/非对称、正定/不定）
- MPI并行正确性测试

### 性能测试 (`tests/benchmark/`)

- **测试矩阵来源**：使用SuiteSparse Matrix Collection（标准测试矩阵集）
  - 包含对称不定矩阵和非对称矩阵
  - 覆盖不同规模（10^4 - 10^6行）和稀疏度
  - 自动下载和解析Matrix Market格式（.mtx文件）
- **性能对比目标**：与MUMPS进行详细对比
  - 符号分析+重排序时间对比
  - 数值分解时间对比（LU/LDL^T）
  - 求解时间对比（前向/后向替换）
  - 内存使用对比（fill-in、峰值内存）
  - MPI并行扩展性对比（strong scaling和weak scaling）
  - 并行效率目标：不低于50-70%
- **性能指标**：
  - 分解时间（analysis + numerical factorization）
  - 求解时间（forward/backward substitution）
  - 内存使用（峰值内存、fill-in比例）
  - 并行加速比和效率
  - 数值精度（相对误差、残差）

## 性能优化策略

1. **缓存优化**：优化数据访问模式，提高缓存命中率
2. **SIMD优化**：在密集块计算中使用SIMD指令
3. **并行调度**：基于消元树的任务调度，最大化并行度（参考MUMPS的multifrontal方法）
4. **内存管理**：预分配、内存池、减少碎片
5. **数值稳定性**：智能主元选择，避免数值问题

   - 对称不定矩阵：1×1和2×2 Bunch-Kaufman pivoting（与MUMPS一致）
   - 非对称矩阵：部分主元选择策略

## 与MUMPS性能对比的关键维度

1. **符号分析阶段**：

   - 重排序算法效率（AMD vs METIS风格）
   - 消元树构建时间
   - 符号分解时间

2. **数值分解阶段**：

   - LU分解性能（非对称矩阵）
   - LDL^T分解性能（对称不定矩阵，Bunch-Kaufman pivoting）
   - 并行分解效率（MPI通信开销）

3. **求解阶段**：

   - 前向/后向替换性能
   - 多右端项求解效率
   - 迭代精化开销

4. **可扩展性**：

   - Strong scaling：固定问题规模，增加MPI进程数
   - Weak scaling：问题规模随MPI进程数增长
   - 并行效率目标：≥50-70%（与MUMPS相当）

5. **内存效率**：

   - Fill-in比例（与MUMPS对比）
   - 峰值内存使用
   - 内存分布平衡性

## 实现优先级

1. **Phase 1**：核心功能（CSR、LU分解、求解）
2. **Phase 2**：符号分解和重排序
3. **Phase 3**：对称矩阵支持（LDL^T、Cholesky）
4. **Phase 4**：迭代精化
5. **Phase 5**：MPI并行
6. **Phase 6**：性能优化和测试完善

## 实现状态

### 已完成
- ✅ Phase 1: 核心功能（CSR、LU分解、求解）
- ✅ Phase 2: 符号分解和重排序
- ✅ Phase 3: 对称矩阵支持（LDL^T、Cholesky）- 部分完成
- ✅ Phase 4: 迭代精化
- ✅ Phase 5: MPI并行 - 基础实现
- ✅ Phase 6: 性能优化和测试完善 - 进行中

### 待改进
- ⚠️ LDL^T分解的数值稳定性
- ⚠️ MPI并行性能优化
- ⚠️ 大规模矩阵测试
