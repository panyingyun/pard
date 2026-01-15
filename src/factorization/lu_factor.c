#include "pard.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/**
 * LU分解（带部分主元选择）
 * 将矩阵A分解为P*A = L*U，其中P是置换矩阵
 */
int pard_lu_factorization(pard_solver_t *solver) {
    if (solver == NULL || solver->matrix == NULL || solver->factors == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    pard_csr_matrix_t *A = solver->matrix;
    pard_factors_t *factors = solver->factors;
    int n = A->n;
    
    /* 分配工作空间 */
    double *dense_row = (double *)calloc(n, sizeof(double));
    int *perm = factors->perm;
    int *inv_perm = (int *)malloc(n * sizeof(int));
    
    if (dense_row == NULL || inv_perm == NULL) {
        free(dense_row);
        free(inv_perm);
        return PARD_ERROR_MEMORY;
    }
    
    for (int i = 0; i < n; i++) {
        inv_perm[perm[i]] = i;
    }
    
    /* 将CSR转换为临时密集存储（用于分解） */
    double **dense_A = (double **)malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++) {
        dense_A[i] = (double *)calloc(n, sizeof(double));
    }
    
    for (int i = 0; i < n; i++) {
        for (int j = A->row_ptr[i]; j < A->row_ptr[i + 1]; j++) {
            dense_A[i][A->col_idx[j]] = A->values[j];
        }
    }
    
    /* 执行LU分解（带部分主元） */
    for (int k = 0; k < n; k++) {
        /* 部分主元选择：找到第k列中绝对值最大的元素 */
        int max_row = k;
        double max_val = fabs(dense_A[k][k]);
        
        for (int i = k + 1; i < n; i++) {
            if (fabs(dense_A[i][k]) > max_val) {
                max_val = fabs(dense_A[i][k]);
                max_row = i;
            }
        }
        
        /* 交换行 */
        if (max_row != k) {
            double *tmp = dense_A[k];
            dense_A[k] = dense_A[max_row];
            dense_A[max_row] = tmp;
            
            int tmp_perm = perm[k];
            perm[k] = perm[max_row];
            perm[max_row] = tmp_perm;
        }
        
        if (fabs(dense_A[k][k]) < 1e-15) {
            /* 数值奇异 */
            for (int i = 0; i < n; i++) {
                free(dense_A[i]);
            }
            free(dense_A);
            free(dense_row);
            free(inv_perm);
            return PARD_ERROR_NUMERICAL;
        }
        
        /* 计算L的第k列和U的第k行 */
        for (int i = k + 1; i < n; i++) {
            dense_A[i][k] /= dense_A[k][k];  /* L[i][k] */
            for (int j = k + 1; j < n; j++) {
                dense_A[i][j] -= dense_A[i][k] * dense_A[k][j];  /* 更新A[i][j] */
            }
        }
    }
    
    /* 将结果复制回CSR格式 */
    int l_pos = 0, u_pos = 0;
    
    for (int i = 0; i < n; i++) {
        factors->row_ptr[i] = l_pos;
        for (int j = 0; j <= i; j++) {
            if (fabs(dense_A[i][j]) > 1e-15 || i == j) {
                factors->col_idx[l_pos] = j;
                factors->l_values[l_pos] = (i == j) ? 1.0 : dense_A[i][j];
                l_pos++;
            }
        }
    }
    factors->row_ptr[n] = l_pos;
    
    for (int i = 0; i < n; i++) {
        factors->u_row_ptr[i] = u_pos;
        for (int j = i; j < n; j++) {
            if (fabs(dense_A[i][j]) > 1e-15 || i == j) {
                factors->u_col_idx[u_pos] = j;
                factors->u_values[u_pos] = dense_A[i][j];
                u_pos++;
            }
        }
    }
    factors->u_row_ptr[n] = u_pos;
    
    factors->nnz = l_pos + u_pos;
    
    /* 清理 */
    for (int i = 0; i < n; i++) {
        free(dense_A[i]);
    }
    free(dense_A);
    free(dense_row);
    free(inv_perm);
    
    return PARD_SUCCESS;
}
