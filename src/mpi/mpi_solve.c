#include "pard.h"
#include <stdlib.h>
#include <mpi.h>

/**
 * MPI并行求解
 * 使用分布式前向/后向替换
 */
int pard_mpi_solve(pard_solver_t *solver, int nrhs, 
                    const double *local_rhs, double *local_sol) {
    if (solver == NULL || !solver->is_parallel || 
        local_rhs == NULL || local_sol == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    /* 注意：当前实现是简化版本 */
    /* 对于真正的并行求解，需要进程间通信来协调前向/后向替换 */
    /* 这里暂时在rank 0上执行串行求解，然后广播结果 */
    
    int err = PARD_SUCCESS;
    
    if (solver->mpi_rank == 0) {
        /* 在rank 0上收集完整的rhs并求解 */
        int n = solver->matrix->n;
        double *global_rhs = (double *)malloc(n * nrhs * sizeof(double));
        double *global_sol = (double *)malloc(n * nrhs * sizeof(double));
        
        if (global_rhs == NULL || global_sol == NULL) {
            if (global_rhs != NULL) free(global_rhs);
            if (global_sol != NULL) free(global_sol);
            return PARD_ERROR_MEMORY;
        }
        
        /* 收集所有进程的rhs（使用Allgatherv会更准确，这里简化） */
        int local_n = n / solver->mpi_size;
        int remainder = n % solver->mpi_size;
        int *counts = (int *)malloc(solver->mpi_size * sizeof(int));
        int *displs = (int *)malloc(solver->mpi_size * sizeof(int));
        
        for (int i = 0; i < solver->mpi_size; i++) {
            counts[i] = (local_n + (i < remainder ? 1 : 0)) * nrhs;
            displs[i] = (i * local_n + (i < remainder ? i : remainder)) * nrhs;
        }
        
        MPI_Allgatherv(local_rhs, local_n * nrhs, MPI_DOUBLE,
                       global_rhs, counts, displs, MPI_DOUBLE, solver->comm);
        
        /* 在rank 0上求解 */
        err = pard_solve_system(solver, nrhs, global_rhs, global_sol);
        
        if (err == PARD_SUCCESS) {
            /* 分发解到各进程 */
            MPI_Scatterv(global_sol, counts, displs, MPI_DOUBLE,
                         local_sol, local_n * nrhs, MPI_DOUBLE, 0, solver->comm);
        }
        
        free(global_rhs);
        free(global_sol);
        free(counts);
        free(displs);
    } else {
        /* 其他进程参与收集和分发 */
        int n = solver->matrix->n;
        int local_n = n / solver->mpi_size;
        int remainder = n % solver->mpi_size;
        int *counts = (int *)malloc(solver->mpi_size * sizeof(int));
        int *displs = (int *)malloc(solver->mpi_size * sizeof(int));
        
        for (int i = 0; i < solver->mpi_size; i++) {
            counts[i] = (local_n + (i < remainder ? 1 : 0)) * nrhs;
            displs[i] = (i * local_n + (i < remainder ? i : remainder)) * nrhs;
        }
        
        double *global_rhs = (double *)malloc(n * nrhs * sizeof(double));
        double *global_sol = (double *)malloc(n * nrhs * sizeof(double));
        
        if (global_rhs != NULL && global_sol != NULL) {
            MPI_Allgatherv(local_rhs, local_n * nrhs, MPI_DOUBLE,
                           global_rhs, counts, displs, MPI_DOUBLE, solver->comm);
            
            /* rank 0会执行求解，其他进程等待 */
            MPI_Bcast(&err, 1, MPI_INT, 0, solver->comm);
            
            if (err == PARD_SUCCESS) {
                MPI_Scatterv(global_sol, counts, displs, MPI_DOUBLE,
                             local_sol, local_n * nrhs, MPI_DOUBLE, 0, solver->comm);
            }
        }
        
        if (global_rhs != NULL) free(global_rhs);
        if (global_sol != NULL) free(global_sol);
        if (counts != NULL) free(counts);
        if (displs != NULL) free(displs);
    }
    
    return err;
}
