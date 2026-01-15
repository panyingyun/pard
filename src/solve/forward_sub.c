#include "pard.h"
#include <stdlib.h>
#include <string.h>

/* 前向声明 */
int pard_forward_substitution_ldlt(const pard_factors_t *factors,
                                    const double *b, double *y, int nrhs);

/**
 * 前向替换：求解 L*y = b（下三角系统）
 */
int pard_forward_substitution(const pard_factors_t *factors, 
                               const double *b, double *y, int nrhs) {
    if (factors == NULL || b == NULL || y == NULL || nrhs <= 0) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int n = factors->n;
    
    /* 复制右端项 */
    memcpy(y, b, n * nrhs * sizeof(double));
    
    /* 对于每个右端项 */
    for (int rhs = 0; rhs < nrhs; rhs++) {
        double *y_rhs = y + rhs * n;
        
        /* 前向替换 */
        for (int i = 0; i < n; i++) {
            double sum = y_rhs[i];
            
            /* 减去L[i][j] * y[j]（j < i） */
            for (int j = factors->row_ptr[i]; j < factors->row_ptr[i + 1]; j++) {
                int col = factors->col_idx[j];
                if (col < i) {
                    sum -= factors->l_values[j] * y_rhs[col];
                }
            }
            
            /* L[i][i] = 1（对于LU分解）或需要除以L[i][i] */
            /* 这里假设L的对角元素为1 */
            y_rhs[i] = sum;
        }
    }
    
    return PARD_SUCCESS;
}

/**
 * 前向替换（LDL^T分解）：求解 L*y = b
 */
int pard_forward_substitution_ldlt(const pard_factors_t *factors,
                                    const double *b, double *y, int nrhs) {
    if (factors == NULL || b == NULL || y == NULL || nrhs <= 0) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int n = factors->n;
    
    memcpy(y, b, n * nrhs * sizeof(double));
    
    for (int rhs = 0; rhs < nrhs; rhs++) {
        double *y_rhs = y + rhs * n;
        
        int i = 0;
        while (i < n) {
            if (factors->pivot_type[i] == 2 && i < n - 1) {
                /* 2x2主元块 */
                double sum1 = y_rhs[i];
                double sum2 = y_rhs[i + 1];
                
                for (int j = factors->row_ptr[i]; j < factors->row_ptr[i + 1]; j++) {
                    int col = factors->col_idx[j];
                    if (col < i) {
                        double l_val = factors->l_values[j];
                        sum1 -= l_val * y_rhs[col];
                        if (j < factors->row_ptr[i + 1] - 1) {
                            sum2 -= factors->l_values[j + 1] * y_rhs[col];
                        }
                    }
                }
                
                y_rhs[i] = sum1;
                y_rhs[i + 1] = sum2;
                i += 2;
            } else {
                /* 1x1主元 */
                double sum = y_rhs[i];
                
                for (int j = factors->row_ptr[i]; j < factors->row_ptr[i + 1]; j++) {
                    int col = factors->col_idx[j];
                    if (col < i) {
                        sum -= factors->l_values[j] * y_rhs[col];
                    }
                }
                
                y_rhs[i] = sum;
                i++;
            }
        }
    }
    
    return PARD_SUCCESS;
}
