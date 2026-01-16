cmake_minimum_required(VERSION 3.10)
project(pard VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找MPI
find_package(MPI REQUIRED COMPONENTS C)

# 包含目录
include_directories(include)

# 源文件
set(CORE_SOURCES
    src/core/csr_matrix.c
    src/core/matrix_utils.c
)

set(ORDERING_SOURCES
    src/ordering/minimum_degree.c
    src/ordering/nested_dissection.c
    src/ordering/ordering_utils.c
)

set(SYMBOLIC_SOURCES
    src/symbolic/elimination_tree.c
    src/symbolic/symbolic_factor.c
)

set(FACTORIZATION_SOURCES
    src/factorization/lu_factor.c
    src/factorization/cholesky_factor.c
    src/factorization/ldlt_factor.c
)

set(SOLVE_SOURCES
    src/solve/forward_sub.c
    src/solve/backward_sub.c
    src/solve/solve.c
)

set(REFINEMENT_SOURCES
    src/refinement/iterative_refinement.c
)

set(MPI_SOURCES
    src/mpi/mpi_distribute.c
    src/mpi/mpi_factor.c
    src/mpi/mpi_solve.c
)

set(ALL_SOURCES
    ${CORE_SOURCES}
    ${ORDERING_SOURCES}
    ${SYMBOLIC_SOURCES}
    ${FACTORIZATION_SOURCES}
    ${SOLVE_SOURCES}
    ${REFINEMENT_SOURCES}
    ${MPI_SOURCES}
    src/pard.c
)

# 创建库
add_library(pard STATIC ${ALL_SOURCES})
target_link_libraries(pard PUBLIC MPI::MPI_C m)

# 编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(pard PRIVATE -O3 -march=native)
endif()

# 示例程序
add_executable(pard_example examples/example.c)
target_link_libraries(pard_example pard)

# 测试程序（如果启用）
option(BUILD_TESTS "Build test programs" ON)
if(BUILD_TESTS)
    enable_testing()
    
    # 单元测试
    add_executable(test_unit tests/unit/test_unit.c)
    target_link_libraries(test_unit pard)
    
    # 集成测试
    add_executable(test_integration tests/integration/test_integration.c)
    target_link_libraries(test_integration pard)
    
    # 基准测试
    add_executable(benchmark tests/benchmark/benchmark.c)
    target_link_libraries(benchmark pard)
    
    # MUMPS对比程序
    add_executable(mumps_compare tests/benchmark/mumps_compare.c)
    target_link_libraries(mumps_compare pard)
    
    # 矩阵生成工具
    add_executable(create_matrix tests/benchmark/create_matrix.c)
    add_executable(create_rectangular_matrix tests/benchmark/create_rectangular_matrix.c)
    
    # MUMPS基准测试
    find_library(MUMPS_LIB dmumps PATHS /usr/lib /usr/local/lib)
    if(MUMPS_LIB)
        add_executable(mumps_benchmark tests/benchmark/mumps_benchmark.c)
        target_link_libraries(mumps_benchmark ${MUMPS_LIB} MPI::MPI_C m)
        message(STATUS "MUMPS library found: ${MUMPS_LIB}")
    else()
        message(WARNING "MUMPS library not found, mumps_benchmark will not be built")
    endif()
endif()
