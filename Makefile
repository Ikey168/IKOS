# IKOS Bootloader Makefile
# Builds the bootloader for real mode initialization and ELF kernel loading

# Assembler and tools
ASM = nasm
QEMU = qemu-system-x86_64

# Directories
BOOT_DIR = boot
BUILD_DIR = build

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

# Default target
all: $(DISK_IMG) $(DISK_COMPACT_IMG) $(DISK_PROTECTED_COMPACT_IMG) $(DISK_ELF_COMPACT_IMG) $(DISK_LONGMODE_IMG) $(DISK_LONGMODE_IMG)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

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

# Test basic bootloader in QEMU
test: $(DISK_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_IMG) -no-reboot -no-shutdown -nographic

# Test enhanced bootloader in QEMU
test-enhanced: $(DISK_ENHANCED_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_ENHANCED_IMG) -no-reboot -no-shutdown

# Test with debugging
debug: $(DISK_IMG)
	$(QEMU) -drive format=raw,file=$(DISK_IMG) -no-reboot -no-shutdown -s -S

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
	sudo apt-get install -y nasm qemu-system-x86

.PHONY: all test test-enhanced test-compact debug debug-enhanced debug-compact test-protected debug-protected test-protected-compact debug-protected-compact test-elf-loader test-elf-compact debug-elf-loader debug-elf-compact test-longmode debug-longmode clean install-deps
