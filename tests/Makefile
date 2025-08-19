# Terminal Emulator Test Makefile
# Issue #34 - VT100/ANSI Terminal Emulator Implementation
# Issue #49 - ext2/ext4 Filesystem Testing

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g -I../include -DTEST_MODE
LDFLAGS = -static

# Directories
KERNEL_DIR = ../kernel
INCLUDE_DIR = ../include
BUILD_DIR = build

# Source files
TERMINAL_SOURCES = $(KERNEL_DIR)/terminal.c $(KERNEL_DIR)/terminal_escape.c $(KERNEL_DIR)/terminal_extended.c
TEST_SOURCES = test_terminal.c
STUB_SOURCES = terminal_test_stubs.c
EXT2_TEST_SOURCES = test_ext2_host.c

# Object files
TERMINAL_OBJECTS = $(BUILD_DIR)/terminal.o $(BUILD_DIR)/terminal_escape.o $(BUILD_DIR)/terminal_extended.o
TEST_OBJECTS = $(BUILD_DIR)/test_terminal.o
STUB_OBJECTS = $(BUILD_DIR)/terminal_test_stubs.o
EXT2_TEST_OBJECTS = $(BUILD_DIR)/test_ext2_host.o

# Targets
TERMINAL_TARGET = $(BUILD_DIR)/terminal_test
EXT2_TARGET = $(BUILD_DIR)/ext2_test

.PHONY: all clean run run-terminal run-ext2

all: $(BUILD_DIR) $(TERMINAL_TARGET) $(EXT2_TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TERMINAL_TARGET): $(TERMINAL_OBJECTS) $(TEST_OBJECTS) $(STUB_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

$(EXT2_TARGET): $(EXT2_TEST_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/terminal.o: $(KERNEL_DIR)/terminal.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/terminal_escape.o: $(KERNEL_DIR)/terminal_escape.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/terminal_extended.o: $(KERNEL_DIR)/terminal_extended.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_terminal.o: test_terminal.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/terminal_test_stubs.o: terminal_test_stubs.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_ext2_host.o: test_ext2_host.c
	$(CC) $(CFLAGS) -c $< -o $@

run: run-terminal

run-terminal: $(TERMINAL_TARGET)
	@echo "Running Terminal Emulator Test Suite..."
	@echo "========================================"
	./$(TERMINAL_TARGET)

run-ext2: $(EXT2_TARGET)
	@echo "Running ext2/ext4 Filesystem Test Suite..."
	@echo "=========================================="
	./$(EXT2_TARGET)

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "IKOS Test Suite Makefile"
	@echo "========================"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build all test executables"
	@echo "  run          - Run all test suites (default: terminal)"
	@echo "  run-terminal - Build and run terminal emulator tests"
	@echo "  run-ext2     - Build and run ext2/ext4 filesystem tests"
	@echo "  clean        - Remove build artifacts"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "The test suite validates:"
	@echo "  - Terminal initialization and configuration"
	@echo "  - Character writing and cursor control"
	@echo "  - VT100/ANSI escape sequence processing"
	@echo "  - Screen manipulation and scrolling"
	@echo "  - Color and text attribute handling"
	@echo "  - Input handling and key mapping"
	@echo "  - Alternate screen buffer management"
	@echo "  - Scrollback buffer functionality"
