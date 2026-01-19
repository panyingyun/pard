#include "pard.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mpi.h>

/* 前向声明 */
extern int pard_minimum_degree(const pard_csr_matrix_t *matrix, int **perm, int **inv_perm);
extern int pard_nested_dissection(const pard_csr_matrix_t *matrix, int **perm, int **inv_perm);
extern int pard_build_elimination_tree(const pard_csr_matrix_t *matrix,
                                       int **parent, int **first_child, int **next_sibling);
extern int pard_symbolic_factorization(const pard_csr_matrix_t *matrix,
                                       const int *parent, const int *first_child,
                                       const int *next_sibling, pard_factors_t **factors);
extern int pard_lu_factorization(pard_solver_t *solver);
extern int pard_ldlt_factorization(pard_solver_t *solver);
extern int pard_cholesky_factorization(pard_solver_t *solver);
extern int pard_solve_system(pard_solver_t *solver, int nrhs, const double *rhs, double *sol);
extern int pard_iterative_refinement(pard_solver_t *solver, int nrhs,
                                      const double *rhs, double *sol,
                                      int max_iter, double tol);
extern int apply_permutation(pard_csr_matrix_t *matrix, const int *perm, const int *inv_perm);

/**
 * 初始化求解器
 */
int pardiso_init(pard_solver_t **solver, pard_matrix_type_t mtype, MPI_Comm comm) {
    if (solver == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    *solver = (pard_solver_t *)calloc(1, sizeof(pard_solver_t));
    if (*solver == NULL) {
        return PARD_ERROR_MEMORY;
    }
    
    (*solver)->matrix_type = mtype;
    (*solver)->comm = comm;
    (*solver)->is_parallel = (comm != MPI_COMM_NULL);
    
    if ((*solver)->is_parallel) {
        MPI_Comm_rank(comm, &((*solver)->mpi_rank));
        MPI_Comm_size(comm, &((*solver)->mpi_size));
    } else {
        (*solver)->mpi_rank = 0;
        (*solver)->mpi_size = 1;
    }
    
    return PARD_SUCCESS;
}

/**
 * 符号分解
 */
int pardiso_symbolic(pard_solver_t *solver, pard_csr_matrix_t *matrix) {
    if (solver == NULL || matrix == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    clock_t start = clock();
    
    solver->matrix = matrix;
    
    /* 重排序：使用最小度算法 */
    int *perm = NULL, *inv_perm = NULL;
    int err = pard_minimum_degree(matrix, &perm, &inv_perm);
    if (err != PARD_SUCCESS) {
        return err;
    }
    
    solver->perm = perm;
    solver->inv_perm = inv_perm;
    
    /* 应用置换 */
    err = apply_permutation(matrix, perm, inv_perm);
    if (err != PARD_SUCCESS) {
        free(perm);
        free(inv_perm);
        return err;
    }
    
    /* 构建消元树 */
    int *parent = NULL, *first_child = NULL, *next_sibling = NULL;
    err = pard_build_elimination_tree(matrix, &parent, &first_child, &next_sibling);
    if (err != PARD_SUCCESS) {
        free(perm);
        free(inv_perm);
        return err;
    }
    
    /* 符号分解 */
    pard_factors_t *factors = NULL;
    err = pard_symbolic_factorization(matrix, parent, first_child, next_sibling, &factors);
    if (err != PARD_SUCCESS) {
        free(perm);
        free(inv_perm);
        free(parent);
        free(first_child);
        free(next_sibling);
        return err;
    }
    
    solver->factors = factors;
    solver->factors->matrix_type = solver->matrix_type;  /* 设置正确的矩阵类型 */
    solver->fill_in_nnz = factors->nnz;
    
    free(parent);
    free(first_child);
    free(next_sibling);
    
    clock_t end = clock();
    solver->analysis_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    return PARD_SUCCESS;
}

/**
 * 数值分解
 */
int pardiso_factor(pard_solver_t *solver) {
    if (solver == NULL || solver->matrix == NULL || solver->factors == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    clock_t start = clock();
    
    int err;
    
    if (solver->is_parallel) {
        err = pard_mpi_factorization(solver);
    } else {
        if (solver->matrix_type == PARD_MATRIX_TYPE_REAL_SYMMETRIC_POSDEF) {
            err = pard_cholesky_factorization(solver);
        } else if (solver->matrix_type == PARD_MATRIX_TYPE_REAL_SYMMETRIC_INDEF) {
            err = pard_ldlt_factorization(solver);
        } else {
            err = pard_lu_factorization(solver);
        }
    }
    
    clock_t end = clock();
    solver->factorization_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    return err;
}

/**
 * 求解
 */
int pardiso_solve(pard_solver_t *solver, int nrhs, double *rhs, double *sol) {
    if (solver == NULL || solver->factors == NULL || rhs == NULL || sol == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    clock_t start = clock();
    
    int err;
    
    if (solver->is_parallel) {
        /* MPI并行求解 */
        double *local_rhs = NULL;
        int local_n = solver->matrix->n / solver->mpi_size;
        if (solver->mpi_rank < solver->matrix->n % solver->mpi_size) {
            local_n++;
        }
        
        err = pard_mpi_distribute_rhs(rhs, solver->matrix->n, nrhs, &local_rhs, solver->comm);
        if (err != PARD_SUCCESS) {
            return err;
        }
        
        double *local_sol = (double *)malloc(local_n * nrhs * sizeof(double));
        if (local_sol == NULL) {
            if (local_rhs != NULL) free(local_rhs);
            return PARD_ERROR_MEMORY;
        }
        
        err = pard_mpi_solve(solver, nrhs, local_rhs, local_sol);
        if (err == PARD_SUCCESS) {
            err = pard_mpi_gather_solution(local_sol, local_n, solver->matrix->n, nrhs, sol, solver->comm);
        }
        
        if (local_rhs != NULL) {
            free(local_rhs);
            local_rhs = NULL;
        }
        if (local_sol != NULL) {
            free(local_sol);
            local_sol = NULL;
        }
    } else {
        err = pard_solve_system(solver, nrhs, rhs, sol);
    }
    
    clock_t end = clock();
    solver->solve_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    return err;
}

/**
 * 迭代精化
 */
int pardiso_refine(pard_solver_t *solver, int nrhs, double *rhs, double *sol,
                   int max_iter, double tol) {
    if (solver == NULL || rhs == NULL || sol == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    return pard_iterative_refinement(solver, nrhs, rhs, sol, max_iter, tol);
}

/**
 * 清理资源
 */
int pardiso_cleanup(pard_solver_t **solver) {
    if (solver == NULL || *solver == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    pard_solver_t *s = *solver;
    
    /* 注意：matrix由外部传入，不应在cleanup中释放 */
    /* apply_permutation会修改matrix的内部结构，但matrix本身由调用者管理 */
    /* 如果需要在cleanup中释放，调用者应该在cleanup之后手动释放 */
    /* 这里我们不释放matrix，只清理solver自己的资源 */
    s->matrix = NULL;
    
    if (s->perm != NULL) {
        free(s->perm);
        s->perm = NULL;
    }
    
    if (s->inv_perm != NULL) {
        free(s->inv_perm);
        s->inv_perm = NULL;
    }
    
    if (s->factors != NULL) {
        if (s->factors->row_ptr != NULL) {
            free(s->factors->row_ptr);
            s->factors->row_ptr = NULL;
        }
        if (s->factors->col_idx != NULL) {
            free(s->factors->col_idx);
            s->factors->col_idx = NULL;
        }
        if (s->factors->l_values != NULL) {
            free(s->factors->l_values);
            s->factors->l_values = NULL;
        }
        if (s->factors->u_row_ptr != NULL) {
            free(s->factors->u_row_ptr);
            s->factors->u_row_ptr = NULL;
        }
        if (s->factors->u_col_idx != NULL) {
            free(s->factors->u_col_idx);
            s->factors->u_col_idx = NULL;
        }
        if (s->factors->u_values != NULL) {
            free(s->factors->u_values);
            s->factors->u_values = NULL;
        }
        if (s->factors->d_values != NULL) {
            free(s->factors->d_values);
            s->factors->d_values = NULL;
        }
        if (s->factors->pivot_type != NULL) {
            free(s->factors->pivot_type);
            s->factors->pivot_type = NULL;
        }
        if (s->factors->perm != NULL) {
            free(s->factors->perm);
            s->factors->perm = NULL;
        }
        free(s->factors);
        s->factors = NULL;
    }
    
    free(s);
    *solver = NULL;
    
    return PARD_SUCCESS;
}
