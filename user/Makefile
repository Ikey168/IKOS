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

# Socket API Source files - Issue #46
SOCKET_SOURCES = socket_user_api.c socket_examples.c socket_test.c
SOCKET_OBJECTS = $(SOCKET_SOURCES:.c=.o)

# Threading API Source files - Issue #52
THREADING_SOURCES = pthread.c threading_examples.c threading_test.c
THREADING_OBJECTS = $(THREADING_SOURCES:.c=.o)

# DNS API Source files - Issue #47
DNS_SOURCES = dns_user_api.c dns_examples.c dns_test.c
DNS_OBJECTS = $(DNS_SOURCES:.c=.o)

# TLS/SSL API Source files - Issue #48
TLS_SOURCES = tls_user_api.c tls_examples.c
TLS_OBJECTS = $(TLS_SOURCES:.c=.o)

# Test and example sources
TEST_SOURCES = fs_test.c cli_shell_test.c socket_test.c threading_test.c dns_test.c
EXAMPLE_SOURCES = file_manager_example.c simple_editor.c socket_examples.c threading_examples.c dns_examples.c tls_examples.c
ALL_SOURCES = $(IPC_SOURCES) $(FS_SOURCES) $(CLI_SOURCES) $(SOCKET_SOURCES) $(THREADING_SOURCES) $(DNS_SOURCES) $(TLS_SOURCES) $(TEST_SOURCES) $(EXAMPLE_SOURCES)
ALL_OBJECTS = $(ALL_SOURCES:.c=.o)

# Targets
IPC_EXAMPLE = ipc_example
IPC_LIBRARY = libipc_user.a
FS_LIBRARY = libfs_user.a
SOCKET_LIBRARY = libsocket_user.a
THREADING_LIBRARY = libpthread_user.a
DNS_LIBRARY = libdns_user.a
TLS_LIBRARY = libtls_user.a
FS_TEST = fs_test
FILE_MANAGER = file_manager
SIMPLE_EDITOR = simple_editor
CLI_SHELL = ikos_cli_shell
CLI_TEST = cli_shell_test
CLI_STANDALONE = ikos_cli_standalone
SOCKET_EXAMPLES = socket_examples
SOCKET_TEST = socket_test
THREADING_EXAMPLES = threading_examples
DNS_EXAMPLES = dns_examples
DNS_TEST = dns_test
TLS_EXAMPLES = tls_examples
THREADING_TEST = threading_test

.PHONY: all clean ipc fs cli sockets threading tls examples test help install

all: ipc fs cli sockets threading tls examples

ipc: $(IPC_EXAMPLE) $(IPC_LIBRARY)

fs: $(FS_LIBRARY)

cli: $(CLI_SHELL) $(CLI_STANDALONE)

sockets: $(SOCKET_LIBRARY) $(SOCKET_EXAMPLES)

threading: $(THREADING_LIBRARY) $(THREADING_EXAMPLES) $(THREADING_TEST)

tls: $(TLS_LIBRARY) $(TLS_EXAMPLES)

examples: $(FILE_MANAGER) $(SIMPLE_EDITOR) $(SOCKET_EXAMPLES)

test: $(FS_TEST) $(CLI_TEST) $(SOCKET_TEST)

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
	rm -f $(IPC_EXAMPLE) $(IPC_LIBRARY) $(FS_LIBRARY) $(SOCKET_LIBRARY) $(THREADING_LIBRARY)
	rm -f $(FS_TEST) $(FILE_MANAGER) $(SIMPLE_EDITOR)
	rm -f $(SOCKET_EXAMPLES) $(SOCKET_TEST)
	rm -f $(THREADING_EXAMPLES) $(THREADING_TEST)

# Install target
install: $(IPC_LIBRARY) $(FS_LIBRARY) $(SOCKET_LIBRARY) $(THREADING_LIBRARY)
	@mkdir -p ../lib ../include
	cp $(IPC_LIBRARY) $(FS_LIBRARY) $(SOCKET_LIBRARY) $(THREADING_LIBRARY) ../lib/
	cp ipc_user_api.h ../include/
	cp fs_user_api.h ../include/ 2>/dev/null || true
	cp fs_commands.h ../include/ 2>/dev/null || true
	cp socket_user_api.h ../include/ 2>/dev/null || true
	cp ../include/pthread.h ../include/ 2>/dev/null || true

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

# Socket API targets - Issue #46
$(SOCKET_LIBRARY): socket_user_api.o
	ar rcs $@ $^

$(SOCKET_EXAMPLES): socket_examples.o socket_user_api.o
	$(CC) $(LDFLAGS) -o $@ $^

$(SOCKET_TEST): socket_test.o socket_user_api.o
	$(CC) $(LDFLAGS) -o $@ $^

demo-socket-examples: $(SOCKET_EXAMPLES)
	@echo "Starting Socket API examples..."
	./$(SOCKET_EXAMPLES)

demo-socket-test: $(SOCKET_TEST)
	@echo "Running Socket API tests..."
	./$(SOCKET_TEST)

# Threading API targets - Issue #52
$(THREADING_LIBRARY): pthread.o
	ar rcs $@ $^

$(THREADING_EXAMPLES): threading_examples.o pthread.o
	$(CC) $(LDFLAGS) -o $@ $^

$(THREADING_TEST): threading_test.o pthread.o
	$(CC) $(LDFLAGS) -o $@ $^

demo-threading-examples: $(THREADING_EXAMPLES)
	@echo "Starting Threading API examples..."
	./$(THREADING_EXAMPLES)

demo-threading-test: $(THREADING_TEST)
	@echo "Running Threading API tests..."
	./$(THREADING_TEST)

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
socket_user_api.o: socket_user_api.c socket_user_api.h
socket_examples.o: socket_examples.c socket_user_api.h
socket_test.o: socket_test.c socket_user_api.h
pthread.o: pthread.c ../include/pthread.h ../include/thread_syscalls.h
threading_examples.o: threading_examples.c ../include/pthread.h
threading_test.o: threading_test.c ../include/pthread.h
fs_test.o: fs_test.c fs_user_api.h fs_commands.h
file_manager_example.o: file_manager_example.c fs_user_api.h fs_commands.h
simple_editor.o: simple_editor.c fs_user_api.h
ipc_syscall_stubs.o: ipc_syscall_stubs.c
ikos_cli_shell.o: ikos_cli_shell.c ikos_cli_shell.h fs_user_api.h
ikos_cli_standalone.o: ikos_cli_standalone.c ikos_cli_shell.h
cli_shell_test.o: cli_shell_test.c ikos_cli_shell.h
simple_shell.o: simple_shell.c ikos_cli_shell.h
