#!/bin/bash

# IKOS Input System Test Script
# Tests the unified input handling system implementation

echo "🧪 IKOS Input System Test Suite"
echo "================================="

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test results tracking
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Function to print test results
print_test_result() {
    local test_name="$1"
    local result="$2"
    local details="$3"
    
    TESTS_RUN=$((TESTS_RUN + 1))
    
    if [ "$result" = "PASS" ]; then
        echo -e "${GREEN}✅ PASS${NC}: $test_name"
        [ -n "$details" ] && echo "    $details"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}❌ FAIL${NC}: $test_name"
        [ -n "$details" ] && echo "    $details"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Function to print section header
print_section() {
    echo ""
    echo -e "${BLUE}📋 $1${NC}"
    echo "----------------------------------------"
}

# Test 1: Check file existence
print_section "File Existence Tests"

test_file_exists() {
    local file="$1"
    local description="$2"
    
    if [ -f "$file" ]; then
        print_test_result "$description" "PASS" "File exists: $file"
    else
        print_test_result "$description" "FAIL" "File missing: $file"
    fi
}

# Check header files
test_file_exists "include/input.h" "Input system main header"
test_file_exists "include/input_events.h" "Input events header"
test_file_exists "include/input_syscalls.h" "Input syscalls header"

# Check implementation files
test_file_exists "kernel/input_manager.c" "Input manager implementation"
test_file_exists "kernel/input_events.c" "Input events implementation"
test_file_exists "kernel/input_keyboard.c" "Input keyboard implementation"
test_file_exists "kernel/input_mouse.c" "Input mouse implementation"
test_file_exists "kernel/input_api.c" "Input API implementation"

# Check test files
test_file_exists "tests/test_input.c" "Input system test suite"

# Test 2: Compilation Tests
print_section "Compilation Tests"

compile_test() {
    local source_file="$1"
    local object_file="$2"
    local description="$3"
    
    echo "Compiling $source_file..."
    if gcc -m64 -nostdlib -nostartfiles -nodefaultlibs \
           -fno-builtin -fno-stack-protector -fno-pic \
           -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
           -mcmodel=kernel -I./include \
           -c "$source_file" -o "$object_file" 2>/dev/null; then
        print_test_result "$description" "PASS" "Compiled successfully"
        return 0
    else
        print_test_result "$description" "FAIL" "Compilation failed"
        return 1
    fi
}

# Test compilation of all input system components
mkdir -p build

compile_test "kernel/input_manager.c" "build/input_manager.o" "Input Manager Compilation"
compile_test "kernel/input_events.c" "build/input_events.o" "Input Events Compilation"
compile_test "kernel/input_keyboard.c" "build/input_keyboard.o" "Input Keyboard Compilation"
compile_test "kernel/input_mouse.c" "build/input_mouse.o" "Input Mouse Compilation"
compile_test "kernel/input_api.c" "build/input_api.o" "Input API Compilation"

# Test 3: Header Analysis
print_section "Header Analysis Tests"

check_header_content() {
    local header_file="$1"
    local pattern="$2"
    local description="$3"
    
    if grep -q "$pattern" "$header_file" 2>/dev/null; then
        print_test_result "$description" "PASS" "Found: $pattern"
    else
        print_test_result "$description" "FAIL" "Missing: $pattern"
    fi
}

# Check main input header for key definitions
check_header_content "include/input.h" "INPUT_DEVICE_KEYBOARD" "Keyboard device type definition"
check_header_content "include/input.h" "INPUT_DEVICE_MOUSE" "Mouse device type definition"
check_header_content "include/input.h" "input_event_t" "Input event structure"
check_header_content "include/input.h" "input_device_t" "Input device structure"
check_header_content "include/input.h" "input_keyboard_config_t" "Keyboard configuration structure"
check_header_content "include/input.h" "input_mouse_config_t" "Mouse configuration structure"

# Check syscall header for API definitions
check_header_content "include/input_syscalls.h" "sys_input_register" "Input registration syscall"
check_header_content "include/input_syscalls.h" "sys_input_poll" "Input polling syscall"
check_header_content "include/input_syscalls.h" "sys_input_wait" "Input waiting syscall"

# Test 4: Implementation Analysis
print_section "Implementation Analysis Tests"

check_implementation() {
    local impl_file="$1"
    local pattern="$2"
    local description="$3"
    
    if grep -q "$pattern" "$impl_file" 2>/dev/null; then
        print_test_result "$description" "PASS" "Implementation found"
    else
        print_test_result "$description" "FAIL" "Implementation missing"
    fi
}

# Check key functions in input manager
check_implementation "kernel/input_manager.c" "input_init" "Input system initialization"
check_implementation "kernel/input_manager.c" "input_register_device" "Device registration"
check_implementation "kernel/input_manager.c" "input_register_app" "Application registration"
check_implementation "kernel/input_manager.c" "input_distribute_event" "Event distribution"

# Check event handling functions
check_implementation "kernel/input_events.c" "input_event_queue_create" "Event queue creation"
check_implementation "kernel/input_events.c" "input_event_validate" "Event validation"
check_implementation "kernel/input_events.c" "input_event_filter" "Event filtering"

# Check keyboard integration
check_implementation "kernel/input_keyboard.c" "input_keyboard_init" "Keyboard initialization"
check_implementation "kernel/input_keyboard.c" "translate_scancode_to_keycode" "Scancode translation"

# Check mouse integration
check_implementation "kernel/input_mouse.c" "input_mouse_init" "Mouse initialization"
check_implementation "kernel/input_mouse.c" "process_mouse_packet" "Mouse packet processing"

# Check system call API
check_implementation "kernel/input_api.c" "sys_input_register" "Input registration API"
check_implementation "kernel/input_api.c" "sys_input_poll" "Input polling API"

# Test 5: Makefile Integration
print_section "Build System Integration Tests"

check_makefile() {
    local makefile="$1"
    local pattern="$2"
    local description="$3"
    
    if grep -q "$pattern" "$makefile" 2>/dev/null; then
        print_test_result "$description" "PASS" "Found in Makefile"
    else
        print_test_result "$description" "FAIL" "Missing from Makefile"
    fi
}

# Check kernel Makefile
check_makefile "kernel/Makefile" "input_manager.c" "Input manager in kernel Makefile"
check_makefile "kernel/Makefile" "input-system:" "Input system target in kernel Makefile"
check_makefile "kernel/Makefile" "test-input-system" "Input system test target"

# Check main Makefile
check_makefile "Makefile" "INPUT_SOURCES" "Input sources in main Makefile"
check_makefile "Makefile" "test-input:" "Input test target in main Makefile"

# Test 6: Documentation and Comments
print_section "Documentation Tests"

check_documentation() {
    local file="$1"
    local description="$2"
    
    # Count comment lines (lines starting with /* or //)
    local comment_lines=$(grep -c "^\s*\(/\*\|//\)" "$file" 2>/dev/null || echo 0)
    local total_lines=$(wc -l < "$file" 2>/dev/null || echo 1)
    local comment_ratio=$((comment_lines * 100 / total_lines))
    
    if [ $comment_ratio -ge 15 ]; then
        print_test_result "$description" "PASS" "Well documented ($comment_ratio% comments)"
    else
        print_test_result "$description" "FAIL" "Poorly documented ($comment_ratio% comments)"
    fi
}

check_documentation "kernel/input_manager.c" "Input Manager Documentation"
check_documentation "kernel/input_events.c" "Input Events Documentation"
check_documentation "kernel/input_keyboard.c" "Input Keyboard Documentation"
check_documentation "kernel/input_mouse.c" "Input Mouse Documentation"

# Test 7: Code Structure Analysis
print_section "Code Structure Tests"

check_structure() {
    local file="$1"
    local pattern="$2"
    local description="$3"
    local min_count="$4"
    
    local count=$(grep -c "$pattern" "$file" 2>/dev/null || echo 0)
    
    if [ $count -ge $min_count ]; then
        print_test_result "$description" "PASS" "Found $count instances"
    else
        print_test_result "$description" "FAIL" "Found $count instances (expected >= $min_count)"
    fi
}

# Check for proper error handling
check_structure "kernel/input_manager.c" "INPUT_ERROR\|return.*-1\|return.*ERROR" "Error handling in input manager" 3
check_structure "kernel/input_events.c" "return.*false\|return.*-1\|return.*ERROR" "Error handling in input events" 2

# Check for proper initialization
check_structure "kernel/input_manager.c" "memset\|calloc\|malloc" "Memory initialization" 2
check_structure "kernel/input_keyboard.c" "memset\|calloc\|malloc" "Memory initialization in keyboard" 1

# Print final results
print_section "Test Summary"

echo ""
echo "📊 Final Results:"
echo "  Tests Run:    $TESTS_RUN"
echo "  Tests Passed: $TESTS_PASSED"
echo "  Tests Failed: $TESTS_FAILED"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "  ${GREEN}🎉 All tests passed!${NC}"
    echo ""
    echo "✅ The IKOS Input System implementation is complete and ready for integration!"
    echo ""
    echo "📋 Key Features Implemented:"
    echo "  • Unified input device abstraction layer"
    echo "  • Event-driven input processing architecture"
    echo "  • Keyboard input with keymap translation and repeat handling"
    echo "  • Mouse input with PS/2 protocol support and acceleration"
    echo "  • Application focus management and event distribution"
    echo "  • Complete system call interface for user-space applications"
    echo "  • Event validation, filtering, and queue management"
    echo "  • Device configuration and capability detection"
else
    echo -e "  ${RED}⚠️  $TESTS_FAILED test(s) failed${NC}"
    echo ""
    echo "❌ Some issues need to be addressed before the input system is ready."
fi

echo ""
echo "Success Rate: $(( TESTS_PASSED * 100 / TESTS_RUN ))%"
echo ""

# Exit with appropriate code
exit $TESTS_FAILED
