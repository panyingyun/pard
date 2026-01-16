#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

/* 生成矩形稀疏矩阵（Matrix Market格式） */
void generate_rectangular_matrix(int m, int n, double sparsity, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return;
    }
    
    /* 计算目标非零元素数 */
    long target_nnz = (long)(m * n * sparsity);
    if (target_nnz < m * 3) {
        target_nnz = m * 3;  // 至少每行3个非零元素
    }
    
    /* 写入Matrix Market头部 */
    fprintf(fp, "%%MatrixMarket matrix coordinate real general\n");
    fprintf(fp, "%d %d %ld\n", m, n, target_nnz);
    
    srand(42);  // 固定种子以便可重复
    
    int *used = (int *)calloc(m * n, sizeof(int));
    long nnz = 0;
    
    /* 1. 对角线元素（如果m<=n，确保每行至少有一个非零） */
    int diag_count = (m < n) ? m : n;
    for (int i = 0; i < diag_count; i++) {
        fprintf(fp, "%d %d %.6e\n", i + 1, i + 1, (double)(m + n + 1));
        used[i * n + i] = 1;
        nnz++;
    }
    
    /* 2. 上下对角线（如果可能） */
    for (int i = 0; i < m - 1 && i < n - 1; i++) {
        fprintf(fp, "%d %d %.6e\n", i + 1, i + 2, -1.0);
        used[i * n + (i + 1)] = 1;
        nnz++;
        
        fprintf(fp, "%d %d %.6e\n", i + 2, i + 1, -0.5);
        used[(i + 1) * n + i] = 1;
        nnz++;
    }
    
    /* 3. 随机添加更多非零元素 */
    while (nnz < target_nnz) {
        int i = rand() % m;
        int j = rand() % n;
        int idx = i * n + j;
        
        if (!used[idx]) {
            double val = (rand() % 2000 - 1000) / 1000.0;
            fprintf(fp, "%d %d %.6e\n", i + 1, j + 1, val);
            used[idx] = 1;
            nnz++;
        }
        
        /* 防止无限循环 */
        if (nnz >= m * n * 0.8) {
            break;
        }
    }
    
    free(used);
    fclose(fp);
    
    printf("Generated matrix: %s (%dx%d, %ld non-zeros, sparsity: %.4f%%)\n",
           filename, m, n, nnz, (double)nnz / (m * n) * 100);
}

int main(int argc, char **argv) {
    if (argc < 5) {
        printf("Usage: %s <m> <n> <sparsity> <output_file>\n", argv[0]);
        printf("  m: number of rows\n");
        printf("  n: number of columns\n");
        printf("  sparsity: sparsity ratio (e.g., 0.01 for 1%%)\n");
        return 1;
    }
    
    int m = atoi(argv[1]);
    int n = atoi(argv[2]);
    double sparsity = atof(argv[3]);
    const char *filename = argv[4];
    
    if (m <= 0 || n <= 0 || sparsity <= 0 || sparsity > 1) {
        fprintf(stderr, "Invalid parameters\n");
        return 1;
    }
    
    generate_rectangular_matrix(m, n, sparsity, filename);
    return 0;
}
