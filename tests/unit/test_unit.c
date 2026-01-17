#include "pard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <mpi.h>

/* 测试CSR矩阵创建和释放 */
void test_csr_create_free() {
    pard_csr_matrix_t *matrix = NULL;
    int err = pard_csr_create(&matrix, 10, 20);
    assert(err == PARD_SUCCESS);
    assert(matrix != NULL);
    assert(matrix->n == 10);
    assert(matrix->nnz == 20);
    
    err = pard_csr_free(&matrix);
    assert(err == PARD_SUCCESS);
    assert(matrix == NULL);
    
    printf("test_csr_create_free: PASSED\n");
}

/* 测试矩阵读取（需要测试矩阵文件） */
void test_matrix_read() {
    /* 使用基准测试目录中的测试矩阵 */
    /* 从项目根目录或build目录都能找到的路径 */
    const char *test_matrix = "tests/benchmark/test_matrices/test_simple.mtx";
    char abs_path[1024];
    
    /* 尝试多个可能的路径 */
    FILE *test_file = NULL;
    strcpy(abs_path, "");  /* 初始化为空 */
    
    /* 首先尝试从build/bin目录查找 */
    const char *alt_path = "../../tests/benchmark/test_matrices/test_simple.mtx";
    printf("  Trying path: %s\n", alt_path);
    if ((test_file = fopen(alt_path, "r")) != NULL) {
        fclose(test_file);
        strcpy(abs_path, alt_path);
        printf("  Found file at: %s\n", abs_path);
    } else {
        printf("  Path %s not found, trying: %s\n", alt_path, test_matrix);
        /* 尝试相对路径（从项目根目录） */
        if ((test_file = fopen(test_matrix, "r")) != NULL) {
            fclose(test_file);
            strcpy(abs_path, test_matrix);
            printf("  Found file at: %s\n", abs_path);
        } else {
            /* 尝试绝对路径（从项目根目录） */
            const char *abs_alt = "/home/yypan/panyingyun/pard/tests/benchmark/test_matrices/test_simple.mtx";
            printf("  Trying absolute path: %s\n", abs_alt);
            if ((test_file = fopen(abs_alt, "r")) != NULL) {
                fclose(test_file);
                strcpy(abs_path, abs_alt);
                printf("  Found file at: %s\n", abs_path);
            } else {
                printf("test_matrix_read: FAILED (cannot find test matrix file, tried: %s, %s, %s)\n", 
                       alt_path, test_matrix, abs_alt);
                return;
            }
        }
    }
    
    pard_csr_matrix_t *matrix = NULL;
    printf("  Attempting to read matrix from: %s\n", abs_path);
    int err = pard_matrix_read_mtx(&matrix, abs_path);
    
    if (err != PARD_SUCCESS) {
        printf("test_matrix_read: FAILED (error code: %d, file: %s)\n", err, abs_path);
        /* 验证文件是否真的存在且可读 */
        FILE *test = fopen(abs_path, "r");
        if (test == NULL) {
            printf("  File cannot be opened: %s\n", abs_path);
        } else {
            printf("  File exists and can be opened, but matrix read failed\n");
            char buf[256];
            if (fgets(buf, sizeof(buf), test) != NULL) {
                printf("  First line of file: %s", buf);
            }
            fclose(test);
        }
        return;
    }
    
    if (matrix == NULL) {
        printf("test_matrix_read: FAILED (matrix is NULL)\n");
        return;
    }
    
    /* 验证矩阵基本信息 */
    if (matrix->n != 10) {
        printf("test_matrix_read: FAILED (wrong dimension: %d, expected 10)\n", matrix->n);
        pard_csr_free(&matrix);
        return;
    }
    
    if (matrix->nnz < 20 || matrix->nnz > 30) {
        printf("test_matrix_read: FAILED (wrong nnz: %d, expected ~28)\n", matrix->nnz);
        pard_csr_free(&matrix);
        return;
    }
    
    /* 验证矩阵数据完整性 */
    if (matrix->row_ptr == NULL || matrix->col_idx == NULL || matrix->values == NULL) {
        printf("test_matrix_read: FAILED (null pointers)\n");
        pard_csr_free(&matrix);
        return;
    }
    
    /* 验证行指针的有效性 */
    if (matrix->row_ptr[0] != 0) {
        printf("test_matrix_read: FAILED (row_ptr[0] should be 0)\n");
        pard_csr_free(&matrix);
        return;
    }
    
    if (matrix->row_ptr[matrix->n] != matrix->nnz) {
        printf("test_matrix_read: FAILED (row_ptr[n] should equal nnz)\n");
        pard_csr_free(&matrix);
        return;
    }
    
    pard_csr_free(&matrix);
    printf("test_matrix_read: PASSED\n");
}

/* 测试重排序 */
void test_ordering() {
    /* 创建一个小测试矩阵 */
    pard_csr_matrix_t *matrix = NULL;
    int n = 5;
    int nnz = 10;
    int err = pard_csr_create(&matrix, n, nnz);
    assert(err == PARD_SUCCESS);
    
    /* 填充一个简单的矩阵 */
    matrix->row_ptr[0] = 0;
    matrix->row_ptr[1] = 2;
    matrix->row_ptr[2] = 4;
    matrix->row_ptr[3] = 6;
    matrix->row_ptr[4] = 8;
    matrix->row_ptr[5] = 10;
    
    /* 简单的5x5矩阵 */
    int cols[] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 0};
    double vals[] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    
    for (int i = 0; i < nnz; i++) {
        matrix->col_idx[i] = cols[i];
        matrix->values[i] = vals[i];
    }
    
    int *perm = NULL, *inv_perm = NULL;
    err = pard_minimum_degree(matrix, &perm, &inv_perm);
    assert(err == PARD_SUCCESS);
    assert(perm != NULL);
    assert(inv_perm != NULL);
    
    free(perm);
    free(inv_perm);
    pard_csr_free(&matrix);
    
    printf("test_ordering: PASSED\n");
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (rank == 0) {
        printf("Running unit tests...\n\n");
        
        test_csr_create_free();
        test_matrix_read();
        test_ordering();
        
        printf("\nAll unit tests completed.\n");
    }
    
    MPI_Finalize();
    return 0;
}
