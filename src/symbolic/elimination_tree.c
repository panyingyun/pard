#include "pard.h"
#include <stdlib.h>
#include <string.h>

/**
 * 消元树节点结构
 */
typedef struct et_node {
    int node_id;
    int parent;
    int num_children;
    int *children;
    int *descendants;  /* 所有后代节点 */
    int num_descendants;
} elimination_tree_node_t;

/**
 * 构建消元树
 * 消元树描述了LU分解过程中变量的消除顺序和依赖关系
 */
int pard_build_elimination_tree(const pard_csr_matrix_t *matrix, 
                                int **parent, int **first_child, int **next_sibling) {
    if (matrix == NULL || parent == NULL || first_child == NULL || next_sibling == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    int n = matrix->n;
    
    *parent = (int *)malloc(n * sizeof(int));
    *first_child = (int *)malloc(n * sizeof(int));
    *next_sibling = (int *)malloc(n * sizeof(int));
    
    if (*parent == NULL || *first_child == NULL || *next_sibling == NULL) {
        if (*parent != NULL) free(*parent);
        if (*first_child != NULL) free(*first_child);
        if (*next_sibling != NULL) free(*next_sibling);
        return PARD_ERROR_MEMORY;
    }
    
    /* 初始化 */
    for (int i = 0; i < n; i++) {
        (*parent)[i] = -1;
        (*first_child)[i] = -1;
        (*next_sibling)[i] = -1;
    }
    
    /* 对于每个节点i，找到其第一个非零列j（j > i） */
    /* 在消元树中，j是i的父节点 */
    for (int i = 0; i < n; i++) {
        int first_nz_col = n;  /* 初始化为最大值 */
        
        for (int j = matrix->row_ptr[i]; j < matrix->row_ptr[i + 1]; j++) {
            int col = matrix->col_idx[j];
            if (col > i && col < first_nz_col) {
                first_nz_col = col;
            }
        }
        
        if (first_nz_col < n) {
            (*parent)[i] = first_nz_col;
            
            /* 将i添加到first_nz_col的子节点列表 */
            if ((*first_child)[first_nz_col] == -1) {
                (*first_child)[first_nz_col] = i;
            } else {
                /* 找到最后一个兄弟节点 */
                int sibling = (*first_child)[first_nz_col];
                while ((*next_sibling)[sibling] != -1) {
                    sibling = (*next_sibling)[sibling];
                }
                (*next_sibling)[sibling] = i;
            }
        }
    }
    
    return PARD_SUCCESS;
}

/**
 * 计算消元树的深度
 */
int elimination_tree_depth(int n, const int *parent) {
    int max_depth = 0;
    
    for (int i = 0; i < n; i++) {
        int depth = 0;
        int node = i;
        while (node != -1 && parent[node] != -1) {
            depth++;
            node = parent[node];
        }
        if (depth > max_depth) {
            max_depth = depth;
        }
    }
    
    return max_depth;
}

/**
 * 获取节点的所有后代
 */
int get_descendants(int node, int n, const int *first_child, const int *next_sibling,
                    int *descendants, int *num_desc) {
    *num_desc = 0;
    
    int child = first_child[node];
    while (child != -1) {
        descendants[(*num_desc)++] = child;
        get_descendants(child, n, first_child, next_sibling, 
                       descendants + *num_desc, num_desc);
        child = next_sibling[child];
    }
    
    return PARD_SUCCESS;
}
