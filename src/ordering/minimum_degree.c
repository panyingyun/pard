#include "pard.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/**
 * 近似最小度算法（AMD）
 * 返回置换数组perm，使得perm[i]是原矩阵第i行在新矩阵中的位置
 */
int pard_minimum_degree(const pard_csr_matrix_t *matrix, int **perm, int **inv_perm) {
    if (matrix == NULL || perm == NULL || inv_perm == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int n = matrix->n;
    
    *perm = (int *)malloc(n * sizeof(int));
    *inv_perm = (int *)malloc(n * sizeof(int));
    if (*perm == NULL || *inv_perm == NULL) {
        if (*perm != NULL) free(*perm);
        if (*inv_perm != NULL) free(*inv_perm);
        return PARD_ERROR_MEMORY;
    }
    
    /* 初始化：所有节点都未消除 */
    int *eliminated = (int *)calloc(n, sizeof(int));
    int *degree = (int *)malloc(n * sizeof(int));
    if (eliminated == NULL || degree == NULL) {
        free(*perm);
        free(*inv_perm);
        free(eliminated);
        free(degree);
        return PARD_ERROR_MEMORY;
    }
    
    /* 计算初始度 */
    for (int i = 0; i < n; i++) {
        degree[i] = 0;
        for (int j = matrix->row_ptr[i]; j < matrix->row_ptr[i + 1]; j++) {
            int col = matrix->col_idx[j];
            if (col != i) {
                degree[i]++;
            }
        }
    }
    
    /* 贪心选择：每次选择度最小的节点 */
    int order = 0;
    while (order < n) {
        /* 找到度最小的未消除节点 */
        int min_degree = INT_MAX;
        int min_node = -1;
        
        for (int i = 0; i < n; i++) {
            if (!eliminated[i] && degree[i] < min_degree) {
                min_degree = degree[i];
                min_node = i;
            }
        }
        
        if (min_node == -1) {
            break;  /* 所有节点都已处理 */
        }
        
        /* 将min_node添加到消除顺序 */
        (*perm)[order] = min_node;
        (*inv_perm)[min_node] = order;
        eliminated[min_node] = 1;
        order++;
        
        /* 更新邻居的度（模拟消除min_node后的fill-in） */
        for (int j = matrix->row_ptr[min_node]; j < matrix->row_ptr[min_node + 1]; j++) {
            int neighbor = matrix->col_idx[j];
            if (!eliminated[neighbor]) {
                /* 检查是否需要增加度（由于fill-in） */
                /* 简化版本：假设消除min_node后，其所有邻居之间会形成边 */
                /* 这里使用更简单的启发式：度减1（因为消除了与min_node的边） */
                degree[neighbor]--;
                if (degree[neighbor] < 0) degree[neighbor] = 0;
            }
        }
    }
    
    /* 处理剩余节点 */
    for (int i = 0; i < n; i++) {
        if (!eliminated[i]) {
            (*perm)[order] = i;
            (*inv_perm)[i] = order;
            order++;
        }
    }
    
    free(eliminated);
    free(degree);
    
    return PARD_SUCCESS;
}

/**
 * 应用置换到矩阵
 */
int apply_permutation(pard_csr_matrix_t *matrix, const int *perm, const int *inv_perm) {
    if (matrix == NULL || perm == NULL || inv_perm == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int n = matrix->n;
    
    /* 创建新的CSR矩阵 */
    pard_csr_matrix_t *new_matrix;
    int err = pard_csr_create(&new_matrix, n, matrix->nnz);
    if (err != PARD_SUCCESS) {
        return err;
    }
    
    new_matrix->is_symmetric = matrix->is_symmetric;
    
    /* 计算新矩阵每行的非零元素数 */
    int *row_counts = (int *)calloc(n, sizeof(int));
    if (row_counts == NULL) {
        pard_csr_free(&new_matrix);
        return PARD_ERROR_MEMORY;
    }
    
    for (int i = 0; i < n; i++) {
        int old_row = perm[i];
        row_counts[i] = matrix->row_ptr[old_row + 1] - matrix->row_ptr[old_row];
    }
    
    /* 构建行指针 */
    new_matrix->row_ptr[0] = 0;
    for (int i = 0; i < n; i++) {
        new_matrix->row_ptr[i + 1] = new_matrix->row_ptr[i] + row_counts[i];
    }
    
    /* 填充新矩阵 */
    memset(row_counts, 0, n * sizeof(int));
    
    for (int i = 0; i < n; i++) {
        int old_row = perm[i];
        for (int j = matrix->row_ptr[old_row]; j < matrix->row_ptr[old_row + 1]; j++) {
            int old_col = matrix->col_idx[j];
            int new_col = inv_perm[old_col];
            double val = matrix->values[j];
            
            int pos = new_matrix->row_ptr[i] + row_counts[i];
            new_matrix->col_idx[pos] = new_col;
            new_matrix->values[pos] = val;
            row_counts[i]++;
        }
        
        /* 对每行的列索引排序 */
        int start = new_matrix->row_ptr[i];
        int end = new_matrix->row_ptr[i + 1];
        for (int j = start; j < end - 1; j++) {
            for (int k = j + 1; k < end; k++) {
                if (new_matrix->col_idx[j] > new_matrix->col_idx[k]) {
                    int tmp_idx = new_matrix->col_idx[j];
                    double tmp_val = new_matrix->values[j];
                    new_matrix->col_idx[j] = new_matrix->col_idx[k];
                    new_matrix->values[j] = new_matrix->values[k];
                    new_matrix->col_idx[k] = tmp_idx;
                    new_matrix->values[k] = tmp_val;
                }
            }
        }
    }
    
    free(row_counts);
    
    /* 保存新矩阵的指针（在释放旧指针之前） */
    int *new_row_ptr = new_matrix->row_ptr;
    int *new_col_idx = new_matrix->col_idx;
    double *new_values = new_matrix->values;
    int new_nnz = new_matrix->nnz;
    
    /* 替换原矩阵：安全释放旧指针 */
    if (matrix->row_ptr != NULL) {
        free(matrix->row_ptr);
    }
    if (matrix->col_idx != NULL) {
        free(matrix->col_idx);
    }
    if (matrix->values != NULL) {
        free(matrix->values);
    }
    
    /* 复制新矩阵的指针和数据 */
    matrix->row_ptr = new_row_ptr;
    matrix->col_idx = new_col_idx;
    matrix->values = new_values;
    matrix->nnz = new_nnz;
    
    /* 释放新矩阵结构体本身（不释放其内部指针，因为它们已经复制到原矩阵） */
    new_matrix->row_ptr = NULL;
    new_matrix->col_idx = NULL;
    new_matrix->values = NULL;
    free(new_matrix);
    
    return PARD_SUCCESS;
}
