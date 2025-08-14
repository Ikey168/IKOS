#!/bin/bash
# Build script for user-space IPC components

echo "Building IKOS User-Space IPC API..."

# Create directories
mkdir -p /workspaces/IKOS/lib
mkdir -p /workspaces/IKOS/bin

# Build user-space IPC library
cd /workspaces/IKOS/user
gcc -Wall -Wextra -std=c99 -I../include -c ipc_user_api.c -o ipc_user_api.o
ar rcs libipc_user.a ipc_user_api.o
cp libipc_user.a ../lib/

# Build example program with stubs
gcc -Wall -Wextra -std=c99 -I../include -o ipc_example ipc_example.c ipc_user_api.c ipc_syscall_stubs.c
cp ipc_example ../bin/

# Build test program with stubs
gcc -Wall -Wextra -std=c99 -I../include -o ipc_test ipc_test.c ipc_user_api.c ipc_syscall_stubs.c
cp ipc_test ../bin/

echo "Build complete!"
echo "Library: /workspaces/IKOS/lib/libipc_user.a"
echo "Example: /workspaces/IKOS/bin/ipc_example"
echo "Tests:   /workspaces/IKOS/bin/ipc_test"
