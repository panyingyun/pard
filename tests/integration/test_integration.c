#include "pard.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

/* 创建测试矩阵 */
int create_test_matrix(pard_csr_matrix_t **matrix, int n, int symmetric) {
    /* 计算实际非零元素数 */
    int nnz = 0;
    for (int i = 0; i < n; i++) {
        nnz++;  /* 对角线 */
        if (i < n - 1) nnz++;  /* 下三角 */
        if (i > 0) nnz++;      /* 上三角 */
    }
    
    int err = pard_csr_create(matrix, n, nnz);
    if (err != PARD_SUCCESS) {
        return err;
    }
    
    (*matrix)->is_symmetric = symmetric;
    
    int pos = 0;
    for (int i = 0; i < n; i++) {
        (*matrix)->row_ptr[i] = pos;
        
        /* 对角线元素：对于对称不定矩阵，使用交替符号使其不定 */
        (*matrix)->col_idx[pos] = i;
        if (symmetric) {
            /* 对称不定：交替正负对角线元素 */
            (*matrix)->values[pos] = (i % 2 == 0) ? (double)(n + 1) : -(double)(n + 1);
        } else {
            /* 非对称或正定：正对角线 */
            (*matrix)->values[pos] = (double)(n + 1);
        }
        pos++;
        
        /* 相邻元素 */
        if (i < n - 1) {
            (*matrix)->col_idx[pos] = i + 1;
            (*matrix)->values[pos] = -1.0;
            pos++;
        }
        
        if (i > 0) {
            (*matrix)->col_idx[pos] = i - 1;
            (*matrix)->values[pos] = -1.0;
            pos++;
        }
    }
    (*matrix)->row_ptr[n] = pos;
    (*matrix)->nnz = pos;
    
    return PARD_SUCCESS;
}

/* 测试完整求解流程 */
int test_solve_flow(int n, pard_matrix_type_t mtype, int use_mpi) {
    MPI_Comm comm = use_mpi ? MPI_COMM_WORLD : MPI_COMM_NULL;
    int rank;
    if (use_mpi) {
        MPI_Comm_rank(comm, &rank);
    } else {
        rank = 0;
    }
    
    /* 创建测试矩阵 */
    pard_csr_matrix_t *matrix = NULL;
    int symmetric = (mtype == PARD_MATRIX_TYPE_REAL_SYMMETRIC_INDEF || 
                     mtype == PARD_MATRIX_TYPE_REAL_SYMMETRIC_POSDEF);
    int err = create_test_matrix(&matrix, n, symmetric);
    if (err != PARD_SUCCESS) {
        return err;
    }
    
    if (rank == 0 && !use_mpi) {
        printf("Testing solve flow: n=%d, type=%d\n", n, mtype);
    }
    
    /* 初始化求解器 */
    pard_solver_t *solver = NULL;
    err = pardiso_init(&solver, mtype, comm);
    if (err != PARD_SUCCESS) {
        pard_csr_free(&matrix);
        return err;
    }
    
    /* 符号分解 */
    err = pardiso_symbolic(solver, matrix);
    if (err != PARD_SUCCESS) {
        pardiso_cleanup(&solver);
        return err;
    }
    
    /* 数值分解 */
    err = pardiso_factor(solver);
    if (err != PARD_SUCCESS) {
        if (rank == 0 || !use_mpi) {
            printf("  ERROR: Factorization failed with error code: %d\n", err);
        }
        pardiso_cleanup(&solver);
        return err;
    }
    
    /* 创建右端项 */
    double *rhs = (double *)malloc(n * sizeof(double));
    double *sol = (double *)malloc(n * sizeof(double));
    
    for (int i = 0; i < n; i++) {
        rhs[i] = 1.0;
    }
    
    /* 求解 */
    err = pardiso_solve(solver, 1, rhs, sol);
    
    /* 验证解（只在rank 0或串行模式下） */
    if (rank == 0 || !use_mpi) {
        if (err != PARD_SUCCESS) {
            printf("  ERROR: Solve failed with error code: %d\n", err);
            if (rhs != NULL) free(rhs);
            if (sol != NULL) free(sol);
            if (solver != NULL) pardiso_cleanup(&solver);
            return err;
        }
        
        double max_residual = 0.0;
        for (int i = 0; i < n; i++) {
            double sum = 0.0;
            for (int j = matrix->row_ptr[i]; j < matrix->row_ptr[i + 1]; j++) {
                sum += matrix->values[j] * sol[matrix->col_idx[j]];
            }
            double residual = fabs(rhs[i] - sum);
            if (residual > max_residual) {
                max_residual = residual;
            }
        }
        
        printf("  Max residual: %.2e\n", max_residual);
        if (max_residual > 1e-10) {
            printf("  WARNING: Residual is large!\n");
        }
    } else {
        if (err != PARD_SUCCESS) {
            if (rhs != NULL) free(rhs);
            if (sol != NULL) free(sol);
            if (solver != NULL) pardiso_cleanup(&solver);
            return err;
        }
    }
    
    if (rhs != NULL) free(rhs);
    if (sol != NULL) free(sol);
    if (solver != NULL) pardiso_cleanup(&solver);
    
    return PARD_SUCCESS;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (rank == 0) {
        printf("Running integration tests...\n\n");
    }
    
    /* 测试非对称矩阵 */
    if (rank == 0) {
        printf("Test 1: Non-symmetric matrix (serial)\n");
    }
    test_solve_flow(100, PARD_MATRIX_TYPE_REAL_NONSYMMETRIC, 0);
    
    /* 测试对称不定矩阵 */
    if (rank == 0) {
        printf("\nTest 2: Symmetric indefinite matrix (serial)\n");
    }
    test_solve_flow(100, PARD_MATRIX_TYPE_REAL_SYMMETRIC_INDEF, 0);
    
    /* 测试MPI并行（如果有多于1个进程） */
    if (size > 1) {
        if (rank == 0) {
            printf("\nTest 3: MPI parallel solve (%d processes)\n", size);
        }
        test_solve_flow(200, PARD_MATRIX_TYPE_REAL_SYMMETRIC_INDEF, 1);
    }
    
    if (rank == 0) {
        printf("\nAll integration tests completed.\n");
    }
    
    MPI_Finalize();
    return 0;
}
