#include "pard.h"
#include <stdlib.h>
#include <string.h>

/**
 * 计算节点的度（在邻接图中）
 */
int compute_degree(const pard_csr_matrix_t *matrix, int node, int *marked) {
    int degree = 0;
    for (int j = matrix->row_ptr[node]; j < matrix->row_ptr[node + 1]; j++) {
        int col = matrix->col_idx[j];
        if (col != node && !marked[col]) {
            degree++;
        }
    }
    return degree;
}

/**
 * 构建图的邻接结构（用于重排序）
 */
int build_adjacency_list(const pard_csr_matrix_t *matrix, int **adj_list, int **adj_count) {
    int n = matrix->n;
    
    *adj_count = (int *)calloc(n, sizeof(int));
    if (*adj_count == NULL) {
        return PARD_ERROR_MEMORY;
    }
    
    /* 计算每个节点的邻居数 */
    for (int i = 0; i < n; i++) {
        for (int j = matrix->row_ptr[i]; j < matrix->row_ptr[i + 1]; j++) {
            int col = matrix->col_idx[j];
            if (col != i) {
                (*adj_count)[i]++;
            }
        }
    }
    
    /* 分配邻接表 */
    *adj_list = (int *)malloc(n * sizeof(int));
    if (*adj_list == NULL) {
        free(*adj_count);
        return PARD_ERROR_MEMORY;
    }
    
    int max_degree = 0;
    for (int i = 0; i < n; i++) {
        if ((*adj_count)[i] > max_degree) {
            max_degree = (*adj_count)[i];
        }
    }
    
    int total_size = n * max_degree;
    *adj_list = (int *)realloc(*adj_list, total_size * sizeof(int));
    if (*adj_list == NULL) {
        free(*adj_count);
        return PARD_ERROR_MEMORY;
    }
    
    /* 填充邻接表 */
    int *pos = (int *)calloc(n, sizeof(int));
    for (int i = 0; i < n; i++) {
        for (int j = matrix->row_ptr[i]; j < matrix->row_ptr[i + 1]; j++) {
            int col = matrix->col_idx[j];
            if (col != i) {
                (*adj_list)[i * max_degree + pos[i]] = col;
                pos[i]++;
            }
        }
    }
    
    free(pos);
    return PARD_SUCCESS;
}
