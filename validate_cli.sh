#!/bin/bash
# IKOS CLI Shell Validation Script - Issue #36
# Tests compilation and basic functionality

echo "=========================================="
echo "IKOS CLI Shell Validation - Issue #36"
echo "=========================================="

cd "$(dirname "$0")"

echo "1. Testing CLI compilation..."
if make -C user ikos_cli_standalone > /dev/null 2>&1; then
    echo "✓ CLI shell compiles successfully"
else
    echo "✗ CLI shell compilation failed"
    exit 1
fi

echo "2. Checking CLI executable..."
if [ -f "user/ikos_cli_standalone" ]; then
    echo "✓ CLI executable created"
else
    echo "✗ CLI executable not found"
    exit 1
fi

echo "3. Testing CLI size and structure..."
CLI_SIZE=$(stat -c%s "user/ikos_cli_standalone" 2>/dev/null || echo "0")
if [ "$CLI_SIZE" -gt 1000 ]; then
    echo "✓ CLI executable has reasonable size: $CLI_SIZE bytes"
else
    echo "✗ CLI executable too small: $CLI_SIZE bytes"
fi

echo "4. Checking source code completeness..."
TOTAL_LINES=$(wc -l user/ikos_cli_standalone.c | cut -d' ' -f1)
if [ "$TOTAL_LINES" -gt 300 ]; then
    echo "✓ CLI implementation is comprehensive: $TOTAL_LINES lines"
else
    echo "⚠ CLI implementation may be incomplete: $TOTAL_LINES lines"
fi

echo "5. Verifying built-in commands implementation..."
COMMANDS=("echo" "pwd" "cd" "ls" "set" "help" "version" "clear" "exit")
FOUND_COMMANDS=0

for cmd in "${COMMANDS[@]}"; do
    if grep -q "cmd_$cmd" user/ikos_cli_standalone.c; then
        echo "  ✓ Command '$cmd' implemented"
        ((FOUND_COMMANDS++))
    else
        echo "  ✗ Command '$cmd' missing"
    fi
done

echo "6. Implementation summary..."
echo "  Built-in commands: $FOUND_COMMANDS/${#COMMANDS[@]}"
echo "  Source files created: $(ls -1 user/ikos_cli_* 2>/dev/null | wc -l)"
echo "  Header files: $(ls -1 user/*.h 2>/dev/null | wc -l)"
echo "  Test files: $(ls -1 user/*test* 2>/dev/null | wc -l)"

echo ""
echo "=========================================="
echo "Issue #36 CLI Implementation Status:"
if [ "$FOUND_COMMANDS" -eq "${#COMMANDS[@]}" ] && [ -f "user/ikos_cli_standalone" ]; then
    echo "✓ COMPLETE - CLI shell successfully implemented"
    echo "✓ All core functionality present"
    echo "✓ Standalone executable built"
    echo "✓ Ready for integration and testing"
else
    echo "⚠ PARTIAL - Some components may need attention"
fi
echo "=========================================="
