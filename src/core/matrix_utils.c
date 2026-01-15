#include "pard.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/**
 * 读取Matrix Market格式文件
 */
int pard_matrix_read_mtx(pard_csr_matrix_t **matrix, const char *filename) {
    if (matrix == NULL || filename == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    char line[1024];
    int nrows, ncols, nnz;
    int symmetric = 0;
    
    /* 读取头部 */
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '%') {
            if (strstr(line, "symmetric") != NULL || strstr(line, "Hermitian") != NULL) {
                symmetric = 1;
            }
            continue;
        }
        
        if (sscanf(line, "%d %d %d", &nrows, &ncols, &nnz) == 3) {
            break;
        }
    }
    
    if (nrows != ncols) {
        fclose(fp);
        return PARD_ERROR_INVALID_INPUT;  /* 只支持方阵 */
    }
    
    int n = nrows;
    int actual_nnz = symmetric ? (nnz * 2 - n) : nnz;  /* 对称矩阵需要存储对角和上三角 */
    
    /* 创建矩阵 */
    int err = pard_csr_create(matrix, n, actual_nnz);
    if (err != PARD_SUCCESS) {
        fclose(fp);
        return err;
    }
    
    (*matrix)->is_symmetric = symmetric;
    
    /* 临时存储所有非零元素 */
    typedef struct {
        int row, col;
        double val;
    } coo_entry_t;
    
    coo_entry_t *entries = (coo_entry_t *)malloc(nnz * sizeof(coo_entry_t));
    if (entries == NULL) {
        pard_csr_free(matrix);
        fclose(fp);
        return PARD_ERROR_MEMORY;
    }
    
    int count = 0;
    while (fgets(line, sizeof(line), fp) && count < nnz) {
        int row, col;
        double val;
        if (sscanf(line, "%d %d %lf", &row, &col, &val) == 3) {
            entries[count].row = row - 1;  /* Matrix Market是1-based */
            entries[count].col = col - 1;
            entries[count].val = val;
            count++;
            
            /* 如果是对称矩阵且不在对角线上，添加对称元素 */
            if (symmetric && row != col && count < nnz) {
                entries[count].row = col - 1;
                entries[count].col = row - 1;
                entries[count].val = val;
                count++;
            }
        }
    }
    
    fclose(fp);
    
    /* 按行排序 */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (entries[i].row > entries[j].row || 
                (entries[i].row == entries[j].row && entries[i].col > entries[j].col)) {
                coo_entry_t tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }
    
    /* 转换为CSR格式 */
    int *row_counts = (int *)calloc(n, sizeof(int));
    if (row_counts == NULL) {
        free(entries);
        pard_csr_free(matrix);
        return PARD_ERROR_MEMORY;
    }
    
    for (int i = 0; i < count; i++) {
        row_counts[entries[i].row]++;
    }
    
    (*matrix)->row_ptr[0] = 0;
    for (int i = 0; i < n; i++) {
        (*matrix)->row_ptr[i + 1] = (*matrix)->row_ptr[i] + row_counts[i];
    }
    
    memset(row_counts, 0, n * sizeof(int));
    
    for (int i = 0; i < count; i++) {
        int row = entries[i].row;
        int pos = (*matrix)->row_ptr[row] + row_counts[row];
        (*matrix)->col_idx[pos] = entries[i].col;
        (*matrix)->values[pos] = entries[i].val;
        row_counts[row]++;
    }
    
    free(entries);
    free(row_counts);
    
    return PARD_SUCCESS;
}

/**
 * 写入Matrix Market格式文件
 */
int pard_matrix_write_mtx(const pard_csr_matrix_t *matrix, const char *filename) {
    if (matrix == NULL || filename == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    fprintf(fp, "%%MatrixMarket matrix coordinate real general\n");
    fprintf(fp, "%d %d %d\n", matrix->n, matrix->n, matrix->nnz);
    
    for (int i = 0; i < matrix->n; i++) {
        for (int j = matrix->row_ptr[i]; j < matrix->row_ptr[i + 1]; j++) {
            fprintf(fp, "%d %d %.17e\n", i + 1, matrix->col_idx[j] + 1, 
                    matrix->values[j]);
        }
    }
    
    fclose(fp);
    return PARD_SUCCESS;
}

/**
 * 打印矩阵信息
 */
int pard_matrix_print_info(const pard_csr_matrix_t *matrix) {
    if (matrix == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    printf("Matrix Information:\n");
    printf("  Dimension: %d x %d\n", matrix->n, matrix->n);
    printf("  Non-zeros: %d\n", matrix->nnz);
    printf("  Density: %.6f%%\n", 100.0 * matrix->nnz / (matrix->n * matrix->n));
    printf("  Symmetric: %s\n", matrix->is_symmetric ? "Yes" : "No");
    
    /* 计算每行平均非零元素数 */
    double avg_nnz_per_row = (double)matrix->nnz / matrix->n;
    printf("  Avg non-zeros per row: %.2f\n", avg_nnz_per_row);
    
    return PARD_SUCCESS;
}

/**
 * 验证矩阵是否对称
 */
int pard_matrix_verify_symmetric(const pard_csr_matrix_t *matrix, double tol) {
    if (matrix == NULL) {
        return PARD_ERROR_INVALID_INPUT;
    }
    
    if (!matrix->is_symmetric) {
        return 0;  /* 标记为非对称 */
    }
    
    /* 检查A[i][j] == A[j][i] */
    for (int i = 0; i < matrix->n; i++) {
        for (int j = matrix->row_ptr[i]; j < matrix->row_ptr[i + 1]; j++) {
            int col = matrix->col_idx[j];
            double val_ij = matrix->values[j];
            
            /* 查找A[col][i] */
            int found = 0;
            for (int k = matrix->row_ptr[col]; k < matrix->row_ptr[col + 1]; k++) {
                if (matrix->col_idx[k] == i) {
                    double val_ji = matrix->values[k];
                    if (fabs(val_ij - val_ji) > tol) {
                        return 0;  /* 不对称 */
                    }
                    found = 1;
                    break;
                }
            }
            
            if (!found && fabs(val_ij) > tol) {
                return 0;  /* 缺少对称元素 */
            }
        }
    }
    
    return 1;  /* 对称 */
}
