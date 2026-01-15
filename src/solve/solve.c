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
    /* 注意：对于LU分解，factors->perm存在；对于LDLT分解，置换已在分解时处理 */
    double *perm_rhs = (double *)malloc(n * nrhs * sizeof(double));
    if (perm_rhs == NULL) {
        return PARD_ERROR_MEMORY;
    }
    
    if (factors->matrix_type == PARD_MATRIX_TYPE_REAL_SYMMETRIC_INDEF) {
        /* LDLT分解：需要应用factors中的置换（来自数值分解阶段的pivoting） */
        if (factors->perm == NULL) {
            memcpy(perm_rhs, rhs, n * nrhs * sizeof(double));
        } else {
            for (int rhs_idx = 0; rhs_idx < nrhs; rhs_idx++) {
                for (int i = 0; i < n; i++) {
                    perm_rhs[rhs_idx * n + i] = rhs[rhs_idx * n + factors->perm[i]];
                }
            }
        }
    } else {
        /* LU分解：需要应用factors中的置换 */
        if (factors->perm == NULL) {
            /* 如果没有置换，直接复制 */
            memcpy(perm_rhs, rhs, n * nrhs * sizeof(double));
        } else {
            for (int rhs_idx = 0; rhs_idx < nrhs; rhs_idx++) {
                for (int i = 0; i < n; i++) {
                    perm_rhs[rhs_idx * n + i] = rhs[rhs_idx * n + factors->perm[i]];
                }
            }
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
    if (factors->matrix_type == PARD_MATRIX_TYPE_REAL_SYMMETRIC_INDEF) {
        /* 对于LDLT分解，需要应用逆置换到解 */
        /* 如果perm[i] = j，意味着原始位置i被映射到位置j */
        /* 逆置换：如果perm[i] = j，那么inv_perm[j] = i */
        if (factors->perm != NULL) {
            double *tmp_sol = (double *)malloc(n * nrhs * sizeof(double));
            if (tmp_sol == NULL) {
                free(perm_rhs);
                free(y);
                return PARD_ERROR_MEMORY;
            }
            memcpy(tmp_sol, sol, n * nrhs * sizeof(double));
            /* 计算逆置换 */
            int *inv_perm = (int *)malloc(n * sizeof(int));
            if (inv_perm == NULL) {
                free(tmp_sol);
                free(perm_rhs);
                free(y);
                return PARD_ERROR_MEMORY;
            }
            for (int i = 0; i < n; i++) {
                inv_perm[factors->perm[i]] = i;
            }
            for (int rhs_idx = 0; rhs_idx < nrhs; rhs_idx++) {
                for (int i = 0; i < n; i++) {
                    sol[rhs_idx * n + i] = tmp_sol[rhs_idx * n + inv_perm[i]];
                }
            }
            free(inv_perm);
            free(tmp_sol);
        }
    } else {
        /* 对于非对称矩阵，可能需要额外的列置换 */
    }
    
    free(perm_rhs);
    free(y);
    
    return PARD_SUCCESS;
}
