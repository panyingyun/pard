#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

/* 生成稀疏矩阵（Matrix Market格式） */
void generate_sparse_matrix(int n, double sparsity, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return;
    }
    
    /* 计算目标非零元素数 */
    long target_nnz = (long)(n * n * sparsity);
    if (target_nnz < n * 3) {
        target_nnz = n * 3;  // 至少三对角
    }
    
    /* 写入Matrix Market头部 */
    fprintf(fp, "%%MatrixMarket matrix coordinate real general\n");
    fprintf(fp, "%d %d %ld\n", n, n, target_nnz);
    
    srand(42);  // 固定种子以便可重复
    
    int *used = (int *)calloc(n * n, sizeof(int));
    long nnz = 0;
    
    /* 1. 对角线元素（确保非奇异） */
    for (int i = 0; i < n; i++) {
        fprintf(fp, "%d %d %.6e\n", i + 1, i + 1, (double)(n + 1));
        used[i * n + i] = 1;
        nnz++;
    }
    
    /* 2. 上下对角线 */
    for (int i = 0; i < n - 1; i++) {
        fprintf(fp, "%d %d %.6e\n", i + 1, i + 2, -1.0);
        used[i * n + (i + 1)] = 1;
        nnz++;
        
        fprintf(fp, "%d %d %.6e\n", i + 2, i + 1, -0.5);
        used[(i + 1) * n + i] = 1;
        nnz++;
    }
    
    /* 3. 随机添加更多非零元素 */
    while (nnz < target_nnz) {
        int i = rand() % n;
        int j = rand() % n;
        int idx = i * n + j;
        
        if (!used[idx]) {
            double val = (rand() % 2000 - 1000) / 1000.0;
            fprintf(fp, "%d %d %.6e\n", i + 1, j + 1, val);
            used[idx] = 1;
            nnz++;
        }
        
        /* 防止无限循环 */
        if (nnz >= n * n * 0.8) {
            break;
        }
    }
    
    free(used);
    fclose(fp);
    
    printf("Generated matrix: %s (%dx%d, %ld non-zeros, sparsity: %.4f%%)\n",
           filename, n, n, nnz, (double)nnz / (n * n) * 100);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s <n> <sparsity> <output_file>\n", argv[0]);
        printf("  n: matrix dimension\n");
        printf("  sparsity: sparsity ratio (e.g., 0.01 for 1%%)\n");
        return 1;
    }
    
    int n = atoi(argv[1]);
    double sparsity = atof(argv[2]);
    const char *filename = argv[3];
    
    if (n <= 0 || sparsity <= 0 || sparsity > 1) {
        fprintf(stderr, "Invalid parameters\n");
        return 1;
    }
    
    generate_sparse_matrix(n, sparsity, filename);
    return 0;
}
