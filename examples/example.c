#include "pard.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (argc < 2) {
        if (rank == 0) {
            printf("Usage: %s <matrix_file.mtx>\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }
    
    /* 读取矩阵 */
    pard_csr_matrix_t *matrix = NULL;
    int err = pard_matrix_read_mtx(&matrix, argv[1]);
    if (err != PARD_SUCCESS) {
        if (rank == 0) {
            printf("Error reading matrix file\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    if (rank == 0) {
        pard_matrix_print_info(matrix);
    }
    
    /* 确定矩阵类型 */
    pard_matrix_type_t mtype;
    if (matrix->is_symmetric) {
        /* 简化：假设是对称不定 */
        mtype = PARD_MATRIX_TYPE_REAL_SYMMETRIC_INDEF;
    } else {
        mtype = PARD_MATRIX_TYPE_REAL_NONSYMMETRIC;
    }
    
    /* 初始化求解器 */
    pard_solver_t *solver = NULL;
    err = pardiso_init(&solver, mtype, MPI_COMM_WORLD);
    if (err != PARD_SUCCESS) {
        if (rank == 0) {
            printf("Error initializing solver\n");
        }
        pard_csr_free(&matrix);
        MPI_Finalize();
        return 1;
    }
    
    /* 符号分解 */
    err = pardiso_symbolic(solver, matrix);
    if (err != PARD_SUCCESS) {
        if (rank == 0) {
            printf("Error in symbolic factorization\n");
        }
        pardiso_cleanup(&solver);
        MPI_Finalize();
        return 1;
    }
    
    if (rank == 0) {
        printf("Symbolic factorization completed in %.3f seconds\n", 
               solver->analysis_time);
    }
    
    /* 数值分解 */
    err = pardiso_factor(solver);
    if (err != PARD_SUCCESS) {
        if (rank == 0) {
            printf("Error in numerical factorization\n");
        }
        pardiso_cleanup(&solver);
        MPI_Finalize();
        return 1;
    }
    
    if (rank == 0) {
        printf("Numerical factorization completed in %.3f seconds\n",
               solver->factorization_time);
    }
    
    /* 创建测试右端项 */
    int n = matrix->n;
    double *rhs = (double *)malloc(n * sizeof(double));
    double *sol = (double *)malloc(n * sizeof(double));
    
    for (int i = 0; i < n; i++) {
        rhs[i] = 1.0;  /* 简单的测试向量 */
    }
    
    /* 求解 */
    err = pardiso_solve(solver, 1, rhs, sol);
    if (err != PARD_SUCCESS) {
        if (rank == 0) {
            printf("Error in solve\n");
        }
        free(rhs);
        free(sol);
        pardiso_cleanup(&solver);
        MPI_Finalize();
        return 1;
    }
    
    if (rank == 0) {
        printf("Solve completed in %.3f seconds\n", solver->solve_time);
        
        /* 验证解 */
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
        printf("Max residual: %.2e\n", max_residual);
    }
    
    free(rhs);
    free(sol);
    pardiso_cleanup(&solver);
    
    MPI_Finalize();
    return 0;
}
