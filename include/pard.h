#ifndef PARD_H
#define PARD_H

#include <stddef.h>
#include <stdint.h>
#include <mpi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 矩阵类型 */
typedef enum {
    PARD_MATRIX_TYPE_REAL_NONSYMMETRIC = 11,  /* 非对称实数矩阵 */
    PARD_MATRIX_TYPE_REAL_SYMMETRIC_POSDEF = 1,  /* 对称正定实数矩阵 */
    PARD_MATRIX_TYPE_REAL_SYMMETRIC_INDEF = -2  /* 对称不定实数矩阵 */
} pard_matrix_type_t;

/* 求解器阶段 */
typedef enum {
    PARD_PHASE_SYMBOLIC = 11,    /* 符号分解 */
    PARD_PHASE_NUMERIC = 22,     /* 数值分解 */
    PARD_PHASE_SOLVE = 33,       /* 求解 */
    PARD_PHASE_CLEANUP = -1      /* 清理 */
} pard_phase_t;

/* CSR矩阵结构 */
typedef struct {
    int n;              /* 矩阵维度 */
    int nnz;            /* 非零元素个数 */
    int *row_ptr;       /* 行指针数组，长度为n+1 */
    int *col_idx;       /* 列索引数组，长度为nnz */
    double *values;     /* 数值数组，长度为nnz */
    int is_symmetric;   /* 是否为对称矩阵 */
    int is_upper;       /* 如果对称，是否只存储上三角 */
} pard_csr_matrix_t;

/* 分解因子结构 */
typedef struct {
    int n;              /* 矩阵维度 */
    int nnz;            /* 非零元素个数 */
    int *row_ptr;       /* L的行指针 */
    int *col_idx;       /* L的列索引 */
    double *l_values;   /* L的数值 */
    
    /* 对于LU分解 */
    int *u_row_ptr;     /* U的行指针 */
    int *u_col_idx;     /* U的列索引 */
    double *u_values;   /* U的数值 */
    int *perm;          /* 行置换数组 */
    
    /* 对于LDL^T分解 */
    double *d_values;   /* D的对角元素 */
    int *pivot_type;    /* 主元类型：1表示1x1，2表示2x2 */
    
    pard_matrix_type_t matrix_type;
} pard_factors_t;

/* 求解器句柄 */
typedef struct {
    pard_csr_matrix_t *matrix;      /* 原始矩阵（已重排序） */
    int *perm;                       /* 行置换数组 */
    int *inv_perm;                   /* 逆置换数组 */
    pard_factors_t *factors;         /* 分解因子 */
    pard_matrix_type_t matrix_type;  /* 矩阵类型 */
    
    /* MPI相关 */
    MPI_Comm comm;                   /* MPI通信器 */
    int mpi_rank;                    /* MPI进程号 */
    int mpi_size;                    /* MPI进程数 */
    int is_parallel;                 /* 是否使用MPI并行 */
    
    /* 统计信息 */
    double analysis_time;            /* 符号分析时间 */
    double factorization_time;       /* 数值分解时间 */
    double solve_time;                /* 求解时间 */
    size_t peak_memory;              /* 峰值内存使用 */
    int fill_in_nnz;                 /* Fill-in后的非零元素数 */
} pard_solver_t;

/* 错误代码 */
typedef enum {
    PARD_SUCCESS = 0,
    PARD_ERROR_INVALID_INPUT = -1,
    PARD_ERROR_MEMORY = -2,
    PARD_ERROR_NUMERICAL = -3,
    PARD_ERROR_MPI = -4
} pard_error_t;

/* 主API函数 */
int pardiso_init(pard_solver_t **solver, pard_matrix_type_t mtype, MPI_Comm comm);
int pardiso_symbolic(pard_solver_t *solver, pard_csr_matrix_t *matrix);
int pardiso_factor(pard_solver_t *solver);
int pardiso_solve(pard_solver_t *solver, int nrhs, double *rhs, double *sol);
int pardiso_refine(pard_solver_t *solver, int nrhs, double *rhs, double *sol, 
                   int max_iter, double tol);
int pardiso_cleanup(pard_solver_t **solver);

/* CSR矩阵操作 */
int pard_csr_create(pard_csr_matrix_t **matrix, int n, int nnz);
int pard_csr_free(pard_csr_matrix_t **matrix);
int pard_csr_copy(pard_csr_matrix_t *dst, const pard_csr_matrix_t *src);
int pard_csr_transpose(pard_csr_matrix_t *dst, const pard_csr_matrix_t *src);
int pard_csr_multiply(pard_csr_matrix_t *C, const pard_csr_matrix_t *A, 
                      const pard_csr_matrix_t *B);

/* 矩阵工具函数 */
int pard_matrix_read_mtx(pard_csr_matrix_t **matrix, const char *filename);
int pard_matrix_write_mtx(const pard_csr_matrix_t *matrix, const char *filename);
int pard_matrix_print_info(const pard_csr_matrix_t *matrix);
int pard_matrix_verify_symmetric(const pard_csr_matrix_t *matrix, double tol);

/* 重排序函数 */
int pard_minimum_degree(const pard_csr_matrix_t *matrix, int **perm, int **inv_perm);
int pard_nested_dissection(const pard_csr_matrix_t *matrix, int **perm, int **inv_perm);
int apply_permutation(pard_csr_matrix_t *matrix, const int *perm, const int *inv_perm);

/* 消元树和符号分解 */
int pard_build_elimination_tree(const pard_csr_matrix_t *matrix,
                                 int **parent, int **first_child, int **next_sibling);
int pard_symbolic_factorization(const pard_csr_matrix_t *matrix,
                                 const int *parent, const int *first_child,
                                 const int *next_sibling, pard_factors_t **factors);

/* 数值分解 */
int pard_lu_factorization(pard_solver_t *solver);
int pard_ldlt_factorization(pard_solver_t *solver);
int pard_cholesky_factorization(pard_solver_t *solver);

/* 求解 */
int pard_solve_system(pard_solver_t *solver, int nrhs, const double *rhs, double *sol);
int pard_forward_substitution(const pard_factors_t *factors, 
                               const double *b, double *y, int nrhs);
int pard_forward_substitution_ldlt(const pard_factors_t *factors,
                                    const double *b, double *y, int nrhs);
int pard_backward_substitution(const pard_factors_t *factors,
                                const double *y, double *x, int nrhs);
int pard_backward_substitution_ldlt(const pard_factors_t *factors,
                                     const double *y, double *x, int nrhs);

/* MPI函数 */
int pard_mpi_distribute_matrix(pard_csr_matrix_t *matrix, MPI_Comm comm,
                                pard_csr_matrix_t **local_matrix);
int pard_mpi_distribute_rhs(const double *rhs, int n, int nrhs,
                             double **local_rhs, MPI_Comm comm);
int pard_mpi_gather_solution(const double *local_sol, int local_n, int n, int nrhs,
                              double *global_sol, MPI_Comm comm);
int pard_mpi_factorization(pard_solver_t *solver);
int pard_mpi_solve(pard_solver_t *solver, int nrhs,
                    const double *local_rhs, double *local_sol);

#ifdef __cplusplus
}
#endif

#endif /* PARD_H */
