#!/bin/bash
# Build user-space programs for IKOS

# Compiler settings for user-space
USER_CC="gcc"
USER_CFLAGS="-m64 -nostdlib -nostartfiles -nodefaultlibs -fno-builtin -static -O2"
USER_LDFLAGS="-static -nostdlib"

# Output directory
OUTPUT_DIR="user/bin"
mkdir -p $OUTPUT_DIR

echo "Building IKOS user-space programs..."

# Build hello_world
echo "Building hello_world..."
$USER_CC $USER_CFLAGS -c user/hello_world.c -o $OUTPUT_DIR/hello_world.o
if [ $? -eq 0 ]; then
    # Create a simple linker script for user programs
    cat > $OUTPUT_DIR/user.ld << 'EOF'
ENTRY(_start)

SECTIONS
{
    . = 0x400000;
    
    .text : {
        *(.text)
        *(.text.*)
    }
    
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }
    
    .data : {
        *(.data)
        *(.data.*)
    }
    
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    
    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
}
EOF

    # Link the program
    ld $USER_LDFLAGS -T $OUTPUT_DIR/user.ld $OUTPUT_DIR/hello_world.o -o $OUTPUT_DIR/hello_world
    
    if [ $? -eq 0 ]; then
        echo "Successfully built hello_world"
        echo "Size: $(ls -lh $OUTPUT_DIR/hello_world | awk '{print $5}')"
        
        # Display binary info
        file $OUTPUT_DIR/hello_world
        readelf -h $OUTPUT_DIR/hello_world | head -20
    else
        echo "Failed to link hello_world"
        exit 1
    fi
else
    echo "Failed to compile hello_world"
    exit 1
fi

echo "User-space build complete!"
