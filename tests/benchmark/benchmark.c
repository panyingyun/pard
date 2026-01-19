#include "pard.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <mpi.h>

/* 性能统计结构 */
typedef struct {
    double analysis_time;
    double factorization_time;
    double solve_time;
    size_t peak_memory;
    int fill_in_nnz;
    double max_residual;
} performance_stats_t;

/* 运行基准测试 */
int run_benchmark(const char *matrix_file, pard_matrix_type_t mtype, 
                  int use_mpi, performance_stats_t *stats) {
    MPI_Comm comm = use_mpi ? MPI_COMM_WORLD : MPI_COMM_NULL;
    int rank;
    if (use_mpi) {
        MPI_Comm_rank(comm, &rank);
    } else {
        rank = 0;
    }
    
    /* 读取矩阵 */
    pard_csr_matrix_t *matrix = NULL;
    int err = pard_matrix_read_mtx(&matrix, matrix_file);
    if (err != PARD_SUCCESS) {
        if (rank == 0) {
            printf("Error reading matrix file: %s\n", matrix_file);
        }
        return err;
    }
    
    if (rank == 0) {
        printf("Matrix: %s\n", matrix_file);
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
        pard_csr_free(&matrix);  /* 释放matrix，因为pardiso_cleanup不再释放它 */
        return err;
    }
    
    /* 数值分解 */
    start = clock();
    err = pardiso_factor(solver);
    end = clock();
    stats->factorization_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (err != PARD_SUCCESS) {
        pardiso_cleanup(&solver);
        pard_csr_free(&matrix);  /* 释放matrix，因为pardiso_cleanup不再释放它 */
        return err;
    }
    
    stats->fill_in_nnz = solver->fill_in_nnz;
    
    /* 创建右端项 */
    /* 注意：使用solver->matrix，因为它已经被置换，但matrix和solver->matrix指向同一个对象 */
    int n = solver->matrix->n;
    double *rhs = (double *)malloc(n * sizeof(double));
    double *sol = (double *)malloc(n * sizeof(double));
    
    if (rhs == NULL || sol == NULL) {
        if (rhs != NULL) free(rhs);
        if (sol != NULL) free(sol);
        pardiso_cleanup(&solver);
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
        pard_csr_free(&matrix);  /* 释放matrix，因为pardiso_cleanup不再释放它 */
        matrix = NULL;  /* 标记已清理，避免后续访问 */
        return err;
    }
    
    /* 计算残差 - 使用solver->matrix（已经置换后的矩阵） */
    double max_residual = 0.0;
    pard_csr_matrix_t *A = solver->matrix;  /* 使用solver中的矩阵引用 */
    
    if (A != NULL && A->row_ptr != NULL && A->col_idx != NULL && A->values != NULL) {
        for (int i = 0; i < n; i++) {
            double sum = 0.0;
            for (int j = A->row_ptr[i]; j < A->row_ptr[i + 1]; j++) {
                sum += A->values[j] * sol[A->col_idx[j]];
            }
            double residual = fabs(rhs[i] - sum);
            if (residual > max_residual) {
                max_residual = residual;
            }
        }
    }
    stats->max_residual = max_residual;
    
    if (rank == 0) {
        printf("\nPerformance Statistics:\n");
        printf("  Analysis time:      %.6f seconds\n", stats->analysis_time);
        printf("  Factorization time: %.6f seconds\n", stats->factorization_time);
        printf("  Solve time:         %.6f seconds\n", stats->solve_time);
        printf("  Total time:         %.6f seconds\n", 
               stats->analysis_time + stats->factorization_time + stats->solve_time);
        printf("  Fill-in nnz:        %d\n", stats->fill_in_nnz);
        printf("  Max residual:       %.2e\n", stats->max_residual);
    }
    
    free(rhs);
    free(sol);
    
    /* 清理求解器（不释放matrix，因为matrix由benchmark管理） */
    pardiso_cleanup(&solver);
    
    /* 释放matrix（由benchmark管理） */
    pard_csr_free(&matrix);
    matrix = NULL;  /* 标记已清理，避免后续访问 */
    
    return PARD_SUCCESS;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (argc < 2) {
        if (rank == 0) {
            printf("Usage: %s <matrix_file.mtx> [matrix_type] [use_mpi]\n", argv[0]);
            printf("  matrix_type: 0=non-symmetric, 1=symmetric_posdef, -2=symmetric_indef\n");
            printf("  use_mpi: 0=serial, 1=parallel\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    pard_matrix_type_t mtype = PARD_MATRIX_TYPE_REAL_NONSYMMETRIC;
    if (argc >= 3) {
        mtype = (pard_matrix_type_t)atoi(argv[2]);
    }
    
    int use_mpi = (size > 1) ? 1 : 0;
    if (argc >= 4) {
        use_mpi = atoi(argv[3]);
    }
    
    if (rank == 0) {
        printf("=== PARD Benchmark ===\n");
        printf("Matrix file: %s\n", argv[1]);
        printf("Matrix type: %d\n", mtype);
        printf("MPI processes: %d\n", use_mpi ? size : 1);
        printf("\n");
    }
    
    performance_stats_t stats;
    int err = run_benchmark(argv[1], mtype, use_mpi, &stats);
    
    if (err != PARD_SUCCESS && rank == 0) {
        printf("Benchmark failed with error: %d\n", err);
    }
    
    MPI_Finalize();
    return (err == PARD_SUCCESS) ? 0 : 1;
}
