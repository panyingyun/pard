#include "pard.h"
#include <stdlib.h>
#include <string.h>

/* 前向声明 */
extern int pard_forward_substitution_ldlt(const pard_factors_t *factors,
                                            const double *b, double *y, int nrhs);
extern int pard_backward_substitution_ldlt(const pard_factors_t *factors,
                                            const double *y, double *x, int nrhs);

/**
 * 求解线性系统：A*x = b
 * 根据矩阵类型选择LU或LDL^T分解
 */
int pard_solve_system(pard_solver_t *solver, int nrhs, const double *rhs, double *sol) {
    if (solver == NULL || solver->factors == NULL || rhs == NULL || sol == NULL || nrhs <= 0) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    pard_factors_t *factors = solver->factors;
    int n = factors->n;
    
    /* 应用行置换到右端项 */
    double *perm_rhs = (double *)malloc(n * nrhs * sizeof(double));
    if (perm_rhs == NULL) {
        return PARD_ERROR_MEMORY;
    }
    
    for (int rhs_idx = 0; rhs_idx < nrhs; rhs_idx++) {
        for (int i = 0; i < n; i++) {
            perm_rhs[rhs_idx * n + i] = rhs[rhs_idx * n + factors->perm[i]];
        }
    }
    
    double *y = (double *)malloc(n * nrhs * sizeof(double));
    if (y == NULL) {
        free(perm_rhs);
        return PARD_ERROR_MEMORY;
    }
    
    int err;
    
    if (factors->matrix_type == PARD_MATRIX_TYPE_REAL_SYMMETRIC_INDEF) {
        /* LDL^T分解 */
        err = pard_forward_substitution_ldlt(factors, perm_rhs, y, nrhs);
        if (err != PARD_SUCCESS) {
            free(perm_rhs);
            free(y);
            return err;
        }
        
        err = pard_backward_substitution_ldlt(factors, y, sol, nrhs);
    } else {
        /* LU分解 */
        err = pard_forward_substitution(factors, perm_rhs, y, nrhs);
        if (err != PARD_SUCCESS) {
            free(perm_rhs);
            free(y);
            return err;
        }
        
        err = pard_backward_substitution(factors, y, sol, nrhs);
    }
    
    if (err != PARD_SUCCESS) {
        free(perm_rhs);
        free(y);
        return err;
    }
    
    /* 应用列置换的逆（如果需要） */
    /* 对于对称矩阵，行置换和列置换相同，所以不需要额外操作 */
    /* 对于非对称矩阵，可能需要额外的列置换 */
    
    free(perm_rhs);
    free(y);
    
    return PARD_SUCCESS;
}
