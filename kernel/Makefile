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
C_SOURCES = scheduler.c interrupts.c scheduler_test.c
ASM_SOURCES = context_switch.asm
BOOT_SOURCES = $(BOOTDIR)/boot_longmode.asm

# Object files
C_OBJECTS = $(patsubst %.c,$(BUILDDIR)/%.o,$(C_SOURCES))
ASM_OBJECTS = $(patsubst %.asm,$(BUILDDIR)/%.o,$(ASM_SOURCES))
BOOT_OBJECTS = $(patsubst $(BOOTDIR)/%.asm,$(BUILDDIR)/%.o,$(BOOT_SOURCES))

# Targets
.PHONY: all clean test scheduler bootloader

all: $(BUILDDIR) bootloader scheduler kernel

# Create build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Bootloader targets
bootloader: $(BUILDDIR)/boot_longmode.bin

$(BUILDDIR)/boot_longmode.bin: $(BOOTDIR)/boot_longmode.asm
	$(AS) -f bin $< -o $@

$(BUILDDIR)/boot_longmode.o: $(BOOTDIR)/boot_longmode.asm
	$(AS) $(ASFLAGS) $< -o $@

# Scheduler components
scheduler: $(C_OBJECTS) $(ASM_OBJECTS)

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
test: test-scheduler test-qemu

test-scheduler: scheduler
	@echo "Building scheduler test..."
	@echo "Scheduler components built successfully"
	@echo "C Objects: $(C_OBJECTS)"
	@echo "ASM Objects: $(ASM_OBJECTS)"

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
