#!/bin/bash

# 从SuiteSparse Matrix Collection下载测试矩阵
# 这个脚本下载一些常用的测试矩阵用于基准测试

MATRIX_DIR="test_matrices"
mkdir -p "$MATRIX_DIR"

BASE_URL="https://suitesparse-collection-website.herokuapp.com/MM"

# 定义要下载的矩阵列表（对称不定和非对称）
# 选择不同规模的矩阵用于测试
MATRICES=(
    "HB/bcsstk01"           # 对称不定，48x48
    "HB/bcsstk02"           # 对称不定，66x66
    "HB/bcsstk03"           # 对称不定，112x112
    "HB/bcsstk04"           # 对称不定，132x132
    "HB/bcsstk05"           # 对称不定，153x153
    "HB/bcsstk06"           # 对称不定，420x420
    "HB/bcsstk07"           # 对称不定，420x420
    "HB/bcsstk08"           # 对称不定，1074x1074
    "HB/bcsstk09"           # 对称不定，1083x1083
    "HB/bcsstk10"           # 对称不定，1086x1086
    "HB/bcsstk11"           # 对称不定，1473x1473
    "HB/bcsstk12"           # 对称不定，1473x1473
    "HB/bcsstk13"           # 对称不定，2003x2003
    "HB/bcsstk14"           # 对称不定，1806x1806
    "HB/bcsstk15"           # 对称不定，3948x3948
    "HB/bcsstk16"           # 对称不定，4884x4884
    "HB/bcsstk17"           # 对称不定，10974x10974
    "HB/bcsstk18"           # 对称不定，11948x11948
    "HB/bcsstk19"           # 对称不定，8172x8172
    "HB/bcsstk20"           # 对称不定，4851x4851
)

# 下载函数
download_matrix() {
    local matrix=$1
    local filename=$(basename "$matrix").mtx
    local filepath="$MATRIX_DIR/$filename"
    
    if [ -f "$filepath" ]; then
        echo "Skipping $filename (already exists)"
        return 0
    fi
    
    echo "Downloading $matrix..."
    curl -L -o "$filepath" "$BASE_URL/$matrix.tar.gz" 2>/dev/null || {
        echo "Failed to download $matrix"
        return 1
    }
    
    # 解压（如果需要）
    if [ -f "$filepath" ]; then
        tar -xzf "$filepath" -C "$MATRIX_DIR" 2>/dev/null || true
        rm -f "$filepath"
    fi
}

# 下载所有矩阵
echo "Downloading test matrices from SuiteSparse Matrix Collection..."
echo "This may take a while..."

# 下载所有矩阵
for matrix in "${MATRICES[@]}"; do
    download_matrix "$matrix"
done

echo "Download complete. Matrices are in $MATRIX_DIR/"
