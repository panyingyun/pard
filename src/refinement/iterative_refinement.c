#include "pard.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/**
 * 迭代精化：提高求解精度
 * 使用残差修正方法
 */
int pard_iterative_refinement(pard_solver_t *solver, int nrhs, 
                                const double *rhs, double *sol,
                                int max_iter, double tol) {
    if (solver == NULL || rhs == NULL || sol == NULL || nrhs <= 0) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    pard_csr_matrix_t *A = solver->matrix;
    int n = A->n;
    
    double *residual = (double *)malloc(n * nrhs * sizeof(double));
    double *correction = (double *)malloc(n * nrhs * sizeof(double));
    double *temp = (double *)malloc(n * sizeof(double));
    
    if (residual == NULL || correction == NULL || temp == NULL) {
        free(residual);
        free(correction);
        free(temp);
        return PARD_ERROR_MEMORY;
    }
    
    /* 计算初始残差 */
    for (int rhs_idx = 0; rhs_idx < nrhs; rhs_idx++) {
        double *sol_rhs = sol + rhs_idx * n;
        double *rhs_rhs = (double *)rhs + rhs_idx * n;
        double *res_rhs = residual + rhs_idx * n;
        
        /* 计算残差：r = b - A*x */
        for (int i = 0; i < n; i++) {
            double sum = 0.0;
            for (int j = A->row_ptr[i]; j < A->row_ptr[i + 1]; j++) {
                sum += A->values[j] * sol_rhs[A->col_idx[j]];
            }
            res_rhs[i] = rhs_rhs[i] - sum;
        }
    }
    
    /* 迭代精化 */
    for (int iter = 0; iter < max_iter; iter++) {
        /* 计算残差的范数 */
        double max_res_norm = 0.0;
        for (int rhs_idx = 0; rhs_idx < nrhs; rhs_idx++) {
            double *res_rhs = residual + rhs_idx * n;
            double res_norm = 0.0;
            for (int i = 0; i < n; i++) {
                res_norm += res_rhs[i] * res_rhs[i];
            }
            res_norm = sqrt(res_norm);
            if (res_norm > max_res_norm) {
                max_res_norm = res_norm;
            }
        }
        
        if (max_res_norm < tol) {
            break;  /* 收敛 */
        }
        
        /* 求解修正量：A*correction = residual */
        int err = pard_solve_system(solver, nrhs, residual, correction);
        if (err != PARD_SUCCESS) {
            free(residual);
            free(correction);
            free(temp);
            return err;
        }
        
        /* 更新解：x = x + correction */
        for (int rhs_idx = 0; rhs_idx < nrhs; rhs_idx++) {
            double *sol_rhs = sol + rhs_idx * n;
            double *corr_rhs = correction + rhs_idx * n;
            for (int i = 0; i < n; i++) {
                sol_rhs[i] += corr_rhs[i];
            }
        }
        
        /* 重新计算残差 */
        for (int rhs_idx = 0; rhs_idx < nrhs; rhs_idx++) {
            double *sol_rhs = sol + rhs_idx * n;
            double *rhs_rhs = (double *)rhs + rhs_idx * n;
            double *res_rhs = residual + rhs_idx * n;
            
            for (int i = 0; i < n; i++) {
                double sum = 0.0;
                for (int j = A->row_ptr[i]; j < A->row_ptr[i + 1]; j++) {
                    sum += A->values[j] * sol_rhs[A->col_idx[j]];
                }
                res_rhs[i] = rhs_rhs[i] - sum;
            }
        }
    }
    
    free(residual);
    free(correction);
    free(temp);
    
    return PARD_SUCCESS;
}
