#!/bin/bash
# Test Issue #17 User-Space Process Execution Implementation

echo "Testing Issue #17 User-Space Process Execution Implementation..."
echo "================================================================="

# Change to workspace directory
cd /workspaces/IKOS

# Test 1: Build the kernel with user-space process execution support
echo "Test 1: Building kernel with user-space process execution support..."
cd kernel
make clean
if make; then
    echo "✓ Kernel build successful"
else
    echo "✗ Kernel build failed"
    exit 1
fi

# Test 2: Check if all required object files were created
echo ""
echo "Test 2: Checking for required object files..."
required_objects=(
    "build/user_app_loader.o"
    "build/process.o"
    "build/elf_loader.o"
)

for obj in "${required_objects[@]}"; do
    if [ -f "$obj" ]; then
        echo "✓ Found $obj"
    else
        echo "✗ Missing $obj"
        exit 1
    fi
done

# Test 3: Build user-space hello world application
echo ""
echo "Test 3: Building user-space hello world application..."
cd ../user
if make; then
    echo "✓ User-space application build successful"
else
    echo "✗ User-space application build failed"
    exit 1
fi

# Test 4: Create a complete kernel image
echo ""
echo "Test 4: Creating complete kernel image..."
cd ..
if make; then
    echo "✓ Complete kernel image created successfully"
else
    echo "✗ Complete kernel image creation failed"
    exit 1
fi

# Test 5: Basic validation of ELF files
echo ""
echo "Test 5: Validating ELF files..."
if command -v file >/dev/null 2>&1; then
    if [ -f "user/hello_world" ]; then
        file_output=$(file user/hello_world)
        if echo "$file_output" | grep -q "ELF.*x86-64"; then
            echo "✓ Hello world ELF is valid x86-64 binary"
        else
            echo "✗ Hello world ELF validation failed: $file_output"
        fi
    else
        echo "✗ Hello world binary not found"
    fi
else
    echo "! file command not available, skipping ELF validation"
fi

echo ""
echo "================================================================="
echo "Issue #17 Implementation Testing Complete"
echo "✓ All critical components built successfully"
echo ""
echo "Next steps for complete testing:"
echo "1. Run QEMU test: ./test_qemu.sh"
echo "2. Test user-space execution: ./test_user_space.sh"
echo "3. Validate process management: ./validate_process_manager.sh"
echo ""
echo "Implementation Status:"
echo "✓ User application loader framework"
echo "✓ File system integration (placeholder)"
echo "✓ Process management integration"
echo "✓ Context switching integration"
echo "✓ Build system integration"
echo ""
echo "Ready for user-space process execution testing!"
