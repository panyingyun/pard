#include "pard.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <mpi.h>

/* 创建测试矩阵（类似集成测试） */
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
        
        /* 对角线元素：使用较大的值使其数值稳定 */
        (*matrix)->col_idx[pos] = i;
        (*matrix)->values[pos] = (double)(n + 1);
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

/* 性能统计结构 */
typedef struct {
    double analysis_time;
    double factorization_time;
    double solve_time;
    double total_time;
    int fill_in_nnz;
    double max_residual;
} perf_stats_t;

/* 运行性能测试 */
int run_perf_test(int n, pard_matrix_type_t mtype, perf_stats_t *stats) {
    MPI_Comm comm = MPI_COMM_WORLD;
    int rank;
    MPI_Comm_rank(comm, &rank);
    
    /* 创建测试矩阵 */
    pard_csr_matrix_t *matrix = NULL;
    int symmetric = (mtype == PARD_MATRIX_TYPE_REAL_SYMMETRIC_INDEF || 
                     mtype == PARD_MATRIX_TYPE_REAL_SYMMETRIC_POSDEF);
    int err = create_test_matrix(&matrix, n, symmetric);
    if (err != PARD_SUCCESS) {
        return err;
    }
    
    if (rank == 0) {
        printf("Created test matrix: %dx%d\n", n, n);
        pard_matrix_print_info(matrix);
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
    stats->analysis_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (err != PARD_SUCCESS) {
        pardiso_cleanup(&solver);
        pard_csr_free(&matrix);
        return err;
    }
    
    /* 数值分解 */
    start = clock();
    err = pardiso_factor(solver);
    end = clock();
    stats->factorization_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (err != PARD_SUCCESS) {
        pardiso_cleanup(&solver);
        pard_csr_free(&matrix);
        return err;
    }
    
    /* 获取fill_in_nnz */
    if (solver != NULL) {
        stats->fill_in_nnz = solver->fill_in_nnz;
    } else {
        stats->fill_in_nnz = 0;
    }
    
    /* 创建右端项并求解 */
    double *rhs = (double *)malloc(n * sizeof(double));
    double *sol = (double *)malloc(n * sizeof(double));
    
    if (rhs == NULL || sol == NULL) {
        if (rhs != NULL) free(rhs);
        if (sol != NULL) free(sol);
        pardiso_cleanup(&solver);
        pard_csr_free(&matrix);
        return PARD_ERROR_MEMORY;
    }
    
    for (int i = 0; i < n; i++) {
        rhs[i] = 1.0;
    }
    
    /* 求解 */
    start = clock();
    err = pardiso_solve(solver, 1, rhs, sol);
    end = clock();
    stats->solve_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (err != PARD_SUCCESS) {
        free(rhs);
        free(sol);
        pardiso_cleanup(&solver);
        pard_csr_free(&matrix);
        return err;
    }
    
    /* 计算残差 */
    double max_residual = 0.0;
    if (matrix != NULL && matrix->row_ptr != NULL && 
        matrix->col_idx != NULL && matrix->values != NULL) {
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
    }
    stats->max_residual = max_residual;
    stats->total_time = stats->analysis_time + stats->factorization_time + stats->solve_time;
    
    if (rank == 0) {
        printf("\nPerformance Statistics:\n");
        printf("  Analysis time:      %.6f seconds\n", stats->analysis_time);
        printf("  Factorization time: %.6f seconds\n", stats->factorization_time);
        printf("  Solve time:         %.6f seconds\n", stats->solve_time);
        printf("  Total time:         %.6f seconds\n", stats->total_time);
        printf("  Fill-in nnz:        %d\n", stats->fill_in_nnz);
        printf("  Max residual:       %.2e\n", stats->max_residual);
    }
    
    free(rhs);
    free(sol);
    pardiso_cleanup(&solver);
    pard_csr_free(&matrix);
    
    return PARD_SUCCESS;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (argc < 2) {
        if (rank == 0) {
            printf("Usage: %s <matrix_size> [matrix_type] [num_cores]\n", argv[0]);
            printf("  matrix_size: e.g., 500, 1000\n");
            printf("  matrix_type: 11=non-symmetric (default)\n");
            printf("  num_cores: 1, 2, 4 (default: 1)\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    int n = atoi(argv[1]);
    if (n <= 0) {
        if (rank == 0) {
            printf("Error: Invalid matrix size\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    pard_matrix_type_t mtype = PARD_MATRIX_TYPE_REAL_NONSYMMETRIC;
    if (argc >= 3) {
        mtype = (pard_matrix_type_t)atoi(argv[2]);
    }
    
    if (rank == 0) {
        printf("=== PARD Performance Test ===\n");
        printf("Matrix size: %dx%d\n", n, n);
        printf("Matrix type: %d\n", mtype);
    }
    
    perf_stats_t stats = {0};
    int err = run_perf_test(n, mtype, &stats);
    
    if (err != PARD_SUCCESS) {
        if (rank == 0) {
            printf("Error: %d\n", err);
        }
        MPI_Finalize();
        return 1;
    }
    
    MPI_Finalize();
    return 0;
}
