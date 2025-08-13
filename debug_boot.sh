#!/bin/bash
# IKOS Boot Debugging Script
# Sets up QEMU with GDB debugging for step-by-step boot analysis

echo "=== IKOS Boot Process Debugging ==="
echo "This script starts QEMU with GDB debugging enabled"
echo ""

# Check if build exists
if [ ! -f "build/ikos_longmode.img" ]; then
    echo "Error: Boot image not found. Please run 'make' first."
    exit 1
fi

# Create GDB script for automated debugging
cat > debug.gdb << 'EOF'
# IKOS Boot Debugging GDB Script
target remote localhost:1234

# Set up breakpoints at key boot stages
# Note: These addresses are relative to the loaded code
define setup_breakpoints
    # Boot sector start
    break *0x7c00
    
    # A20 line enable
    break *0x7c20
    
    # Protected mode entry
    break *0x7c40
    
    # Long mode setup
    break *0x7c60
    
    # Paging setup
    break *0x7c80
end

# Display current state
define show_state
    info registers
    x/10i $pc
    echo \n--- Stack ---\n
    x/8x $sp
    echo \n--- Next Instructions ---\n
    x/5i $pc
end

# Step through boot process
define debug_boot
    echo === Boot Process Debugging ===\n
    setup_breakpoints
    continue
    
    echo \n=== Stage 1: Boot Sector Start ===\n
    show_state
    echo Press Enter to continue to A20 enable...
    shell read
    continue
    
    echo \n=== Stage 2: A20 Line Enable ===\n
    show_state
    echo Press Enter to continue to protected mode...
    shell read
    continue
    
    echo \n=== Stage 3: Protected Mode Entry ===\n
    show_state
    echo Press Enter to continue to long mode setup...
    shell read
    continue
    
    echo \n=== Stage 4: Long Mode Setup ===\n
    show_state
    echo Press Enter to continue to paging setup...
    shell read
    continue
    
    echo \n=== Stage 5: Paging Setup ===\n
    show_state
    echo Press Enter to finish...
    shell read
    continue
end

# Memory analysis commands
define analyze_memory
    echo \n=== Memory Analysis ===\n
    echo PML4 Table (0x1000):
    x/4x 0x1000
    echo PDPT Table (0x2000):
    x/4x 0x2000
    echo PD Table (0x3000):
    x/4x 0x3000
    echo VGA Buffer (first 32 bytes):
    x/32c 0xb8000
end

# Register analysis
define analyze_registers
    echo \n=== Register Analysis ===\n
    echo Control Registers:
    info registers cr0 cr3 cr4
    echo Segment Registers:
    info registers cs ds es ss
    echo General Purpose:
    info registers eax ebx ecx edx esi edi esp ebp
end

# Print help
define help_debug
    echo Available commands:
    echo   debug_boot     - Step through boot process
    echo   analyze_memory - Show memory state
    echo   analyze_registers - Show register state
    echo   show_state     - Show current CPU state
    echo   setup_breakpoints - Set debugging breakpoints
end

echo IKOS Boot Debugger Ready
echo Type 'help_debug' for available commands
echo Type 'debug_boot' to start step-by-step debugging
EOF

echo "Starting QEMU with GDB debugging..."
echo "QEMU will wait for GDB connection on port 1234"
echo ""
echo "In another terminal, run:"
echo "  gdb -x debug.gdb"
echo ""
echo "Or connect manually:"
echo "  gdb"
echo "  (gdb) target remote localhost:1234"
echo ""

# Start QEMU with debugging
qemu-system-x86_64 \
    -fda build/ikos_longmode.img \
    -boot a \
    -S \
    -s \
    -nographic \
    -monitor stdio \
    -serial file:debug_serial.log \
    -d int,cpu_reset \
    -D debug_qemu.log

echo ""
echo "QEMU debugging session ended."
echo "Check debug_serial.log for serial output"
echo "Check debug_qemu.log for QEMU debug info"
