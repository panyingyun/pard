#include "pard.h"
#include <stdlib.h>
#include <math.h>

/**
 * Cholesky分解：A = L * L^T（对称正定矩阵）
 */
int pard_cholesky_factorization(pard_solver_t *solver) {
    if (solver == NULL || solver->matrix == NULL || solver->factors == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    pard_csr_matrix_t *A = solver->matrix;
    pard_factors_t *factors = solver->factors;
    int n = A->n;
    
    /* 将CSR转换为临时密集存储 */
    double **dense_A = (double **)malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++) {
        dense_A[i] = (double *)calloc(n, sizeof(double));
    }
    
    for (int i = 0; i < n; i++) {
        for (int j = A->row_ptr[i]; j < A->row_ptr[i + 1]; j++) {
            int col = A->col_idx[j];
            dense_A[i][col] = A->values[j];
            if (A->is_symmetric && col != i) {
                dense_A[col][i] = A->values[j];
            }
        }
    }
    
    /* Cholesky分解 */
    for (int j = 0; j < n; j++) {
        if (dense_A[j][j] <= 0.0) {
            /* 不是正定矩阵 */
            for (int i = 0; i < n; i++) {
                free(dense_A[i]);
            }
            free(dense_A);
            return PARD_ERROR_NUMERICAL;
        }
        
        dense_A[j][j] = sqrt(dense_A[j][j]);
        
        for (int i = j + 1; i < n; i++) {
            dense_A[i][j] /= dense_A[j][j];
        }
        
        for (int k = j + 1; k < n; k++) {
            for (int i = k; i < n; i++) {
                dense_A[i][k] -= dense_A[i][j] * dense_A[k][j];
            }
        }
    }
    
    /* 将下三角部分复制到L */
    int pos = 0;
    for (int i = 0; i < n; i++) {
        factors->row_ptr[i] = pos;
        for (int j = 0; j <= i; j++) {
            if (fabs(dense_A[i][j]) > 1e-15 || i == j) {
                factors->col_idx[pos] = j;
                factors->l_values[pos] = dense_A[i][j];
                pos++;
            }
        }
    }
    factors->row_ptr[n] = pos;
    factors->nnz = pos;
    
    /* 清理 */
    for (int i = 0; i < n; i++) {
        free(dense_A[i]);
    }
    free(dense_A);
    
    return PARD_SUCCESS;
}
