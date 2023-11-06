# Compiler and flags
CC := gcc
CFLAGS := -g -Wall -Wextra -Iinclude -Wconversion -pedantic-errors -Werror

# Source directory, target directory, and source files
DEPS := include/utils.h
SRC_DIR := src
BIN_DIR := bin
SRCS := $(wildcard $(SRC_DIR)/*.c)
BINS := $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%, $(SRCS))

# Build rule
all: $(BINS)

$(BIN_DIR)/%: $(SRC_DIR)/%.c include/%.h $(DEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean

run-%: $(BIN_DIR)/% $(DEPS)
	./$<
