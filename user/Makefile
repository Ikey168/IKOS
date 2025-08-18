# User-Space Makefile for IKOS IPC and File System APIs

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I../include -I../kernel -D__USER_SPACE__
LDFLAGS = 

# Directories
BUILDDIR = ../build

# IPC Source files
IPC_SOURCES = ipc_user_api.c ipc_example.c
IPC_OBJECTS = $(IPC_SOURCES:.c=.o)

# File System Source files
FS_SOURCES = fs_user_api.c fs_commands.c vfs_stubs.c
FS_OBJECTS = $(FS_SOURCES:.c=.o)

# CLI Shell Source files - Issue #36
CLI_SOURCES = ikos_cli_shell.c simple_shell.c ikos_cli_standalone.c
CLI_OBJECTS = $(CLI_SOURCES:.c=.o)

# Test and example sources
TEST_SOURCES = fs_test.c cli_shell_test.c
EXAMPLE_SOURCES = file_manager_example.c simple_editor.c
ALL_SOURCES = $(IPC_SOURCES) $(FS_SOURCES) $(CLI_SOURCES) $(TEST_SOURCES) $(EXAMPLE_SOURCES)
ALL_OBJECTS = $(ALL_SOURCES:.c=.o)

# Targets
IPC_EXAMPLE = ipc_example
IPC_LIBRARY = libipc_user.a
FS_LIBRARY = libfs_user.a
FS_TEST = fs_test
FILE_MANAGER = file_manager
SIMPLE_EDITOR = simple_editor
CLI_SHELL = ikos_cli_shell
CLI_TEST = cli_shell_test
CLI_STANDALONE = ikos_cli_standalone

.PHONY: all clean ipc fs cli examples test help install

all: ipc fs cli examples

ipc: $(IPC_EXAMPLE) $(IPC_LIBRARY)

fs: $(FS_LIBRARY)

cli: $(CLI_SHELL) $(CLI_STANDALONE)

examples: $(FILE_MANAGER) $(SIMPLE_EDITOR)

test: $(FS_TEST) $(CLI_TEST)

help:
	@echo "IKOS User-Space Build System"
	@echo "============================"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build IPC, filesystem, CLI shell, and examples"
	@echo "  ipc        - Build IPC library and example"
	@echo "  fs         - Build filesystem library"
	@echo "  cli        - Build CLI shell (Issue #36)"
	@echo "  examples   - Build example programs"
	@echo "  test       - Build and run filesystem tests"
	@echo "  clean      - Clean all build files"
	@echo "  install    - Install libraries and headers"
	@echo "  help       - Show this help"
	@echo ""
	@echo "File System Examples:"
	@echo "  make $(FILE_MANAGER) && ./$(FILE_MANAGER)"
	@echo "  make $(SIMPLE_EDITOR) && ./$(SIMPLE_EDITOR)"
	@echo "  make $(FS_TEST) && ./$(FS_TEST)"

# Build rules

# IPC targets
$(IPC_EXAMPLE): ipc_example.o ipc_user_api.o
	$(CC) $(LDFLAGS) -o $@ $^

$(IPC_LIBRARY): ipc_user_api.o
	ar rcs $@ $^

# File System targets
$(FS_LIBRARY): $(FS_OBJECTS)
	ar rcs $@ $^

$(FS_TEST): fs_test.o $(FS_OBJECTS) ipc_syscall_stubs.o
	$(CC) $(LDFLAGS) -o $@ $^

$(FILE_MANAGER): file_manager_example.o $(FS_OBJECTS) ipc_syscall_stubs.o
	$(CC) $(LDFLAGS) -o $@ $^

$(SIMPLE_EDITOR): simple_editor.o fs_user_api.o ipc_syscall_stubs.o
	$(CC) $(LDFLAGS) -o $@ $^

# CLI Shell targets - Issue #36
$(CLI_SHELL): ikos_cli_shell.o fs_user_api.o ipc_user_api.o
	$(CC) $(LDFLAGS) -o $@ $^

$(CLI_STANDALONE): ikos_cli_standalone.o
	$(CC) $(LDFLAGS) -o $@ $^

$(CLI_TEST): cli_shell_test.o ikos_cli_shell.o fs_user_api.o
	$(CC) $(LDFLAGS) -o $@ $^

# Generic compilation rule
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean target
clean:
	rm -f $(ALL_OBJECTS) ipc_syscall_stubs.o
	rm -f $(IPC_EXAMPLE) $(IPC_LIBRARY) $(FS_LIBRARY)
	rm -f $(FS_TEST) $(FILE_MANAGER) $(SIMPLE_EDITOR)

# Install target
install: $(IPC_LIBRARY) $(FS_LIBRARY)
	@mkdir -p ../lib ../include
	cp $(IPC_LIBRARY) $(FS_LIBRARY) ../lib/
	cp ipc_user_api.h ../include/
	cp fs_user_api.h ../include/ 2>/dev/null || true
	cp fs_commands.h ../include/ 2>/dev/null || true

# Test targets for IPC
test-ipc-server: $(IPC_EXAMPLE)
	./$(IPC_EXAMPLE) server

test-ipc-client: $(IPC_EXAMPLE)
	./$(IPC_EXAMPLE) client 1

test-ipc-broadcast: $(IPC_EXAMPLE)
	./$(IPC_EXAMPLE) broadcast

test-ipc-async: $(IPC_EXAMPLE)
	./$(IPC_EXAMPLE) async

# Test targets for File System
test-fs: $(FS_TEST)
	@echo "Running filesystem API tests..."
	./$(FS_TEST)

demo-file-manager: $(FILE_MANAGER)
	@echo "Starting File Manager demo..."
	./$(FILE_MANAGER)

demo-editor: $(SIMPLE_EDITOR)
	@echo "Starting Simple Editor demo..."
	./$(SIMPLE_EDITOR)

# Dependencies
ipc_example.o: ipc_example.c ipc_user_api.h
ipc_user_api.o: ipc_user_api.c ipc_user_api.h
fs_user_api.o: fs_user_api.c fs_user_api.h
fs_commands.o: fs_commands.c fs_commands.h fs_user_api.h
fs_test.o: fs_test.c fs_user_api.h fs_commands.h
file_manager_example.o: file_manager_example.c fs_user_api.h fs_commands.h
simple_editor.o: simple_editor.c fs_user_api.h
ipc_syscall_stubs.o: ipc_syscall_stubs.c
ikos_cli_shell.o: ikos_cli_shell.c ikos_cli_shell.h fs_user_api.h
ikos_cli_standalone.o: ikos_cli_standalone.c ikos_cli_shell.h
cli_shell_test.o: cli_shell_test.c ikos_cli_shell.h
simple_shell.o: simple_shell.c ikos_cli_shell.h
