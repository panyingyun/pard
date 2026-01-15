#include "pard.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 前向声明 */
int pard_backward_substitution_ldlt(const pard_factors_t *factors,
                                    const double *y, double *x, int nrhs);

/**
 * 后向替换：求解 U*x = y（上三角系统）
 */
int pard_backward_substitution(const pard_factors_t *factors,
                                const double *y, double *x, int nrhs) {
    if (factors == NULL || y == NULL || x == NULL || nrhs <= 0) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int n = factors->n;
    
    /* 复制y到x */
    memcpy(x, y, n * nrhs * sizeof(double));
    
    /* 对于每个右端项 */
    for (int rhs = 0; rhs < nrhs; rhs++) {
        double *x_rhs = x + rhs * n;
        
        /* 后向替换 */
        for (int i = n - 1; i >= 0; i--) {
            double sum = x_rhs[i];
            
            /* 减去U[i][j] * x[j]（j > i） */
            for (int j = factors->u_row_ptr[i]; j < factors->u_row_ptr[i + 1]; j++) {
                int col = factors->u_col_idx[j];
                if (col > i) {
                    sum -= factors->u_values[j] * x_rhs[col];
                }
            }
            
            /* 除以U[i][i] */
            double diag = 0.0;
            for (int j = factors->u_row_ptr[i]; j < factors->u_row_ptr[i + 1]; j++) {
                if (factors->u_col_idx[j] == i) {
                    diag = factors->u_values[j];
                    break;
                }
            }
            
            if (fabs(diag) < 1e-15) {
                return PARD_ERROR_NUMERICAL;
            }
            
            x_rhs[i] = sum / diag;
        }
    }
    
    return PARD_SUCCESS;
}

/**
 * 后向替换（LDL^T分解）：求解 D*L^T*x = y
 */
int pard_backward_substitution_ldlt(const pard_factors_t *factors,
                                    const double *y, double *x, int nrhs) {
    if (factors == NULL || y == NULL || x == NULL || nrhs <= 0) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int n = factors->n;
    
    /* 首先求解 D*z = y */
    double *z = (double *)malloc(n * nrhs * sizeof(double));
    if (z == NULL) {
        return PARD_ERROR_MEMORY;
    }
    
    for (int rhs = 0; rhs < nrhs; rhs++) {
        const double *y_rhs = y + rhs * n;
        double *z_rhs = z + rhs * n;
        
        int i = 0;
        while (i < n) {
            if (factors->pivot_type[i] == 2 && i < n - 1) {
                /* 2x2主元块 */
                double d11 = factors->d_values[i];
                double d22 = factors->d_values[i + 1];
                /* 对于2x2块，D的存储方式需要根据实际实现调整 */
                z_rhs[i] = d11 * y_rhs[i];
                z_rhs[i + 1] = d22 * y_rhs[i + 1];
                i += 2;
            } else {
                /* 1x1主元 */
                z_rhs[i] = factors->d_values[i] * y_rhs[i];
                i++;
            }
        }
    }
    
    /* 然后求解 L^T*x = z（后向替换） */
    memcpy(x, z, n * nrhs * sizeof(double));
    
    for (int rhs = 0; rhs < nrhs; rhs++) {
        double *x_rhs = x + rhs * n;
        
        for (int i = n - 1; i >= 0; i--) {
            double sum = x_rhs[i];
            
            /* 减去L^T[i][j] * x[j]（j > i） */
            for (int j = 0; j < n; j++) {
                if (j > i) {
                    /* 查找L[j][i] */
                    for (int k = factors->row_ptr[j]; k < factors->row_ptr[j + 1]; k++) {
                        if (factors->col_idx[k] == i) {
                            sum -= factors->l_values[k] * x_rhs[j];
                            break;
                        }
                    }
                }
            }
            
            x_rhs[i] = sum;
        }
    }
    
    free(z);
    return PARD_SUCCESS;
}
