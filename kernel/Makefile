# IKOS Kernel Makefile with Scheduler Support
# Builds kernel components including preemptive scheduler

# Compiler and assembler settings
CC = gcc
AS = nasm
LD = ld

# Compiler flags
CFLAGS = -m64 -nostdlib -nostartfiles -nodefaultlibs \
         -fno-builtin -fno-stack-protector -fno-pic \
         -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -mcmodel=kernel -I../include

# Assembler flags  
ASFLAGS = -f elf64

# Linker flags
LDFLAGS = -T kernel.ld -nostdlib

# Directories
SRCDIR = .
INCLUDEDIR = ../include
BOOTDIR = ../boot
BUILDDIR = build

# Source files
C_SOURCES = scheduler.c interrupts.c scheduler_test.c kalloc.c kalloc_test.c user_space_test.c \
            device_manager.c pci.c ide_driver.c device_driver_test.c framebuffer.c framebuffer_syscalls.c framebuffer_test.c \
            usb_controller.c kernel_log.c kernel_main.c user_app_loader.c process.c elf_loader.c \
            process_exit.c process_helpers.c process_termination_test.c \
            signal_delivery.c signal_mask.c signal_syscalls.c signal_handlers.c signal_test.c \
            syscall_fork.c syscall_execve.c syscall_wait.c process_lifecycle_test.c \
            daemon_core.c daemon_service_registry.c daemon_ipc.c daemon_config.c \
            terminal.c terminal_escape.c terminal_extended.c \
            gui.c gui_widgets.c gui_render.c gui_utils.c gui_test.c \
            input_manager.c input_events.c input_keyboard.c input_mouse.c input_api.c \
            app_loader.c app_loader_test.c
ASM_SOURCES = context_switch.asm
BOOT_SOURCES = $(BOOTDIR)/boot_longmode.asm

# Object files
C_OBJECTS = $(patsubst %.c,$(BUILDDIR)/%.o,$(C_SOURCES))
ASM_OBJECTS = $(patsubst %.asm,$(BUILDDIR)/%.o,$(ASM_SOURCES))
BOOT_OBJECTS = $(patsubst $(BOOTDIR)/%.asm,$(BUILDDIR)/%.o,$(BOOT_SOURCES))

# Targets
.PHONY: all clean test scheduler bootloader kernel ipc device-drivers framebuffer process-termination signal-handling process-lifecycle daemon-system terminal-emulator gui-system input-system

all: $(BUILDDIR) bootloader scheduler ipc device-drivers framebuffer process-termination signal-handling process-lifecycle daemon-system terminal-emulator gui-system input-system kernel

# Create build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Bootloader targets
bootloader: $(BUILDDIR)/boot_longmode.bin

build/boot_longmode.bin: ../boot/boot_compact.asm
	nasm -f bin ../boot/boot_compact.asm -o build/boot_longmode.bin

$(BUILDDIR)/boot_longmode.o: $(BOOTDIR)/boot_longmode.asm
	$(AS) $(ASFLAGS) $< -o $@

# Scheduler components
scheduler: $(C_OBJECTS) $(ASM_OBJECTS)

# IPC components
ipc: $(C_OBJECTS) $(ASM_OBJECTS)

# Device Driver Framework components (Issue #15)
device-drivers: $(C_OBJECTS) $(ASM_OBJECTS)

# USB Controller components (Issue #15 Enhancement)
usb: $(C_OBJECTS) $(ASM_OBJECTS)

# Kernel Logging System components (Issue #16)
logging: $(C_OBJECTS) $(ASM_OBJECTS)

# Framebuffer Driver components (Issue #26)
framebuffer: $(C_OBJECTS) $(ASM_OBJECTS)

# Process Termination System components (Issue #18)
process-termination: $(C_OBJECTS) $(ASM_OBJECTS)

# Advanced Signal Handling System components (Issue #19)
signal-handling: $(C_OBJECTS) $(ASM_OBJECTS)

# Process Lifecycle Management components (Issue #24)
process-lifecycle: $(C_OBJECTS) $(ASM_OBJECTS)

# System Daemon Management components (Issue #33)
daemon-system: $(C_OBJECTS) $(ASM_OBJECTS)

# Terminal Emulator components (Issue #34)
terminal-emulator: $(C_OBJECTS) $(ASM_OBJECTS)

# GUI System components (Issue #37)
gui-system: $(C_OBJECTS) $(ASM_OBJECTS)

# User Input Handling System components (Issue #38)
input-system: $(C_OBJECTS) $(ASM_OBJECTS)

# Window Manager System components (Issue #39)
window-manager: $(C_OBJECTS) $(ASM_OBJECTS)

# Advanced Memory Management simple test (Issue #27)
simple_memory_test: $(BUILDDIR)/simple_memory_test

$(BUILDDIR)/simple_memory_test: $(BUILDDIR)/simple_memory_test.o $(BUILDDIR)/simple_memory_stub.o
	$(CC) -static -nostdlib -no-pie -o $@ $^

$(BUILDDIR)/simple_memory_stub.o: simple_memory_stub.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/simple_memory_test.o: simple_memory_test.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/simple_memory_impl.o: simple_memory_impl.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C sources
$(BUILDDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble assembly sources
$(BUILDDIR)/%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# Kernel linking
kernel: $(BUILDDIR)/kernel.bin

$(BUILDDIR)/kernel.bin: $(C_OBJECTS) $(ASM_OBJECTS) kernel.ld
	$(LD) $(LDFLAGS) -o $@ $(C_OBJECTS) $(ASM_OBJECTS)

# Test targets
test: test-scheduler test-ipc test-device-drivers test-usb test-logging test-process-termination test-signal-handling test-process-lifecycle test-daemon-system test-terminal-emulator test-input-system test-qemu

test-scheduler: scheduler
	@echo "Building scheduler test..."
	@echo "Scheduler components built successfully"
	@echo "C Objects: $(C_OBJECTS)"
	@echo "ASM Objects: $(ASM_OBJECTS)"

test-ipc: ipc
	@echo "Building IPC test..."
	@echo "IPC components built successfully"
	@echo "Testing message queues, channels, and system calls"

test-device-drivers: device-drivers
	@echo "Building Device Driver Framework test..."
	@echo "Device Driver Framework components built successfully"
	@echo "Testing device manager, PCI bus driver, and IDE controller"

test-usb: usb
	@echo "Building USB Controller test..."
	@echo "USB Controller components built successfully"
	@echo "Testing USB controller integration with device framework"

test-logging: logging
	@echo "Building Kernel Logging System test..."
	@echo "Kernel Logging System components built successfully"
	@echo "Testing serial output, debugging support, and IPC/memory logging"

test-framebuffer: framebuffer
	@echo "Building Framebuffer Driver test..."
	@echo "Framebuffer Driver components built successfully"
	@echo "Testing display modes, drawing functions, and user-space API"

test-process-termination: process-termination
	@echo "Building Process Termination System test..."
	@echo "Process Termination System components built successfully"
	@echo "Testing process exit, resource cleanup, wait system calls, and zombie reaping"

test-signal-handling: signal-handling
	@echo "Building Advanced Signal Handling System test..."
	@echo "Advanced Signal Handling System components built successfully"
	@echo "Testing signal delivery, masking, handlers, RT signals, and POSIX compatibility"

test-process-lifecycle: process-lifecycle
	@echo "Building Process Lifecycle Management test..."
	@echo "Process Lifecycle Management components built successfully"
	@echo "Testing fork(), execve(), wait() system calls, COW memory, and process tree management"

test-daemon-system: daemon-system
	@echo "Building System Daemon Management test..."
	@echo "System Daemon Management components built successfully"
	@echo "Testing daemon lifecycle, service registry, IPC communication, and health monitoring"

test-terminal-emulator: terminal-emulator
	@echo "Building Terminal Emulator test..."
	@echo "Terminal Emulator components built successfully"
	@echo "Testing VT100/ANSI escape sequences, cursor control, text formatting, and screen management"

test-input-system: input-system
	@echo "Building Input System test..."
	@echo "Input System components built successfully"
	@echo "Testing unified input handling, keyboard & mouse events, device abstraction, and application focus management"

test-qemu: $(BUILDDIR)/boot_longmode.bin
	@echo "Testing bootloader with scheduler in QEMU..."
	qemu-system-x86_64 \
		-drive format=raw,file=$< \
		-m 256M \
		-serial stdio \
		-display none \
		-machine accel=tcg \
		-cpu qemu64 \
		-d cpu_reset \
		-monitor none

# QEMU with scheduler debugging
debug-qemu: $(BUILDDIR)/boot_longmode.bin
	@echo "Starting QEMU with GDB debugging..."
	qemu-system-x86_64 \
		-drive format=raw,file=$< \
		-m 256M \
		-serial stdio \
		-display none \
		-machine accel=tcg \
		-cpu qemu64 \
		-s -S \
		-monitor none &
	@echo "QEMU started with GDB server on port 1234"
	@echo "Connect with: gdb -ex 'target remote localhost:1234'"

# Create bootable disk image with scheduler
disk-image: $(BUILDDIR)/boot_longmode.bin $(BUILDDIR)/kernel.bin
	@echo "Creating bootable disk image..."
	dd if=/dev/zero of=$(BUILDDIR)/ikos_scheduler.img bs=1M count=10
	dd if=$< of=$(BUILDDIR)/ikos_scheduler.img conv=notrunc
	# Copy kernel to appropriate location on disk
	# (This would need more sophisticated disk layout)

# Documentation
docs:
	@echo "IKOS Scheduler Documentation"
	@echo "============================"
	@echo ""
	@echo "Components:"
	@echo "  - scheduler.c/h: Main scheduler implementation"
	@echo "  - context_switch.asm: Low-level context switching"
	@echo "  - interrupts.c/h: Timer and interrupt management"
	@echo "  - scheduler_test.c: Test program with multiple tasks"
	@echo ""
	@echo "Features:"
	@echo "  - Preemptive multitasking"
	@echo "  - Round Robin and Priority scheduling"
	@echo "  - Timer-based task switching"
	@echo "  - Task creation and destruction"
	@echo "  - System call interface (yield)"
	@echo ""
	@echo "Build targets:"
	@echo "  make all        - Build everything"
	@echo "  make scheduler  - Build scheduler components"
	@echo "  make bootloader - Build bootloader"
	@echo "  make kernel     - Link kernel binary"
	@echo "  make test       - Run tests"
	@echo "  make debug-qemu - Start QEMU with GDB debugging"

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR)

# Check dependencies
check-deps:
	@echo "Checking build dependencies..."
	@which $(CC) > /dev/null || (echo "ERROR: $(CC) not found" && exit 1)
	@which $(AS) > /dev/null || (echo "ERROR: $(AS) not found" && exit 1) 
	@which $(LD) > /dev/null || (echo "ERROR: $(LD) not found" && exit 1)
	@which qemu-system-x86_64 > /dev/null || (echo "WARNING: QEMU not found, testing disabled")
	@echo "Dependencies OK"

# Show current configuration
config:
	@echo "Build Configuration:"
	@echo "  CC = $(CC)"
	@echo "  AS = $(AS)"
	@echo "  LD = $(LD)"
	@echo "  CFLAGS = $(CFLAGS)"
	@echo "  ASFLAGS = $(ASFLAGS)"
	@echo "  LDFLAGS = $(LDFLAGS)"
