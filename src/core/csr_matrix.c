#include "pard.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * 创建CSR矩阵
 */
int pard_csr_create(pard_csr_matrix_t **matrix, int n, int nnz) {
    if (matrix == NULL || n <= 0 || nnz < 0) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    *matrix = (pard_csr_matrix_t *)calloc(1, sizeof(pard_csr_matrix_t));
    if (*matrix == NULL) {
        return PARD_ERROR_MEMORY;
    }
    
    (*matrix)->n = n;
    (*matrix)->nnz = nnz;
    (*matrix)->is_symmetric = 0;
    (*matrix)->is_upper = 0;
    
    if (nnz > 0) {
        (*matrix)->row_ptr = (int *)calloc(n + 1, sizeof(int));
        (*matrix)->col_idx = (int *)calloc(nnz, sizeof(int));
        (*matrix)->values = (double *)calloc(nnz, sizeof(double));
        
        if ((*matrix)->row_ptr == NULL || 
            (*matrix)->col_idx == NULL || 
            (*matrix)->values == NULL) {
            pard_csr_free(matrix);
            return PARD_ERROR_MEMORY;
        }
    } else {
        (*matrix)->row_ptr = (int *)calloc(n + 1, sizeof(int));
        (*matrix)->col_idx = NULL;
        (*matrix)->values = NULL;
    }
    
    return PARD_SUCCESS;
}

/**
 * 释放CSR矩阵
 */
int pard_csr_free(pard_csr_matrix_t **matrix) {
    if (matrix == NULL || *matrix == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    pard_csr_matrix_t *m = *matrix;
    
    /* 安全释放各个字段，避免重复释放 */
    if (m->row_ptr != NULL) {
        free(m->row_ptr);
        m->row_ptr = NULL;
    }
    if (m->col_idx != NULL) {
        free(m->col_idx);
        m->col_idx = NULL;
    }
    if (m->values != NULL) {
        free(m->values);
        m->values = NULL;
    }
    
    /* 释放矩阵结构体本身 */
    free(m);
    *matrix = NULL;
    
    return PARD_SUCCESS;
}

/**
 * 复制CSR矩阵
 */
int pard_csr_copy(pard_csr_matrix_t *dst, const pard_csr_matrix_t *src) {
    if (dst == NULL || src == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    if (dst->n != src->n || dst->nnz != src->nnz) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    memcpy(dst->row_ptr, src->row_ptr, (src->n + 1) * sizeof(int));
    
    if (src->nnz > 0) {
        memcpy(dst->col_idx, src->col_idx, src->nnz * sizeof(int));
        memcpy(dst->values, src->values, src->nnz * sizeof(double));
    }
    
    dst->is_symmetric = src->is_symmetric;
    dst->is_upper = src->is_upper;
    
    return PARD_SUCCESS;
}

/**
 * 转置CSR矩阵
 */
int pard_csr_transpose(pard_csr_matrix_t *dst, const pard_csr_matrix_t *src) {
    if (dst == NULL || src == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int n = src->n;
    int nnz = src->nnz;
    
    /* 分配转置矩阵的空间 */
    if (dst->n != n || dst->nnz != nnz) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    /* 计算每行的非零元素个数 */
    int *row_counts = (int *)calloc(n, sizeof(int));
    if (row_counts == NULL) {
        return PARD_ERROR_MEMORY;
    }
    
    for (int i = 0; i < nnz; i++) {
        row_counts[src->col_idx[i]]++;
    }
    
    /* 构建行指针 */
    dst->row_ptr[0] = 0;
    for (int i = 0; i < n; i++) {
        dst->row_ptr[i + 1] = dst->row_ptr[i] + row_counts[i];
    }
    
    /* 重置计数器用于填充 */
    memset(row_counts, 0, n * sizeof(int));
    
    /* 填充转置矩阵 */
    for (int i = 0; i < n; i++) {
        for (int j = src->row_ptr[i]; j < src->row_ptr[i + 1]; j++) {
            int col = src->col_idx[j];
            int pos = dst->row_ptr[col] + row_counts[col];
            dst->col_idx[pos] = i;
            dst->values[pos] = src->values[j];
            row_counts[col]++;
        }
    }
    
    free(row_counts);
    dst->is_symmetric = src->is_symmetric;
    dst->is_upper = !src->is_upper;  /* 转置后上三角变下三角 */
    
    return PARD_SUCCESS;
}

/**
 * 矩阵乘法 C = A * B
 * 注意：这是一个简化版本，假设结果矩阵已正确分配
 */
int pard_csr_multiply(pard_csr_matrix_t *C, const pard_csr_matrix_t *A, 
                      const pard_csr_matrix_t *B) {
    if (C == NULL || A == NULL || B == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    if (A->n != B->n || C->n != A->n) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int n = A->n;
    double *temp = (double *)calloc(n, sizeof(double));
    if (temp == NULL) {
        return PARD_ERROR_MEMORY;
    }
    
    int nnz = 0;
    C->row_ptr[0] = 0;
    
    for (int i = 0; i < n; i++) {
        /* 清零临时数组 */
        memset(temp, 0, n * sizeof(double));
        
        /* 计算第i行 */
        for (int j = A->row_ptr[i]; j < A->row_ptr[i + 1]; j++) {
            int k = A->col_idx[j];
            double a_val = A->values[j];
            
            for (int l = B->row_ptr[k]; l < B->row_ptr[k + 1]; l++) {
                int col = B->col_idx[l];
                temp[col] += a_val * B->values[l];
            }
        }
        
        /* 计算非零元素个数 */
        for (int j = 0; j < n; j++) {
            if (temp[j] != 0.0) {
                nnz++;
            }
        }
    }
    
    /* 重新分配C矩阵以容纳结果 */
    if (C->nnz < nnz) {
        if (C->col_idx != NULL) free(C->col_idx);
        if (C->values != NULL) free(C->values);
        C->col_idx = (int *)malloc(nnz * sizeof(int));
        C->values = (double *)malloc(nnz * sizeof(double));
        if (C->col_idx == NULL || C->values == NULL) {
            free(temp);
            return PARD_ERROR_MEMORY;
        }
        C->nnz = nnz;
    }
    
    /* 重新计算并填充 */
    nnz = 0;
    C->row_ptr[0] = 0;
    
    for (int i = 0; i < n; i++) {
        memset(temp, 0, n * sizeof(double));
        
        for (int j = A->row_ptr[i]; j < A->row_ptr[i + 1]; j++) {
            int k = A->col_idx[j];
            double a_val = A->values[j];
            
            for (int l = B->row_ptr[k]; l < B->row_ptr[k + 1]; l++) {
                int col = B->col_idx[l];
                temp[col] += a_val * B->values[l];
            }
        }
        
        for (int j = 0; j < n; j++) {
            if (temp[j] != 0.0) {
                C->col_idx[nnz] = j;
                C->values[nnz] = temp[j];
                nnz++;
            }
        }
        C->row_ptr[i + 1] = nnz;
    }
    
    free(temp);
    return PARD_SUCCESS;
}
