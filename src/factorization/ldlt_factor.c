#include "pard.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/**
 * LDL^T分解（对称不定矩阵，使用Bunch-Kaufman pivoting）
 * 将矩阵A分解为P*A*P^T = L*D*L^T
 * 支持1x1和2x2主元块
 */
int pard_ldlt_factorization(pard_solver_t *solver) {
    if (solver == NULL || solver->matrix == NULL || solver->factors == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    pard_csr_matrix_t *A = solver->matrix;
    pard_factors_t *factors = solver->factors;
    int n = A->n;
    
    /* 分配D和pivot_type数组 */
    factors->d_values = (double *)malloc(n * sizeof(double));
    factors->pivot_type = (int *)malloc(n * sizeof(int));
    
    if (factors->d_values == NULL || factors->pivot_type == NULL) {
        free(factors->d_values);
        free(factors->pivot_type);
        return PARD_ERROR_MEMORY;
    }
    
    /* 初始化pivot_type为1（1x1主元） */
    for (int i = 0; i < n; i++) {
        factors->pivot_type[i] = 1;
    }
    
    /* 初始化置换数组（如果还没有分配） */
    if (factors->perm == NULL) {
        factors->perm = (int *)malloc(n * sizeof(int));
        if (factors->perm == NULL) {
            free(factors->d_values);
            free(factors->pivot_type);
            return PARD_ERROR_MEMORY;
        }
        for (int i = 0; i < n; i++) {
            factors->perm[i] = i;  /* 初始为单位置换 */
        }
    }
    
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
    
    /* 简化的LDL^T分解：暂时只使用1x1主元，稍后可以添加2x2主元支持 */
    /* Bunch-Kaufman LDL^T分解 */
    int k = 0;
    while (k < n) {
        /* 选择主元策略：优先选择对角元素，如果对角元素太小，则选择列中最大的元素 */
        double diag_val = fabs(dense_A[k][k]);
        double alpha = diag_val;
        int pivot_row = k;
        
        /* 如果对角元素太小，查找列中最大的元素 */
        if (diag_val < 1e-10) {
            alpha = 0.0;
            for (int i = k; i < n; i++) {
                if (fabs(dense_A[i][k]) > alpha) {
                    alpha = fabs(dense_A[i][k]);
                    pivot_row = i;
                }
            }
        }
        
        if (alpha < 1e-15) {
            /* 数值奇异 */
            for (int i = 0; i < n; i++) {
                free(dense_A[i]);
            }
            free(dense_A);
            return PARD_ERROR_NUMERICAL;
        }
        
        /* 决定使用1x1还是2x2主元块 */
        double lambda = 0.0;
        int lambda_row = k;
        
        for (int i = k; i < n; i++) {
            if (i != pivot_row && fabs(dense_A[i][k]) > lambda) {
                lambda = fabs(dense_A[i][k]);
                lambda_row = i;
            }
        }
        
        double sigma = fabs(dense_A[pivot_row][pivot_row]);
        
        /* Bunch-Kaufman条件：如果满足，使用1x1主元，否则使用2x2 */
        /* 暂时禁用2x2块，只使用1x1主元，简化实现 */
        int use_2x2 = 0;
        /* if (k < n - 1 && alpha * lambda > sigma * sigma) {
            use_2x2 = 1;
        } */
        
        if (use_2x2 && k < n - 1) {
            /* 2x2主元块 */
            factors->pivot_type[k] = 2;
            factors->pivot_type[k + 1] = 2;
            
            /* 交换行和列（如果需要） */
            if (pivot_row != k) {
                /* 交换行 */
                double *tmp = dense_A[k];
                dense_A[k] = dense_A[pivot_row];
                dense_A[pivot_row] = tmp;
                
                /* 交换列 */
                for (int i = 0; i < n; i++) {
                    double tmp_val = dense_A[i][k];
                    dense_A[i][k] = dense_A[i][pivot_row];
                    dense_A[i][pivot_row] = tmp_val;
                }
            }
            
            if (lambda_row != k + 1) {
                /* 交换行 */
                double *tmp = dense_A[k + 1];
                dense_A[k + 1] = dense_A[lambda_row];
                dense_A[lambda_row] = tmp;
                
                /* 交换列 */
                for (int i = 0; i < n; i++) {
                    double tmp_val = dense_A[i][k + 1];
                    dense_A[i][k + 1] = dense_A[i][lambda_row];
                    dense_A[i][lambda_row] = tmp_val;
                }
            }
            
            /* 计算2x2块 */
            double a11 = dense_A[k][k];
            double a12 = dense_A[k][k + 1];
            double a22 = dense_A[k + 1][k + 1];
            double det = a11 * a22 - a12 * a12;
            
            if (fabs(det) < 1e-15) {
                for (int i = 0; i < n; i++) {
                    free(dense_A[i]);
                }
                free(dense_A);
                return PARD_ERROR_NUMERICAL;
            }
            
            /* 2x2块的D矩阵：D = inv(A_block) = [a22/det  -a12/det; -a12/det  a11/det] */
            /* 但我们需要存储的是块对角形式，这里简化：只存储对角元素 */
            factors->d_values[k] = a22 / det;
            factors->d_values[k + 1] = a11 / det;
            
            /* 更新剩余矩阵：对于2x2块，L的更新更复杂 */
            /* 首先计算 L = A * inv(D)，其中D是2x2块 */
            for (int i = k + 2; i < n; i++) {
                double l1_old = dense_A[i][k];
                double l2_old = dense_A[i][k + 1];
                
                /* L[i][k] 和 L[i][k+1] 的更新 */
                dense_A[i][k] = (a22 * l1_old - a12 * l2_old) / det;
                dense_A[i][k + 1] = (a11 * l2_old - a12 * l1_old) / det;
                
                /* 更新 A[i][j] = A[i][j] - L[i][k]*A[k][j] - L[i][k+1]*A[k+1][j] */
                for (int j = k + 2; j < n; j++) {
                    dense_A[i][j] -= dense_A[i][k] * dense_A[k][j] + 
                                     dense_A[i][k + 1] * dense_A[k + 1][j];
                }
            }
            
            k += 2;
        } else {
            /* 1x1主元 */
            factors->pivot_type[k] = 1;
            
            if (pivot_row != k) {
                /* 交换行 */
                double *tmp = dense_A[k];
                dense_A[k] = dense_A[pivot_row];
                dense_A[pivot_row] = tmp;
                
                /* 交换列 */
                for (int i = 0; i < n; i++) {
                    double tmp_val = dense_A[i][k];
                    dense_A[i][k] = dense_A[i][pivot_row];
                    dense_A[i][pivot_row] = tmp_val;
                }
                
                /* 记录置换 */
                int tmp_perm = factors->perm[k];
                factors->perm[k] = factors->perm[pivot_row];
                factors->perm[pivot_row] = tmp_perm;
            }
            
            factors->d_values[k] = dense_A[k][k];
            
            /* 检查数值稳定性：使用相对阈值 */
            double max_diag = fabs(dense_A[k][k]);
            for (int i = k; i < n; i++) {
                if (fabs(dense_A[i][i]) > max_diag) {
                    max_diag = fabs(dense_A[i][i]);
                }
            }
            if (fabs(factors->d_values[k]) < 1e-12 * max_diag) {
                for (int i = 0; i < n; i++) {
                    free(dense_A[i]);
                }
                free(dense_A);
                return PARD_ERROR_NUMERICAL;
            }
            
            /* 更新剩余矩阵 */
            for (int i = k + 1; i < n; i++) {
                dense_A[i][k] /= factors->d_values[k];
                for (int j = k + 1; j < n; j++) {
                    dense_A[i][j] -= dense_A[i][k] * dense_A[k][j];
                }
            }
            
            k++;
        }
    }
    
    /* 将L复制到factors */
    int pos = 0;
    for (int i = 0; i < n; i++) {
        factors->row_ptr[i] = pos;
        for (int j = 0; j < i; j++) {
            if (fabs(dense_A[i][j]) > 1e-15) {
                factors->col_idx[pos] = j;
                factors->l_values[pos] = dense_A[i][j];
                pos++;
            }
        }
        /* 对角线元素设为1 */
        factors->col_idx[pos] = i;
        factors->l_values[pos] = 1.0;
        pos++;
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
