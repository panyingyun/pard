#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mpi.h>
#include <dmumps_c.h>

/* 读取Matrix Market格式矩阵 */
int read_matrix_market(const char *filename, int *m, int *n, int *nnz,
                       int **irn, int **jcn, double **a) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return 1;
    }
    
    char line[1024];
    int header_read = 0;
    int count = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '%') {
            continue;  // 跳过注释
        }
        
        if (!header_read) {
            sscanf(line, "%d %d %d", m, n, nnz);
            header_read = 1;
            
            *irn = (int *)malloc(*nnz * sizeof(int));
            *jcn = (int *)malloc(*nnz * sizeof(int));
            *a = (double *)malloc(*nnz * sizeof(double));
            
            if (!*irn || !*jcn || !*a) {
                fclose(fp);
                return 1;
            }
        } else {
            int i, j;
            double val;
            sscanf(line, "%d %d %lf", &i, &j, &val);
            (*irn)[count] = i;
            (*jcn)[count] = j;
            (*a)[count] = val;
            count++;
            
            if (count >= *nnz) {
                break;
            }
        }
    }
    
    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (argc < 2) {
        if (rank == 0) {
            printf("Usage: %s <matrix_file.mtx>\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }
    
    const char *matrix_file = argv[1];
    
    if (rank == 0) {
        printf("=== MUMPS Benchmark ===\n");
        printf("Matrix file: %s\n", matrix_file);
        printf("MPI processes: %d\n\n", size);
    }
    
    // 读取矩阵（只在rank 0）
    int m, n, nnz;
    int *irn = NULL, *jcn = NULL;
    double *a = NULL;
    
    if (rank == 0) {
        if (read_matrix_market(matrix_file, &m, &n, &nnz, &irn, &jcn, &a) != 0) {
            printf("Error reading matrix file\n");
            MPI_Finalize();
            return 1;
        }
        
        printf("Matrix: %s\n", matrix_file);
        printf("  Dimension: %d x %d\n", m, n);
        printf("  Non-zeros: %d\n", nnz);
    }
    
    // 广播矩阵信息
    MPI_Bcast(&m, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&nnz, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // 初始化MUMPS
    DMUMPS_STRUC_C id;
    id.job = -1;
    id.par = 1;
    id.sym = 0;  // 非对称
    id.comm_fortran = MPI_Comm_c2f(MPI_COMM_WORLD);
    dmumps_c(&id);
    
    // 设置矩阵
    if (rank == 0) {
        id.n = n;
        id.nz = nnz;
        id.irn = irn;
        id.jcn = jcn;
        id.a = a;
    }
    
    // 符号分析
    clock_t start = clock();
    id.icntl[1-1] = 0;  // 错误输出
    id.icntl[2-1] = 0;  // 诊断输出
    id.icntl[3-1] = 0;  // 全局信息
    id.icntl[4-1] = 2;  // 错误级别
    
    id.job = 1;  // 符号分析
    dmumps_c(&id);
    clock_t end = clock();
    double analysis_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (id.info[1-1] < 0) {
        if (rank == 0) {
            printf("Error in symbolic analysis: %d\n", id.info[1-1]);
        }
        if (rank == 0) {
            free(irn);
            free(jcn);
            free(a);
        }
        id.job = -2;
        dmumps_c(&id);
        MPI_Finalize();
        return 1;
    }
    
    // 数值分解
    start = clock();
    id.job = 2;  // 数值分解
    dmumps_c(&id);
    end = clock();
    double factor_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (id.info[1-1] < 0) {
        if (rank == 0) {
            printf("Error in factorization: %d\n", id.info[1-1]);
        }
        if (rank == 0) {
            free(irn);
            free(jcn);
            free(a);
        }
        id.job = -2;
        dmumps_c(&id);
        MPI_Finalize();
        return 1;
    }
    
    // 创建右端项并求解
    double *rhs = NULL;
    double *sol = NULL;
    
    if (rank == 0) {
        rhs = (double *)malloc(n * sizeof(double));
        sol = (double *)malloc(n * sizeof(double));
        for (int i = 0; i < n; i++) {
            rhs[i] = 1.0;
        }
        
        id.rhs = rhs;
        id.nrhs = 1;
        id.lrhs = n;
    }
    
    start = clock();
    id.job = 3;  // 求解
    dmumps_c(&id);
    end = clock();
    double solve_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (rank == 0) {
        if (id.info[1-1] < 0) {
            printf("Error in solve: %d\n", id.info[1-1]);
        } else {
            sol = id.rhs;  // MUMPS将解放在rhs中
            
            printf("\nPerformance Statistics:\n");
            printf("  Analysis time:      %.6f seconds\n", analysis_time);
            printf("  Factorization time: %.6f seconds\n", factor_time);
            printf("  Solve time:         %.6f seconds\n", solve_time);
            printf("  Total time:         %.6f seconds\n", 
                   analysis_time + factor_time + solve_time);
        }
    }
    
    // 清理
    if (rank == 0) {
        free(irn);
        free(jcn);
        free(a);
        if (rhs && rhs != sol) {
            free(rhs);
        }
    }
    
    id.job = -2;
    dmumps_c(&id);
    
    MPI_Finalize();
    return 0;
}
