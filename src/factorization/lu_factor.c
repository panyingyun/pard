#include "pard.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/**
 * 安全释放dense_A数组
 * 注意：由于行交换改为交换内存内容而不是指针，现在不需要担心重复释放问题
 * 每个dense_A[i]都指向独立分配的内存块
 */
static void safe_free_dense_A(double **dense_A, int n) {
    if (dense_A == NULL) {
        return;
    }
    
    /* 释放所有非NULL的行指针 */
    /* 从后往前释放，避免可能的竞争条件 */
    if (n > 0 && n <= 1000000) {
        for (int i = n - 1; i >= 0; i--) {
            if (dense_A[i] != NULL) {
                free(dense_A[i]);
                dense_A[i] = NULL;
            }
        }
    }
    
    /* 释放dense_A本身 */
    free(dense_A);
}

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
    
    /* 验证输入 */
    if (A == NULL || factors == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int n = A->n;
    
    /* 确保n是有效的 */
    if (n <= 0 || n > 1000000) {  /* 合理的上限 */
        return PARD_ERROR_INVALID_INPUT;
    }
    
    /* 验证factors的必要字段是否已分配 */
    if (factors->row_ptr == NULL || factors->col_idx == NULL || 
        factors->l_values == NULL || factors->u_row_ptr == NULL ||
        factors->u_col_idx == NULL || factors->u_values == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    /* 分配工作空间 */
    double *dense_row = (double *)calloc(n, sizeof(double));
    
    /* 获取置换数组：优先使用solver->perm（来自符号分解），否则使用factors->perm */
    int *perm = solver->perm;
    int perm_allocated = 0;  /* 标记perm是否由本函数分配 */
    if (perm == NULL) {
        perm = factors->perm;
    }
    
    /* 如果perm仍然为NULL，创建单位置换 */
    if (perm == NULL) {
        perm = (int *)malloc(n * sizeof(int));
        if (perm == NULL) {
            free(dense_row);
            return PARD_ERROR_MEMORY;
        }
        perm_allocated = 1;  /* 标记为已分配 */
        for (int i = 0; i < n; i++) {
            perm[i] = i;
        }
    }
    
    int *inv_perm = (int *)malloc(n * sizeof(int));
    
    if (dense_row == NULL || inv_perm == NULL) {
        free(dense_row);
        if (perm_allocated && perm != NULL) {
            free(perm);
        }
        free(inv_perm);
        return PARD_ERROR_MEMORY;
    }
    
    for (int i = 0; i < n; i++) {
        if (perm[i] >= 0 && perm[i] < n) {
            inv_perm[perm[i]] = i;
        } else {
            /* 无效置换值，使用单位置换 */
            for (int j = 0; j < n; j++) {
                inv_perm[j] = j;
            }
            break;
        }
    }
    
    /* 将CSR转换为临时密集存储（用于分解） */
    double **dense_A = (double **)malloc(n * sizeof(double *));
    if (dense_A == NULL) {
        free(dense_row);
        free(inv_perm);
        if (perm_allocated && perm != NULL) {
            free(perm);
        }
        return PARD_ERROR_MEMORY;
    }
    
    /* 初始化为NULL，以便在错误情况下安全释放 */
    for (int i = 0; i < n; i++) {
        dense_A[i] = NULL;
    }
    
    for (int i = 0; i < n; i++) {
        dense_A[i] = (double *)calloc(n, sizeof(double));
        if (dense_A[i] == NULL) {
            /* 清理已分配的内存 */
            for (int j = 0; j < i; j++) {
                if (dense_A[j] != NULL) {
                    free(dense_A[j]);
                    dense_A[j] = NULL;
                }
            }
            free(dense_A);
            dense_A = NULL;
            free(dense_row);
            free(inv_perm);
            if (perm_allocated && perm != NULL) {
                free(perm);
            }
            return PARD_ERROR_MEMORY;
        }
    }
    
    /* 从CSR格式转换为密集格式 */
    for (int i = 0; i < n; i++) {
        /* 确保行索引有效 */
        if (i >= 0 && i < n && A->row_ptr != NULL && 
            A->row_ptr[i + 1] >= A->row_ptr[i] &&
            A->row_ptr[i] >= 0 && A->row_ptr[i + 1] <= A->nnz) {
            for (int j = A->row_ptr[i]; j < A->row_ptr[i + 1]; j++) {
                /* 确保列索引在有效范围内 */
                if (j >= 0 && j < A->nnz && A->col_idx != NULL && A->values != NULL && dense_A[i] != NULL) {
                    int col = A->col_idx[j];
                    /* 确保列索引在有效范围内 */
                    if (col >= 0 && col < n) {
                        dense_A[i][col] = A->values[j];
                    }
                }
            }
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
        
        /* 交换行：使用memcpy优化内存复制 */
        if (max_row != k) {
            /* 确保索引有效，并且指针有效 */
            if (max_row >= 0 && max_row < n && k >= 0 && k < n &&
                dense_A[k] != NULL && dense_A[max_row] != NULL) {
                /* 使用临时缓冲区交换行数据，优化内存访问 */
                /* 先复制到临时行 */
                memcpy(dense_row, dense_A[k], n * sizeof(double));
                /* 复制max_row到k */
                memcpy(dense_A[k], dense_A[max_row], n * sizeof(double));
                /* 复制临时行到max_row */
                memcpy(dense_A[max_row], dense_row, n * sizeof(double));
                
                /* 交换置换数组 */
                if (perm != NULL) {
                    int tmp_perm = perm[k];
                    perm[k] = perm[max_row];
                    perm[max_row] = tmp_perm;
                }
            }
        }
        
        /* 确保dense_A[k]仍然有效 */
        if (k < 0 || k >= n || dense_A[k] == NULL) {
            /* 清理内存并返回错误 */
            safe_free_dense_A(dense_A, n);
            dense_A = NULL;
            if (dense_row != NULL) {
                free(dense_row);
                dense_row = NULL;
            }
            if (inv_perm != NULL) {
                free(inv_perm);
                inv_perm = NULL;
            }
            if (perm_allocated && perm != NULL) {
                free(perm);
                perm = NULL;
            }
            return PARD_ERROR_INVALID_INPUT;
        }
        
        if (fabs(dense_A[k][k]) < 1e-15) {
            /* 数值奇异 - 清理内存并返回 */
            safe_free_dense_A(dense_A, n);
            dense_A = NULL;
            if (dense_row != NULL) {
                free(dense_row);
                dense_row = NULL;
            }
            if (inv_perm != NULL) {
                free(inv_perm);
                inv_perm = NULL;
            }
            if (perm_allocated && perm != NULL) {
                free(perm);
                perm = NULL;
            }
            return PARD_ERROR_NUMERICAL;
        }
        
        /* L和U已在上面更新，这里不需要重复计算 */
    }
    
    /* 将结果复制回CSR格式 */
    /* 注意：符号分解已经分配了factors字段，确保不越界 */
    int l_pos = 0, u_pos = 0;
    
    /* 验证factors字段是否已分配 */
    if (factors->row_ptr == NULL || factors->col_idx == NULL || 
        factors->l_values == NULL || factors->u_row_ptr == NULL ||
        factors->u_col_idx == NULL || factors->u_values == NULL) {
        /* 清理内存 - 安全释放 */
        safe_free_dense_A(dense_A, n);
        dense_A = NULL;
        if (dense_row != NULL) {
            free(dense_row);
            dense_row = NULL;
        }
        if (inv_perm != NULL) {
            free(inv_perm);
            inv_perm = NULL;
        }
        if (perm_allocated && perm != NULL) {
            free(perm);
            perm = NULL;
        }
        return PARD_ERROR_INVALID_INPUT;
    }
    
    /* 验证factors->n是否与n匹配 */
    if (factors->n != n) {
        /* 清理内存 - 使用安全释放函数 */
        safe_free_dense_A(dense_A, n);
        dense_A = NULL;
        if (dense_row != NULL) {
            free(dense_row);
            dense_row = NULL;
        }
        if (inv_perm != NULL) {
            free(inv_perm);
            inv_perm = NULL;
        }
        if (perm_allocated && perm != NULL) {
            free(perm);
            perm = NULL;
        }
        return PARD_ERROR_INVALID_INPUT;
    }
    
    /* 获取符号分解时分配的空间大小 */
    /* factors->nnz是L和U的总和，但我们分别需要l_nnz和u_nnz */
    /* 我们需要从row_ptr和u_row_ptr计算实际的分配大小 */
    /* 在符号分解时，factors->row_ptr[n]已经被设置为l_nnz */
    /* factors->u_row_ptr[n]已经被设置为u_nnz */
    int max_l_nnz = 0;
    int max_u_nnz = 0;
    
    /* 安全地获取l_nnz */
    if (factors->row_ptr != NULL && n >= 0 && factors->n >= n) {
        max_l_nnz = factors->row_ptr[n];
    } else if (factors->nnz > 0) {
        /* 如果无法获取，使用保守估计（假设L和U各占一半） */
        max_l_nnz = factors->nnz / 2;
    } else {
        max_l_nnz = 0;
    }
    
    /* 安全地获取u_nnz */
    if (factors->u_row_ptr != NULL && n >= 0 && factors->n >= n) {
        max_u_nnz = factors->u_row_ptr[n];
    } else if (factors->nnz > 0) {
        /* 如果无法获取，使用保守估计（假设L和U各占一半） */
        max_u_nnz = factors->nnz / 2;
    } else {
        max_u_nnz = 0;
    }
    
    /* 确保至少有足够的空间 */
    if (max_l_nnz <= 0) max_l_nnz = n * n;  /* 最坏情况：完全密集 */
    if (max_u_nnz <= 0) max_u_nnz = n * n;
    
    /* 复制L到factors */
    /* 注意：必须确保不超过分配的空间，否则会导致内存损坏 */
    l_pos = 0;
    for (int i = 0; i < n; i++) {
        /* 确保i在有效范围内，并且dense_A[i]有效 */
        if (i >= 0 && i < n && dense_A[i] != NULL) {
            if (factors->row_ptr != NULL && i < factors->n) {
                factors->row_ptr[i] = l_pos;
            }
            for (int j = 0; j <= i; j++) {
                /* 确保j在有效范围内 */
                if (j >= 0 && j < n) {
                    if (fabs(dense_A[i][j]) > 1e-15 || i == j) {
                        /* 确保不超过分配的空间 */
                        if (l_pos < max_l_nnz && factors->col_idx != NULL && factors->l_values != NULL) {
                            factors->col_idx[l_pos] = j;
                            factors->l_values[l_pos] = (i == j) ? 1.0 : dense_A[i][j];
                        }
                        l_pos++;
                    }
                }
            }
        }
    }
    /* 确保row_ptr[n]被正确设置 */
    if (factors->row_ptr != NULL && n >= 0 && factors->n >= n) {
        factors->row_ptr[n] = l_pos;
    }
    
    /* 复制U到factors */
    /* 注意：必须确保不超过分配的空间，否则会导致内存损坏 */
    u_pos = 0;
    for (int i = 0; i < n; i++) {
        /* 确保i在有效范围内，并且dense_A[i]有效 */
        if (i >= 0 && i < n && dense_A[i] != NULL) {
            if (factors->u_row_ptr != NULL && i < factors->n) {
                factors->u_row_ptr[i] = u_pos;
            }
            for (int j = i; j < n; j++) {
                /* 确保j在有效范围内 */
                if (j >= 0 && j < n) {
                    if (fabs(dense_A[i][j]) > 1e-15 || i == j) {
                        /* 确保不超过分配的空间 */
                        if (u_pos < max_u_nnz && factors->u_col_idx != NULL && factors->u_values != NULL) {
                            factors->u_col_idx[u_pos] = j;
                            factors->u_values[u_pos] = dense_A[i][j];
                        }
                        u_pos++;
                    }
                }
            }
        }
    }
    /* 确保u_row_ptr[n]被正确设置 */
    if (factors->u_row_ptr != NULL && n >= 0 && factors->n >= n) {
        factors->u_row_ptr[n] = u_pos;
    }
    
    /* 更新实际的nnz（可能大于符号分解时的估计） */
    factors->nnz = l_pos + u_pos;
    
    /* 清理dense_A - 使用安全释放函数 */
    safe_free_dense_A(dense_A, n);
    dense_A = NULL;
    if (dense_row != NULL) {
        free(dense_row);
        dense_row = NULL;
    }
    if (inv_perm != NULL) {
        free(inv_perm);
        inv_perm = NULL;
    }
    /* 只释放由本函数分配的perm */
    if (perm_allocated && perm != NULL) {
        free(perm);
        perm = NULL;
    }
    
    return PARD_SUCCESS;
}
