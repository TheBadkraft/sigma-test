CC = gcc
CFLAGS = -Wall -g -fPIC -I$(INCLUDE_DIR)
LDFLAGS = -shared
TST_CFLAGS = $(CFLAGS) -DSIGTEST_TEST
TST_LDFLAGS = -g
CLI_CFLAGS = $(CFLAGS)
CLI_LDFLAGS = -g -L$(LIB_DIR) -lsigtest -Wl,-rpath,$(LIB_DIR)

SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin
LIB_DIR = $(BIN_DIR)/lib
TEST_DIR = test
LIB_TEST_DIR = test/lib
TST_BUILD_DIR = $(BUILD_DIR)/test

SRCS = $(wildcard $(SRC_DIR)/*.c)
CLI_SRC = $(SRC_DIR)/sigtest_cli.c
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(filter-out $(CLI_SRC), $(SRCS)))
CLI_OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(CLI_SRC))
TST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TST_OBJS = $(patsubst $(TEST_DIR)/%.c, $(TST_BUILD_DIR)/%.o, $(TST_SRCS))
LIB_TST_SRCS = $(wildcard $(LIB_TEST_DIR)/*.c)
LIB_TST_OBJS = $(patsubst $(LIB_TEST_DIR)/%.c, $(TST_BUILD_DIR)/%.o, $(LIB_TST_SRCS))

HEADER = $(INCLUDE_DIR)/sigtest.h
LIB_TEST_HEADER = $(LIB_TEST_DIR)/math_utils.h

LIB_TARGET = $(LIB_DIR)/libsigtest.so
BIN_TARGET = $(BIN_DIR)/sigtest
TST_TARGET = $(TST_BUILD_DIR)/run_tests

INSTALL_LIB_DIR = /usr/lib
INSTALL_INCLUDE_DIR = /usr/include
INSTALL_BIN_DIR = /usr/bin

.PRECIOUS: $(TST_BUILD_DIR)/test_%

all: $(LIB_TARGET)

$(LIB_TARGET): $(OBJS)
	@mkdir -p $(LIB_DIR)
	$(CC) $(OBJS) -o $(LIB_TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TST_CFLAGS) -c $< -o $@

$(CLI_OBJ): $(CLI_SRC) $(HEADER)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CLI_CFLAGS) -c $< -o $@

$(TST_BUILD_DIR)/%.o: $(TEST_DIR)/%.c $(HEADER)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $(TST_CFLAGS) -c $< -o $@

$(TST_BUILD_DIR)/%.o: $(LIB_TEST_DIR)/%.c $(HEADER) $(LIB_TEST_HEADER)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $(TST_CFLAGS) -I$(LIB_TEST_DIR) -c $< -o $@

$(BIN_TARGET): $(LIB_TARGET) $(CLI_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CLI_OBJ) -o $(BIN_TARGET) $(CLI_LDFLAGS)

$(TST_TARGET): $(TST_OBJS) $(OBJS)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $(TST_OBJS) $(OBJS) -o $(TST_TARGET) $(TST_LDFLAGS)

$(TST_BUILD_DIR)/test_%: $(TST_BUILD_DIR)/test_%.o $(OBJS)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $< $(OBJS) -o $@ $(TST_LDFLAGS)

$(TST_BUILD_DIR)/test_lib: $(TST_BUILD_DIR)/test_lib.o $(TST_BUILD_DIR)/math_utils.o $(LIB_TARGET)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $(TST_BUILD_DIR)/test_lib.o $(TST_BUILD_DIR)/math_utils.o -o $@ -L$(LIB_DIR) -lsigtest $(TST_LDFLAGS)

lib: $(LIB_TARGET) $(HEADER)

cli: $(BIN_TARGET)

test_lib: $(TST_BUILD_DIR)/test_lib
	@$<

install: $(LIB_TARGET) $(HEADER) $(BIN_TARGET)
	sudo cp $(LIB_TARGET) $(INSTALL_LIB_DIR)/
	sudo cp $(INCLUDE_DIR)/sigtest.h $(INSTALL_INCLUDE_DIR)/
	sudo cp $(BIN_TARGET) $(INSTALL_BIN_DIR)/
	sudo ldconfig

build_%: $(TST_BUILD_DIR)/test_%
	@echo "Built $<"

build_test_%: $(TST_BUILD_DIR)/test_%
	@echo "Built $<"

test: $(TST_TARGET)
	@$

clean:
	find $(BUILD_DIR) -type f -delete
	find $(BIN_DIR) -type f -delete

.PHONY: all clean lib cli install test test_% build_% build_test_% test_lib