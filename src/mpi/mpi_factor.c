#include "pard.h"
#include <mpi.h>

/* 前向声明 */
extern int pard_ldlt_factorization(pard_solver_t *solver);
extern int pard_lu_factorization(pard_solver_t *solver);

/**
 * MPI并行分解
 * 这是一个简化版本，实际应该使用更复杂的并行算法
 */
int pard_mpi_factorization(pard_solver_t *solver) {
    if (solver == NULL || !solver->is_parallel) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    /* 对于简化版本，我们在每个进程上对本地矩阵进行分解 */
    /* 实际实现应该使用基于消元树的并行分解算法 */
    
    /* 这里调用串行分解（每个进程处理自己的部分） */
    /* 更完整的实现需要进程间通信和任务调度 */
    
    /* 暂时使用串行版本，后续可以优化 */
    if (solver->matrix_type == PARD_MATRIX_TYPE_REAL_SYMMETRIC_INDEF) {
        return pard_ldlt_factorization(solver);
    } else {
        return pard_lu_factorization(solver);
    }
}
