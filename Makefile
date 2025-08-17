# IKOS Kernel Makefile
# Builds the bootloader and kernel with comprehensive system components

# Assembler and tools
ASM = nasm
CC = gcc
LD = ld
QEMU = qemu-system-x86_64

# Compiler flags
CFLAGS = -m64 -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -Wall -Wextra -std=c99 -O2 -g -Iinclude/
ASMFLAGS = -f elf64 -g -Iinclude/
LDFLAGS = -T kernel/kernel.ld -nostdlib

# Directories
BOOT_DIR = boot
KERNEL_DIR = kernel
TESTS_DIR = tests
BUILD_DIR = build
INCLUDE_DIR = include

# Kernel source files
KERNEL_C_SOURCES = $(wildcard $(KERNEL_DIR)/*.c)
KERNEL_ASM_SOURCES = $(wildcard $(KERNEL_DIR)/*.asm)
KERNEL_OBJECTS = $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SOURCES)) \
                 $(patsubst $(KERNEL_DIR)/%.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SOURCES))

# Test source files
TEST_C_SOURCES = $(wildcard $(TESTS_DIR)/*.c)
TEST_OBJECTS = $(patsubst $(TESTS_DIR)/%.c,$(BUILD_DIR)/test_%.o,$(TEST_C_SOURCES))

# VMM specific files
VMM_SOURCES = $(KERNEL_DIR)/vmm.c $(KERNEL_DIR)/vmm_cow.c $(KERNEL_DIR)/vmm_regions.c \
              $(KERNEL_DIR)/vmm_interrupts.c $(KERNEL_DIR)/vmm_asm.asm
VMM_OBJECTS = $(BUILD_DIR)/vmm.o $(BUILD_DIR)/vmm_cow.o $(BUILD_DIR)/vmm_regions.o \
              $(BUILD_DIR)/vmm_interrupts.o $(BUILD_DIR)/vmm_asm.o

# Interrupt handling specific files
INTERRUPT_SOURCES = $(KERNEL_DIR)/idt.c $(KERNEL_DIR)/interrupt_handlers.c \
                    $(KERNEL_DIR)/interrupt_stubs.asm
INTERRUPT_OBJECTS = $(BUILD_DIR)/idt.o $(BUILD_DIR)/interrupt_handlers.o \
                    $(BUILD_DIR)/interrupt_stubs.o

# User-space process execution specific files
USERSPACE_SOURCES = $(KERNEL_DIR)/process.c $(KERNEL_DIR)/elf_loader.c \
                    $(KERNEL_DIR)/syscalls.c $(KERNEL_DIR)/user_mode.asm
USERSPACE_OBJECTS = $(BUILD_DIR)/process.o $(BUILD_DIR)/elf_loader.o \
                    $(BUILD_DIR)/syscalls.o $(BUILD_DIR)/user_mode.o

# Process Manager specific files
PROCESS_MANAGER_SOURCES = $(KERNEL_DIR)/process_manager.c $(KERNEL_DIR)/pm_syscalls.c $(KERNEL_DIR)/string_utils.c
PROCESS_MANAGER_OBJECTS = $(BUILD_DIR)/process_manager.o $(BUILD_DIR)/pm_syscalls.o $(BUILD_DIR)/string_utils.o

# Virtual File System specific files
VFS_SOURCES = $(KERNEL_DIR)/vfs.c $(KERNEL_DIR)/ramfs.c
VFS_OBJECTS = $(BUILD_DIR)/vfs.o $(BUILD_DIR)/ramfs.o

# FAT Filesystem specific files
FAT_SOURCES = $(KERNEL_DIR)/fat.c $(KERNEL_DIR)/ramdisk.c
FAT_OBJECTS = $(BUILD_DIR)/fat.o $(BUILD_DIR)/ramdisk.o

# Keyboard Driver specific files
KEYBOARD_SOURCES = $(KERNEL_DIR)/keyboard.c $(KERNEL_DIR)/keyboard_syscalls.c \
                   $(KERNEL_DIR)/keyboard_user_api.c
KEYBOARD_OBJECTS = $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/keyboard_syscalls.o \
                   $(BUILD_DIR)/keyboard_user_api.o

# Advanced Memory Management specific files - Issue #27
MEMORY_ADVANCED_SOURCES = $(KERNEL_DIR)/buddy_allocator.c $(KERNEL_DIR)/slab_allocator.c \
                          $(KERNEL_DIR)/demand_paging.c $(KERNEL_DIR)/memory_compression.c \
                          $(KERNEL_DIR)/numa_allocator.c $(KERNEL_DIR)/advanced_memory_manager.c
MEMORY_ADVANCED_OBJECTS = $(BUILD_DIR)/buddy_allocator.o $(BUILD_DIR)/slab_allocator.o \
                          $(BUILD_DIR)/demand_paging.o $(BUILD_DIR)/memory_compression.o \
                          $(BUILD_DIR)/numa_allocator.o $(BUILD_DIR)/advanced_memory_manager.o

# Authentication & Authorization System specific files - Issue #31
AUTH_SOURCES = $(KERNEL_DIR)/auth_core.c $(KERNEL_DIR)/auth_authorization.c \
               $(KERNEL_DIR)/auth_mfa.c
AUTH_OBJECTS = $(BUILD_DIR)/auth_core.o $(BUILD_DIR)/auth_authorization.o \
               $(BUILD_DIR)/auth_mfa.o
AUTH_LIBS = -lssl -lcrypto -lpthread

# Files
BOOT_ASM = $(BOOT_DIR)/boot.asm
BOOT_ENHANCED_ASM = $(BOOT_DIR)/boot_enhanced.asm
BOOT_COMPACT_ASM = $(BOOT_DIR)/boot_compact.asm
BOOT_PROTECTED_ASM = $(BOOT_DIR)/boot_protected.asm
BOOT_PROTECTED_COMPACT_ASM = $(BOOT_DIR)/boot_protected_compact.asm
BOOT_ELF_LOADER_ASM = $(BOOT_DIR)/boot_elf_loader.asm
BOOT_ELF_COMPACT_ASM = $(BOOT_DIR)/boot_elf_compact.asm
BOOT_LONGMODE_ASM = $(BOOT_DIR)/boot_longmode.asm
BOOT_BIN = $(BUILD_DIR)/boot.bin
BOOT_ENHANCED_BIN = $(BUILD_DIR)/boot_enhanced.bin
BOOT_COMPACT_BIN = $(BUILD_DIR)/boot_compact.bin
BOOT_PROTECTED_BIN = $(BUILD_DIR)/boot_protected.bin
BOOT_PROTECTED_COMPACT_BIN = $(BUILD_DIR)/boot_protected_compact.bin
BOOT_ELF_LOADER_BIN = $(BUILD_DIR)/boot_elf_loader.bin
BOOT_ELF_COMPACT_BIN = $(BUILD_DIR)/boot_elf_compact.bin
BOOT_LONGMODE_BIN = $(BUILD_DIR)/boot_longmode.bin
DISK_IMG = $(BUILD_DIR)/ikos.img
DISK_ENHANCED_IMG = $(BUILD_DIR)/ikos_enhanced.img
DISK_COMPACT_IMG = $(BUILD_DIR)/ikos_compact.img
DISK_PROTECTED_IMG = $(BUILD_DIR)/ikos_protected.img
DISK_PROTECTED_COMPACT_IMG = $(BUILD_DIR)/ikos_protected_compact.img
DISK_ELF_LOADER_IMG = $(BUILD_DIR)/ikos_elf_loader.img
DISK_ELF_COMPACT_IMG = $(BUILD_DIR)/ikos_elf_compact.img
DISK_LONGMODE_IMG = $(BUILD_DIR)/ikos_longmode.img

# Default target - build kernel with VMM, interrupt handling, user-space support, process manager, VFS, FAT filesystem, keyboard driver, advanced memory management, and authentication system
all: $(BUILD_DIR)/kernel.elf $(BUILD_DIR)/vmm_test $(BUILD_DIR)/interrupt_test $(BUILD_DIR)/userspace_test $(BUILD_DIR)/process_manager_test $(BUILD_DIR)/vfs_test $(BUILD_DIR)/fat_test $(BUILD_DIR)/keyboard_test $(BUILD_DIR)/advanced_memory_test $(BUILD_DIR)/auth_test $(DISK_LONGMODE_IMG)

# Kernel ELF binary
KERNEL_ELF = $(BUILD_DIR)/kernel.elf

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# =============================================================================
# KERNEL BUILD RULES
# =============================================================================

# Build kernel ELF
$(BUILD_DIR)/kernel.elf: $(KERNEL_OBJECTS) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) $(KERNEL_OBJECTS) -o $@

# Compile C source files
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile assembly files
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS) $< -o $@

# Compile test files
$(BUILD_DIR)/test_%.o: $(TESTS_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build VMM test executable
$(BUILD_DIR)/vmm_test: $(VMM_OBJECTS) $(BUILD_DIR)/test_vmm.o | $(BUILD_DIR)
	$(CC) -o $@ $^ -nostdlib -lgcc

# Build interrupt handling test executable
$(BUILD_DIR)/interrupt_test: $(INTERRUPT_OBJECTS) $(BUILD_DIR)/test_interrupts.o | $(BUILD_DIR)
	$(CC) -o $@ $^ -nostdlib -lgcc

# Build user-space process execution test executable
$(BUILD_DIR)/userspace_test: $(USERSPACE_OBJECTS) $(BUILD_DIR)/test_user_space.o | $(BUILD_DIR)
	$(CC) -o $@ $^ -nostdlib -lgcc -no-pie

# Build process manager test executable
$(BUILD_DIR)/process_manager_test: $(PROCESS_MANAGER_OBJECTS) $(BUILD_DIR)/test_stubs.o $(TESTS_DIR)/test_process_manager.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(PROCESS_MANAGER_OBJECTS) $(BUILD_DIR)/test_stubs.o $(TESTS_DIR)/test_process_manager.c -nostdlib -lgcc -no-pie

# Build VFS test executable
$(BUILD_DIR)/vfs_test: $(VFS_OBJECTS) $(BUILD_DIR)/test_vfs.o | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(VFS_OBJECTS) $(BUILD_DIR)/test_vfs.o -nostdlib -lgcc -no-pie

# Build FAT filesystem test executable
$(BUILD_DIR)/fat_test: $(FAT_OBJECTS) $(VFS_OBJECTS) $(BUILD_DIR)/test_fat.o | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(FAT_OBJECTS) $(VFS_OBJECTS) $(BUILD_DIR)/test_fat.o -nostdlib -lgcc -no-pie

# Build keyboard driver test executable
$(BUILD_DIR)/keyboard_test: $(KEYBOARD_OBJECTS) $(BUILD_DIR)/test_test_keyboard.o | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(KEYBOARD_OBJECTS) $(BUILD_DIR)/test_test_keyboard.o -nostdlib -lgcc -no-pie

# Build advanced memory management test executable
$(BUILD_DIR)/advanced_memory_test: $(MEMORY_ADVANCED_OBJECTS) $(BUILD_DIR)/test_advanced_memory.o | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(MEMORY_ADVANCED_OBJECTS) $(BUILD_DIR)/test_advanced_memory.o -nostdlib -lgcc -no-pie

# Build authentication & authorization system test executable
$(BUILD_DIR)/auth_test: $(AUTH_OBJECTS) $(BUILD_DIR)/auth_test.o | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(AUTH_OBJECTS) $(KERNEL_DIR)/auth_test.c $(AUTH_LIBS) -no-pie

# =============================================================================
# VMM SPECIFIC TARGETS
# =============================================================================

# Build VMM only
vmm: $(VMM_OBJECTS)
	@echo "VMM components built successfully"

# Run VMM tests
test-vmm: $(BUILD_DIR)/vmm_test
	$(BUILD_DIR)/vmm_test

# VMM smoke test
vmm-smoke: $(BUILD_DIR)/vmm_test
	$(BUILD_DIR)/vmm_test smoke

# =============================================================================
# INTERRUPT HANDLING TARGETS
# =============================================================================

# Build interrupt handling only
interrupts: $(INTERRUPT_OBJECTS)
	@echo "Interrupt handling components built successfully"

# Run interrupt tests
test-interrupts: $(BUILD_DIR)/interrupt_test
	$(BUILD_DIR)/interrupt_test

# Interrupt smoke test
interrupt-smoke: $(BUILD_DIR)/interrupt_test
	$(BUILD_DIR)/interrupt_test smoke

# =============================================================================
# USER-SPACE PROCESS EXECUTION TARGETS
# =============================================================================

# Build user-space process execution only
userspace: $(USERSPACE_OBJECTS)
	@echo "User-space process execution components built successfully"

# Run user-space tests
test-userspace: $(BUILD_DIR)/userspace_test
	$(BUILD_DIR)/userspace_test

# User-space smoke test
userspace-smoke: $(BUILD_DIR)/userspace_test
	$(BUILD_DIR)/userspace_test smoke

# =============================================================================
# PROCESS MANAGER TARGETS
# =============================================================================

# Build process manager only
process-manager: $(PROCESS_MANAGER_OBJECTS) $(USERSPACE_OBJECTS)
	@echo "Process manager components built successfully"

# Run process manager tests
test-process-manager: $(BUILD_DIR)/process_manager_test
	$(BUILD_DIR)/process_manager_test

# Process manager smoke test
process-manager-smoke: $(BUILD_DIR)/process_manager_test
	$(BUILD_DIR)/process_manager_test smoke

# =============================================================================
# VFS TARGETS
# =============================================================================

# Build VFS only
vfs: $(VFS_OBJECTS)
	@echo "VFS components built successfully"

# Run VFS tests
test-vfs: $(BUILD_DIR)/vfs_test
	$(BUILD_DIR)/vfs_test

# VFS smoke test
vfs-smoke: $(BUILD_DIR)/vfs_test
	$(BUILD_DIR)/vfs_test smoke

# =============================================================================
# FAT FILESYSTEM TARGETS
# =============================================================================

# Build FAT filesystem only
fat: $(FAT_OBJECTS) $(VFS_OBJECTS)
	@echo "FAT filesystem components built successfully"

# Run FAT filesystem tests
test-fat: $(BUILD_DIR)/fat_test
	$(BUILD_DIR)/fat_test

# FAT filesystem smoke test
fat-smoke: $(BUILD_DIR)/fat_test
	$(BUILD_DIR)/fat_test smoke

# =============================================================================
# KEYBOARD DRIVER TARGETS
# =============================================================================

# Build keyboard driver only
keyboard: $(KEYBOARD_OBJECTS)
	@echo "Keyboard driver components built successfully"

# Run keyboard tests
test-keyboard: $(BUILD_DIR)/keyboard_test
	$(BUILD_DIR)/keyboard_test

# Keyboard smoke test
keyboard-smoke: $(BUILD_DIR)/keyboard_test
	$(BUILD_DIR)/keyboard_test smoke

# Keyboard hardware test (requires actual keyboard)
keyboard-hardware: $(BUILD_DIR)/keyboard_test
	$(BUILD_DIR)/keyboard_test hardware

# =============================================================================
# ADVANCED MEMORY MANAGEMENT TARGETS - Issue #27
# =============================================================================

# Build advanced memory management only
memory-advanced: $(MEMORY_ADVANCED_OBJECTS)
	@echo "Advanced memory management components built successfully"

# Run advanced memory management tests
test-memory-advanced: $(BUILD_DIR)/advanced_memory_test
	$(BUILD_DIR)/advanced_memory_test

# Advanced memory management smoke test
memory-advanced-smoke: $(BUILD_DIR)/advanced_memory_test
	$(BUILD_DIR)/advanced_memory_test smoke

# Advanced memory management stress test
memory-advanced-stress: $(BUILD_DIR)/advanced_memory_test
	$(BUILD_DIR)/advanced_memory_test stress

# Test individual memory management components
test-buddy: $(BUILD_DIR)/advanced_memory_test
	$(BUILD_DIR)/advanced_memory_test buddy

test-slab: $(BUILD_DIR)/advanced_memory_test
	$(BUILD_DIR)/advanced_memory_test slab

test-numa: $(BUILD_DIR)/advanced_memory_test
	$(BUILD_DIR)/advanced_memory_test numa

test-compression: $(BUILD_DIR)/advanced_memory_test
	$(BUILD_DIR)/advanced_memory_test compression

test-paging: $(BUILD_DIR)/advanced_memory_test
	$(BUILD_DIR)/advanced_memory_test paging

# =============================================================================
# AUTHENTICATION & AUTHORIZATION SYSTEM TARGETS - Issue #31
# =============================================================================

# Build authentication & authorization system only
auth: $(AUTH_OBJECTS)
	@echo "Authentication & authorization system components built successfully"

# Run authentication & authorization tests
test-auth: $(BUILD_DIR)/auth_test
	$(BUILD_DIR)/auth_test

# Authentication system smoke test
auth-smoke: $(BUILD_DIR)/auth_test
	$(BUILD_DIR)/auth_test smoke

# Authentication system security test
auth-security: $(BUILD_DIR)/auth_test
	$(BUILD_DIR)/auth_test security

# Test individual authentication components
test-user-management: $(BUILD_DIR)/auth_test
	$(BUILD_DIR)/auth_test user

test-sessions: $(BUILD_DIR)/auth_test
	$(BUILD_DIR)/auth_test sessions

test-roles: $(BUILD_DIR)/auth_test
	$(BUILD_DIR)/auth_test roles

test-permissions: $(BUILD_DIR)/auth_test
	$(BUILD_DIR)/auth_test permissions

test-mfa: $(BUILD_DIR)/auth_test
	$(BUILD_DIR)/auth_test mfa

test-acl: $(BUILD_DIR)/auth_test
	$(BUILD_DIR)/auth_test acl

# =============================================================================
# BOOTLOADER BUILD RULES
# =============================================================================

# Assemble bootloader
$(BOOT_BIN): $(BOOT_ASM) | $(BUILD_DIR)
	$(ASM) -f bin $(BOOT_ASM) -o $(BOOT_BIN)

# Assemble enhanced bootloader
$(BOOT_ENHANCED_BIN): $(BOOT_ENHANCED_ASM) | $(BUILD_DIR)
	$(ASM) -f bin $(BOOT_ENHANCED_ASM) -o $(BOOT_ENHANCED_BIN) -I include/

# Create disk image with basic bootloader
$(DISK_IMG): $(BOOT_BIN)
	dd if=/dev/zero of=$(DISK_IMG) bs=512 count=2880
	dd if=$(BOOT_BIN) of=$(DISK_IMG) bs=512 count=1 conv=notrunc

# Assemble compact bootloader
$(BOOT_COMPACT_BIN): $(BOOT_COMPACT_ASM) | $(BUILD_DIR)
	$(ASM) -f bin $(BOOT_COMPACT_ASM) -o $(BOOT_COMPACT_BIN) -I include/

# Create disk image with compact bootloader
$(DISK_COMPACT_IMG): $(BOOT_COMPACT_BIN)
	dd if=/dev/zero of=$(DISK_COMPACT_IMG) bs=512 count=2880
	dd if=$(BOOT_COMPACT_BIN) of=$(DISK_COMPACT_IMG) bs=512 count=1 conv=notrunc

# Assemble protected mode bootloader
$(BOOT_PROTECTED_BIN): $(BOOT_PROTECTED_ASM) | $(BUILD_DIR)
	$(ASM) -f bin $(BOOT_PROTECTED_ASM) -o $(BOOT_PROTECTED_BIN) -I include/

# Create disk image with protected mode bootloader
$(DISK_PROTECTED_IMG): $(BOOT_PROTECTED_BIN)
	dd if=/dev/zero of=$(DISK_PROTECTED_IMG) bs=512 count=2880
	dd if=$(BOOT_PROTECTED_BIN) of=$(DISK_PROTECTED_IMG) bs=512 count=1 conv=notrunc

# Assemble protected compact mode bootloader
$(BOOT_PROTECTED_COMPACT_BIN): $(BOOT_PROTECTED_COMPACT_ASM) | $(BUILD_DIR)
	$(ASM) -f bin $(BOOT_PROTECTED_COMPACT_ASM) -o $(BOOT_PROTECTED_COMPACT_BIN) -I include/

# Create disk image with protected compact mode bootloader
$(DISK_PROTECTED_COMPACT_IMG): $(BOOT_PROTECTED_COMPACT_BIN)
	dd if=/dev/zero of=$(DISK_PROTECTED_COMPACT_IMG) bs=512 count=2880
	dd if=$(BOOT_PROTECTED_COMPACT_BIN) of=$(DISK_PROTECTED_COMPACT_IMG) bs=512 count=1 conv=notrunc

# Assemble ELF kernel loader
$(BOOT_ELF_LOADER_BIN): $(BOOT_ELF_LOADER_ASM) | $(BUILD_DIR)
	$(ASM) -f bin $(BOOT_ELF_LOADER_ASM) -o $(BOOT_ELF_LOADER_BIN) -I include/

# Create disk image with ELF kernel loader
$(DISK_ELF_LOADER_IMG): $(BOOT_ELF_LOADER_BIN)
	dd if=/dev/zero of=$(DISK_ELF_LOADER_IMG) bs=512 count=2880
	dd if=$(BOOT_ELF_LOADER_BIN) of=$(DISK_ELF_LOADER_IMG) bs=512 count=1 conv=notrunc

# Assemble ELF compact kernel loader
$(BOOT_ELF_COMPACT_BIN): $(BOOT_ELF_COMPACT_ASM) | $(BUILD_DIR)
	$(ASM) -f bin $(BOOT_ELF_COMPACT_ASM) -o $(BOOT_ELF_COMPACT_BIN) -I include/

# Create disk image with ELF compact kernel loader
$(DISK_ELF_COMPACT_IMG): $(BOOT_ELF_COMPACT_BIN)
	dd if=/dev/zero of=$(DISK_ELF_COMPACT_IMG) bs=512 count=2880
	dd if=$(BOOT_ELF_COMPACT_BIN) of=$(DISK_ELF_COMPACT_IMG) bs=512 count=1 conv=notrunc

# Assemble long mode bootloader
$(BOOT_LONGMODE_BIN): $(BOOT_LONGMODE_ASM) | $(BUILD_DIR)
	$(ASM) -f bin $(BOOT_LONGMODE_ASM) -o $(BOOT_LONGMODE_BIN) -I include/

# Create disk image with long mode bootloader
$(DISK_LONGMODE_IMG): $(BOOT_LONGMODE_BIN)
	dd if=/dev/zero of=$(DISK_LONGMODE_IMG) bs=512 count=2880
	dd if=$(BOOT_LONGMODE_BIN) of=$(DISK_LONGMODE_IMG) bs=512 count=1 conv=notrunc

# Debug-enabled long mode bootloader
$(BUILD_DIR)/boot_longmode_debug.bin: boot/boot_longmode_debug.asm | $(BUILD_DIR)
	$(ASM) -f bin boot/boot_longmode_debug.asm -o $(BUILD_DIR)/boot_longmode_debug.bin -I include/

$(BUILD_DIR)/ikos_longmode_debug.img: $(BUILD_DIR)/boot_longmode_debug.bin
	dd if=/dev/zero of=$(BUILD_DIR)/ikos_longmode_debug.img bs=512 count=2880
	dd if=$(BUILD_DIR)/boot_longmode_debug.bin of=$(BUILD_DIR)/ikos_longmode_debug.img bs=512 count=1 conv=notrunc

debug-build: $(BUILD_DIR)/ikos_longmode_debug.img

# Test basic bootloader in QEMU
test: $(DISK_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_IMG) -no-reboot -no-shutdown -nographic

# Test enhanced bootloader in QEMU
test-enhanced: $(DISK_ENHANCED_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_ENHANCED_IMG) -no-reboot -no-shutdown

# Test with debugging
debug: $(DISK_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_IMG) -no-reboot -no-shutdown -s -S

# Test debug-enabled bootloader with serial output
test-debug: $(BUILD_DIR)/ikos_longmode_debug.img
	$(QEMU) -fda $(BUILD_DIR)/ikos_longmode_debug.img -boot a -nographic -chardev stdio,id=char0 -serial chardev:char0

# Debug session with GDB support
debug-gdb: $(BUILD_DIR)/ikos_longmode_debug.img
	$(QEMU) -fda $(BUILD_DIR)/ikos_longmode_debug.img -boot a -S -s -nographic -serial file:debug_serial.log

# Test compact bootloader in QEMU
test-compact: $(DISK_COMPACT_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_COMPACT_IMG) -no-reboot -no-shutdown -nographic

# Debug compact bootloader
debug-compact: $(DISK_COMPACT_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_COMPACT_IMG) -no-reboot -no-shutdown -s -S

# Test protected mode bootloader in QEMU
test-protected: $(DISK_PROTECTED_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_PROTECTED_IMG) -no-reboot -no-shutdown -nographic

# Test compact protected mode bootloader in QEMU
test-protected-compact: $(DISK_PROTECTED_COMPACT_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_PROTECTED_COMPACT_IMG) -no-reboot -no-shutdown -nographic

# Debug protected mode bootloader
debug-protected: $(DISK_PROTECTED_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_PROTECTED_IMG) -no-reboot -no-shutdown -s -S

# Debug compact protected mode bootloader
debug-protected-compact: $(DISK_PROTECTED_COMPACT_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_PROTECTED_COMPACT_IMG) -no-reboot -no-shutdown -s -S

# Test ELF kernel loader in QEMU
test-elf-loader: $(DISK_ELF_LOADER_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_ELF_LOADER_IMG) -no-reboot -no-shutdown -nographic

# Test compact ELF kernel loader in QEMU
test-elf-compact: $(DISK_ELF_COMPACT_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_ELF_COMPACT_IMG) -no-reboot -no-shutdown -nographic

# Debug ELF kernel loader
debug-elf-loader: $(DISK_ELF_LOADER_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_ELF_LOADER_IMG) -no-reboot -no-shutdown -s -S

# Debug compact ELF kernel loader
debug-elf-compact: $(DISK_ELF_COMPACT_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_ELF_COMPACT_IMG) -no-reboot -no-shutdown -s -S

# Test long mode bootloader in QEMU
test-longmode: $(DISK_LONGMODE_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_LONGMODE_IMG) -no-reboot -no-shutdown -nographic

# Debug long mode bootloader
debug-longmode: $(DISK_LONGMODE_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_LONGMODE_IMG) -no-reboot -no-shutdown -s -S

# Clean build files
clean:
	rm -rf $(BUILD_DIR)

# Install required tools (for Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y nasm qemu-system-x86 ovmf

# =============================================================================
# COMPREHENSIVE TESTING SUITE (Issue #8)
# =============================================================================

# Build debug-enabled bootloaders for testing
debug-builds: $(BUILD_DIR)
	$(ASM) -f bin $(BOOT_DIR)/boot_longmode_debug.asm -o $(BUILD_DIR)/ikos_longmode_debug.img -DDEBUG_ENABLED

# Run comprehensive QEMU testing suite
test-qemu: $(DISK_LONGMODE_IMG) debug-builds
	@echo "=== Running QEMU Testing Suite ==="
	./test_qemu.sh

# Set up real hardware testing environment
setup-real-hardware: $(DISK_LONGMODE_IMG)
	@echo "=== Setting up Real Hardware Testing ==="
	./test_real_hardware.sh

# Create BIOS/UEFI compatibility testing environment
setup-compat-tests:
	@echo "=== Creating BIOS/UEFI Compatibility Tests ==="
	./create_bios_uefi_compat.sh

# Run automated compatibility tests
test-compat: $(DISK_LONGMODE_IMG) debug-builds
	@echo "=== Running Automated Compatibility Tests ==="
	@if [ -d "bios_uefi_compat" ]; then \
		cd bios_uefi_compat && ./automated_compat_test.sh; \
	else \
		echo "Please run 'make setup-compat-tests' first"; \
		exit 1; \
	fi

# Create USB bootable device (requires sudo)
create-usb: $(DISK_LONGMODE_IMG)
	@echo "=== Creating USB Bootable Device ==="
	@echo "This will run the real hardware testing script"
	sudo ./test_real_hardware.sh

# Run comprehensive test suite (QEMU + Compatibility)
test-all: test-qemu test-compat
	@echo "=== All Tests Complete ==="
	@echo "Check test results in:"
	@echo "- qemu_test_results/"
	@echo "- bios_uefi_compat/test_results/"

# Debug with serial output enabled
debug-serial: $(DISK_LONGMODE_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_LONGMODE_IMG) -no-reboot -no-shutdown -s -S -serial stdio

# Debug with GDB integration
debug-gdb: $(DISK_LONGMODE_IMG)
	@echo "Starting QEMU with GDB server on port 1234"
	@echo "Connect with: gdb -ex 'target remote localhost:1234'"
	$(QEMU) -drive format=raw,file=$(DISK_LONGMODE_IMG) -no-reboot -no-shutdown -s -S -nographic

# Performance testing
test-performance: $(DISK_LONGMODE_IMG)
	@echo "=== Performance Testing ==="
	@echo "Testing boot time and memory usage..."
	time $(QEMU) -drive format=raw,file=$(DISK_LONGMODE_IMG) -no-reboot -no-shutdown -nographic & \
	sleep 5 && kill $$!

# Hardware compatibility check
check-hardware:
	@echo "=== Hardware Compatibility Check ==="
	@if [ -f "real_hardware_test/diagnose_hardware.sh" ]; then \
		./real_hardware_test/diagnose_hardware.sh; \
	else \
		echo "Please run 'make setup-real-hardware' first"; \
	fi

# Clean all testing artifacts
clean-tests:
	rm -rf qemu_test_results/
	rm -rf bios_uefi_compat/test_results/
	rm -rf real_hardware_test/

# Clean everything including test setups
clean-all: clean clean-tests
	rm -rf bios_uefi_compat/
	rm -rf real_hardware_test/

# Help target for testing commands
help-testing:
	@echo "IKOS Bootloader Testing Commands:"
	@echo "=================================="
	@echo ""
	@echo "Setup Commands:"
	@echo "  make setup-compat-tests    - Create BIOS/UEFI compatibility tests"
	@echo "  make setup-real-hardware   - Set up real hardware testing tools"
	@echo ""
	@echo "Testing Commands:"
	@echo "  make test-qemu            - Run comprehensive QEMU tests"
	@echo "  make test-compat          - Run BIOS/UEFI compatibility tests"
	@echo "  make test-all             - Run all automated tests"
	@echo "  make test-performance     - Run performance benchmarks"
	@echo ""
	@echo "Hardware Testing:"
	@echo "  make create-usb           - Create bootable USB (requires sudo)"
	@echo "  make check-hardware       - Check hardware compatibility"
	@echo ""
	@echo "Debug Commands:"
	@echo "  make debug-serial         - Debug with serial output"
	@echo "  make debug-gdb            - Debug with GDB integration"
	@echo ""
	@echo "Cleanup Commands:"
	@echo "  make clean-tests          - Clean test results only"
	@echo "  make clean-all            - Clean everything including test setups"
	@echo ""

.PHONY: all test test-enhanced test-compact debug debug-enhanced debug-compact test-protected debug-protected test-protected-compact debug-protected-compact test-elf-loader test-elf-compact debug-elf-loader debug-elf-compact test-longmode debug-longmode clean install-deps debug-builds test-qemu setup-real-hardware setup-compat-tests test-compat create-usb test-all debug-serial debug-gdb test-performance check-hardware clean-tests clean-all help-testing

# QEMU and Real Hardware Test Suite
qemu-test:
	bash qemu_test.sh

real-hardware-test:
	@echo "See real_hardware_test.md for instructions on USB and PXE boot testing."

bios-uefi-test:
	@echo "See real_hardware_test.md for BIOS/UEFI compatibility steps."
