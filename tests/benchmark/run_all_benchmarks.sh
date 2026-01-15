#!/bin/bash

# 运行所有基准测试的脚本

echo "=========================================="
echo "PARD 性能基准测试套件"
echo "=========================================="
echo ""

BENCHMARK_DIR="tests/benchmark"
MATRIX_DIR="$BENCHMARK_DIR/test_matrices"
BIN_DIR="build/bin"

# 检查可执行文件是否存在
if [ ! -f "$BIN_DIR/benchmark" ]; then
    echo "错误: 基准测试程序未找到，请先编译项目"
    exit 1
fi

echo "测试矩阵目录: $MATRIX_DIR"
echo ""

# 测试矩阵列表
declare -a matrices=(
    "test_simple.mtx:11:Non-symmetric small"
    "test_sym_100.mtx:-2:Symmetric indefinite medium"
)

# 运行每个测试
for test_case in "${matrices[@]}"; do
    IFS=':' read -r matrix_file mtype description <<< "$test_case"
    matrix_path="$MATRIX_DIR/$matrix_file"
    
    if [ ! -f "$matrix_path" ]; then
        echo "跳过: $matrix_file (文件不存在)"
        continue
    fi
    
    echo "----------------------------------------"
    echo "测试: $description"
    echo "矩阵: $matrix_file"
    echo "类型: $mtype"
    echo "----------------------------------------"
    
    $BIN_DIR/benchmark "$matrix_path" "$mtype" 0 2>&1 | grep -E "(Matrix Information|Analysis time|Factorization time|Solve time|Total time|Fill-in|Max residual)" || echo "测试失败"
    
    echo ""
done

echo "=========================================="
echo "所有基准测试完成"
echo "=========================================="
