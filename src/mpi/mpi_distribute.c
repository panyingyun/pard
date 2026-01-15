#include "pard.h"
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

/**
 * 将矩阵分布到多个MPI进程
 * 使用简单的行块分布
 */
int pard_mpi_distribute_matrix(pard_csr_matrix_t *matrix, MPI_Comm comm,
                                pard_csr_matrix_t **local_matrix) {
    if (matrix == NULL || local_matrix == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    int n = matrix->n;
    int local_n = n / size;
    int remainder = n % size;
    
    /* 计算每个进程的行范围 */
    int start_row = rank * local_n + (rank < remainder ? rank : remainder);
    int end_row = start_row + local_n + (rank < remainder ? 1 : 0);
    int actual_local_n = end_row - start_row;
    
    /* 计算本地非零元素数 */
    int local_nnz = 0;
    for (int i = start_row; i < end_row; i++) {
        local_nnz += matrix->row_ptr[i + 1] - matrix->row_ptr[i];
    }
    
    /* 创建本地矩阵 */
    int err = pard_csr_create(local_matrix, actual_local_n, local_nnz);
    if (err != PARD_SUCCESS) {
        return err;
    }
    
    (*local_matrix)->is_symmetric = matrix->is_symmetric;
    
    /* 复制本地行 */
    int pos = 0;
    (*local_matrix)->row_ptr[0] = 0;
    
    for (int i = 0; i < actual_local_n; i++) {
        int global_row = start_row + i;
        int row_start = matrix->row_ptr[global_row];
        int row_end = matrix->row_ptr[global_row + 1];
        
        for (int j = row_start; j < row_end; j++) {
            (*local_matrix)->col_idx[pos] = matrix->col_idx[j];
            (*local_matrix)->values[pos] = matrix->values[j];
            pos++;
        }
        (*local_matrix)->row_ptr[i + 1] = pos;
    }
    
    return PARD_SUCCESS;
}

/**
 * 分布右端项到多个MPI进程
 */
int pard_mpi_distribute_rhs(const double *global_rhs, int n, int nrhs,
                             double **local_rhs, MPI_Comm comm) {
    if (global_rhs == NULL || local_rhs == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    int local_n = n / size;
    int remainder = n % size;
    int start_row = rank * local_n + (rank < remainder ? rank : remainder);
    int end_row = start_row + local_n + (rank < remainder ? 1 : 0);
    int actual_local_n = end_row - start_row;
    
    *local_rhs = (double *)malloc(actual_local_n * nrhs * sizeof(double));
    if (*local_rhs == NULL) {
        return PARD_ERROR_MEMORY;
    }
    
    for (int rhs = 0; rhs < nrhs; rhs++) {
        for (int i = 0; i < actual_local_n; i++) {
            (*local_rhs)[rhs * actual_local_n + i] = 
                global_rhs[rhs * n + start_row + i];
        }
    }
    
    return PARD_SUCCESS;
}

/**
 * 收集本地解到全局解
 */
int pard_mpi_gather_solution(const double *local_sol, int local_n, int n, int nrhs,
                              double *global_sol, MPI_Comm comm) {
    if (local_sol == NULL || global_sol == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    /* 计算每个进程的行数 */
    int *counts = (int *)malloc(size * sizeof(int));
    int *displs = (int *)malloc(size * sizeof(int));
    
    if (counts == NULL || displs == NULL) {
        free(counts);
        free(displs);
        return PARD_ERROR_MEMORY;
    }
    
    int local_n_base = n / size;
    int remainder = n % size;
    
    for (int i = 0; i < size; i++) {
        counts[i] = (local_n_base + (i < remainder ? 1 : 0)) * nrhs;
        displs[i] = (i * local_n_base + (i < remainder ? i : remainder)) * nrhs;
    }
    
    /* 收集数据 */
    MPI_Allgatherv((void *)local_sol, local_n * nrhs, MPI_DOUBLE,
                    global_sol, counts, displs, MPI_DOUBLE, comm);
    
    free(counts);
    free(displs);
    
    return PARD_SUCCESS;
}
