#include "pard.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <mpi.h>

/* 运行PARD求解器并收集统计信息 */
int run_pard_benchmark(const char *matrix_file, pard_matrix_type_t mtype,
                       double *analysis_time, double *factorization_time,
                       double *solve_time, double *max_residual) {
    MPI_Comm comm = MPI_COMM_WORLD;
    int rank;
    MPI_Comm_rank(comm, &rank);
    
    /* 读取矩阵 */
    pard_csr_matrix_t *matrix = NULL;
    int err = pard_matrix_read_mtx(&matrix, matrix_file);
    if (err != PARD_SUCCESS) {
        return err;
    }
    
    /* 初始化求解器 */
    pard_solver_t *solver = NULL;
    err = pardiso_init(&solver, mtype, comm);
    if (err != PARD_SUCCESS) {
        pard_csr_free(&matrix);
        return err;
    }
    
    /* 符号分解 */
    clock_t start = clock();
    err = pardiso_symbolic(solver, matrix);
    clock_t end = clock();
    *analysis_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (err != PARD_SUCCESS) {
        pardiso_cleanup(&solver);
        return err;
    }
    
    /* 数值分解 */
    start = clock();
    err = pardiso_factor(solver);
    end = clock();
    *factorization_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (err != PARD_SUCCESS) {
        pardiso_cleanup(&solver);
        return err;
    }
    
    /* 创建右端项并求解 */
    int n = matrix->n;
    double *rhs = (double *)malloc(n * sizeof(double));
    double *sol = (double *)malloc(n * sizeof(double));
    
    for (int i = 0; i < n; i++) {
        rhs[i] = 1.0;
    }
    
    start = clock();
    err = pardiso_solve(solver, 1, rhs, sol);
    end = clock();
    *solve_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (err != PARD_SUCCESS) {
        free(rhs);
        free(sol);
        pardiso_cleanup(&solver);
        return err;
    }
    
    /* 计算残差 */
    double max_res = 0.0;
    for (int i = 0; i < n; i++) {
        double sum = 0.0;
        for (int j = matrix->row_ptr[i]; j < matrix->row_ptr[i + 1]; j++) {
            sum += matrix->values[j] * sol[matrix->col_idx[j]];
        }
        double residual = fabs(rhs[i] - sum);
        if (residual > max_res) {
            max_res = residual;
        }
    }
    *max_residual = max_res;
    
    free(rhs);
    free(sol);
    pardiso_cleanup(&solver);
    
    return PARD_SUCCESS;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (argc < 2) {
        if (rank == 0) {
            printf("Usage: %s <matrix_file.mtx> [matrix_type]\n", argv[0]);
            printf("This program runs PARD solver and outputs timing information\n");
            printf("for comparison with MUMPS.\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    pard_matrix_type_t mtype = PARD_MATRIX_TYPE_REAL_NONSYMMETRIC;
    if (argc >= 3) {
        mtype = (pard_matrix_type_t)atoi(argv[2]);
    }
    
    double analysis_time, factorization_time, solve_time, max_residual;
    
    int err = run_pard_benchmark(argv[1], mtype, &analysis_time,
                                  &factorization_time, &solve_time, &max_residual);
    
    if (err != PARD_SUCCESS) {
        if (rank == 0) {
            printf("Error running benchmark: %d\n", err);
        }
        MPI_Finalize();
        return 1;
    }
    
    if (rank == 0) {
        printf("=== PARD Performance Results ===\n");
        printf("Matrix: %s\n", argv[1]);
        printf("MPI processes: %d\n", size);
        printf("\n");
        printf("Analysis time:      %.6f seconds\n", analysis_time);
        printf("Factorization time: %.6f seconds\n", factorization_time);
        printf("Solve time:         %.6f seconds\n", solve_time);
        printf("Total time:         %.6f seconds\n",
               analysis_time + factorization_time + solve_time);
        printf("Max residual:       %.2e\n", max_residual);
        printf("\n");
        printf("Compare these results with MUMPS output.\n");
    }
    
    MPI_Finalize();
    return 0;
}
