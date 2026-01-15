CC = mpicc
CFLAGS = -Wall -Wextra -std=c11 -O3 -march=native
INCLUDES = -Iinclude
LDFLAGS = -lm

# 目录
SRC_DIR = src
OBJ_DIR = build/obj
BIN_DIR = build/bin

# 源文件
CORE_SRCS = $(SRC_DIR)/core/csr_matrix.c $(SRC_DIR)/core/matrix_utils.c
ORDERING_SRCS = $(SRC_DIR)/ordering/minimum_degree.c $(SRC_DIR)/ordering/nested_dissection.c $(SRC_DIR)/ordering/ordering_utils.c
SYMBOLIC_SRCS = $(SRC_DIR)/symbolic/elimination_tree.c $(SRC_DIR)/symbolic/symbolic_factor.c
FACTORIZATION_SRCS = $(SRC_DIR)/factorization/lu_factor.c $(SRC_DIR)/factorization/cholesky_factor.c $(SRC_DIR)/factorization/ldlt_factor.c
SOLVE_SRCS = $(SRC_DIR)/solve/forward_sub.c $(SRC_DIR)/solve/backward_sub.c $(SRC_DIR)/solve/solve.c
REFINEMENT_SRCS = $(SRC_DIR)/refinement/iterative_refinement.c
MPI_SRCS = $(SRC_DIR)/mpi/mpi_distribute.c $(SRC_DIR)/mpi/mpi_factor.c $(SRC_DIR)/mpi/mpi_solve.c
MAIN_SRC = $(SRC_DIR)/pard.c

ALL_SRCS = $(CORE_SRCS) $(ORDERING_SRCS) $(SYMBOLIC_SRCS) $(FACTORIZATION_SRCS) \
           $(SOLVE_SRCS) $(REFINEMENT_SRCS) $(MPI_SRCS) $(MAIN_SRC)

# 目标文件
OBJS = $(ALL_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# 库文件
LIB = $(BIN_DIR)/libpard.a

# 可执行文件
EXAMPLE = $(BIN_DIR)/example
TEST_UNIT = $(BIN_DIR)/test_unit
TEST_INTEGRATION = $(BIN_DIR)/test_integration
BENCHMARK = $(BIN_DIR)/benchmark
MUMPS_COMPARE = $(BIN_DIR)/mumps_compare

.PHONY: all clean lib example tests

all: lib example

lib: $(LIB)

example: $(EXAMPLE)

tests: $(TEST_UNIT) $(TEST_INTEGRATION) $(BENCHMARK) $(MUMPS_COMPARE)

# 创建目录
$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

# 编译规则
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 库
$(LIB): $(OBJS) | $(BIN_DIR)
	ar rcs $@ $^

# 示例程序
$(EXAMPLE): examples/example.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BIN_DIR) -lpard $(LDFLAGS) -o $@

# 测试程序
$(TEST_UNIT): tests/unit/test_unit.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BIN_DIR) -lpard $(LDFLAGS) -o $@

$(TEST_INTEGRATION): tests/integration/test_integration.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BIN_DIR) -lpard $(LDFLAGS) -o $@

$(BENCHMARK): tests/benchmark/benchmark.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BIN_DIR) -lpard $(LDFLAGS) -o $@

$(MUMPS_COMPARE): tests/benchmark/mumps_compare.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BIN_DIR) -lpard $(LDFLAGS) -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

install: lib
	install -d $(PREFIX)/lib $(PREFIX)/include
	install $(LIB) $(PREFIX)/lib/
	install include/pard.h $(PREFIX)/include/
