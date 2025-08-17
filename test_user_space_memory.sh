#!/bin/bash

# IKOS User Space Memory Management Test Runner
# Builds and executes comprehensive tests for USMM

set -e

echo "IKOS User Space Memory Management Test Suite"
echo "============================================="

# Configuration
BUILD_DIR="build"
TESTS_DIR="tests"
KERNEL_DIR="kernel"
INCLUDE_DIR="include"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    log_info "Creating build directory: $BUILD_DIR"
    mkdir -p "$BUILD_DIR"
fi

# Compilation flags
CFLAGS="-Wall -Wextra -std=c11 -I$INCLUDE_DIR -g -O0"
LDFLAGS=""

# Test if we have a proper compiler
if ! command -v gcc &> /dev/null; then
    log_error "GCC compiler not found. Please install GCC."
    exit 1
fi

log_info "Using GCC version: $(gcc --version | head -n1)"

# ========================== Build Tests ==========================

log_info "Building User Space Memory Management components..."

# Build the main test suite
log_info "Building functional test suite..."
gcc $CFLAGS -o "$BUILD_DIR/test_usmm_functional" \
    "$TESTS_DIR/test_user_space_memory.c" \
    "$TESTS_DIR/usmm_stubs.c" \
    $LDFLAGS

if [ $? -eq 0 ]; then
    log_success "Functional test suite built successfully"
else
    log_error "Failed to build functional test suite"
    exit 1
fi

# Build the performance test suite
log_info "Building performance test suite..."
gcc $CFLAGS -o "$BUILD_DIR/test_usmm_performance" \
    "$TESTS_DIR/test_user_space_memory_performance.c" \
    "$TESTS_DIR/usmm_stubs.c" \
    $LDFLAGS

if [ $? -eq 0 ]; then
    log_success "Performance test suite built successfully"
else
    log_error "Failed to build performance test suite"
    exit 1
fi

# ========================== Run Tests ==========================

log_info "Running User Space Memory Management tests..."

# Check if we should run tests
RUN_TESTS=${1:-"yes"}
if [ "$RUN_TESTS" = "build-only" ]; then
    log_info "Build-only mode requested, skipping test execution"
    exit 0
fi

# Run functional tests
log_info "Running functional tests..."
echo "----------------------------------------"
if [ -x "$BUILD_DIR/test_usmm_functional" ]; then
    ./"$BUILD_DIR/test_usmm_functional"
    FUNCTIONAL_RESULT=$?
    echo "----------------------------------------"
    
    if [ $FUNCTIONAL_RESULT -eq 0 ]; then
        log_success "Functional tests passed"
    else
        log_error "Functional tests failed with code $FUNCTIONAL_RESULT"
    fi
else
    log_error "Functional test executable not found or not executable"
    FUNCTIONAL_RESULT=1
fi

# Run performance tests
log_info "Running performance tests..."
echo "----------------------------------------"
if [ -x "$BUILD_DIR/test_usmm_performance" ]; then
    ./"$BUILD_DIR/test_usmm_performance"
    PERFORMANCE_RESULT=$?
    echo "----------------------------------------"
    
    if [ $PERFORMANCE_RESULT -eq 0 ]; then
        log_success "Performance tests completed"
    else
        log_warning "Performance tests completed with issues (code $PERFORMANCE_RESULT)"
    fi
else
    log_error "Performance test executable not found or not executable"
    PERFORMANCE_RESULT=1
fi

# ========================== Static Analysis ==========================

log_info "Running static analysis on USMM code..."

# Check for common issues in header file
if [ -f "$INCLUDE_DIR/user_space_memory.h" ]; then
    log_info "Analyzing header file..."
    
    # Check for missing include guards
    if ! grep -q "#ifndef.*USER_SPACE_MEMORY_H" "$INCLUDE_DIR/user_space_memory.h"; then
        log_warning "Header file may be missing include guards"
    fi
    
    # Check for proper struct definitions
    STRUCT_COUNT=$(grep -c "typedef struct" "$INCLUDE_DIR/user_space_memory.h" || echo "0")
    log_info "Found $STRUCT_COUNT struct definitions in header"
    
    # Check for function declarations
    FUNC_COUNT=$(grep -c "^[a-zA-Z_][a-zA-Z0-9_]*.*(" "$INCLUDE_DIR/user_space_memory.h" | head -1)
    log_info "Found function declarations in header"
    
    log_success "Header file analysis completed"
else
    log_warning "Header file not found: $INCLUDE_DIR/user_space_memory.h"
fi

# Analyze implementation files
for impl_file in "$KERNEL_DIR/user_space_memory_manager.c" \
                 "$KERNEL_DIR/shared_memory.c" \
                 "$KERNEL_DIR/copy_on_write.c"; do
    if [ -f "$impl_file" ]; then
        log_info "Analyzing $(basename "$impl_file")..."
        
        # Check for basic function definitions
        FUNC_COUNT=$(grep -c "^[a-zA-Z_][a-zA-Z0-9_]*.*{" "$impl_file" || echo "0")
        log_info "  Found $FUNC_COUNT function definitions"
        
        # Check for error handling
        ERROR_COUNT=$(grep -c "return.*-.*;" "$impl_file" || echo "0")
        log_info "  Found $ERROR_COUNT error return statements"
        
        # Check for memory allocations
        ALLOC_COUNT=$(grep -c "malloc\|kmalloc\|alloc" "$impl_file" || echo "0")
        FREE_COUNT=$(grep -c "free\|kfree" "$impl_file" || echo "0")
        log_info "  Found $ALLOC_COUNT allocations and $FREE_COUNT deallocations"
        
    else
        log_warning "Implementation file not found: $impl_file"
    fi
done

# ========================== Memory Analysis ==========================

log_info "Checking for potential memory issues..."

# Look for potential memory leaks in test files
for test_file in "$TESTS_DIR/test_user_space_memory.c" \
                 "$TESTS_DIR/test_user_space_memory_performance.c"; do
    if [ -f "$test_file" ]; then
        log_info "Analyzing $(basename "$test_file") for memory issues..."
        
        MALLOC_COUNT=$(grep -c "malloc\|sys_mmap" "$test_file" || echo "0")
        FREE_COUNT=$(grep -c "free\|sys_munmap\|mm_free" "$test_file" || echo "0")
        
        log_info "  Allocations: $MALLOC_COUNT, Deallocations: $FREE_COUNT"
        
        if [ $MALLOC_COUNT -gt $FREE_COUNT ]; then
            log_warning "  Potential memory leaks detected (more allocations than deallocations)"
        elif [ $FREE_COUNT -gt $MALLOC_COUNT ]; then
            log_warning "  Potential double-free detected (more deallocations than allocations)"
        else
            log_success "  Memory allocation/deallocation appears balanced"
        fi
    fi
done

# ========================== Code Coverage Analysis ==========================

log_info "Analyzing code coverage..."

# Count test functions
FUNCTIONAL_TESTS=$(grep -c "^int test_.*(" "$TESTS_DIR/test_user_space_memory.c" || echo "0")
PERFORMANCE_TESTS=$(grep -c "^int test_.*(" "$TESTS_DIR/test_user_space_memory_performance.c" || echo "0")

log_info "Test coverage:"
log_info "  Functional tests: $FUNCTIONAL_TESTS"
log_info "  Performance tests: $PERFORMANCE_TESTS"
log_info "  Total tests: $((FUNCTIONAL_TESTS + PERFORMANCE_TESTS))"

# Estimate API coverage
if [ -f "$INCLUDE_DIR/user_space_memory.h" ]; then
    API_FUNCTIONS=$(grep -c "^[a-zA-Z_][a-zA-Z0-9_]*.*(" "$INCLUDE_DIR/user_space_memory.h" || echo "0")
    TESTED_FUNCTIONS=$(grep -o "sys_[a-zA-Z_][a-zA-Z0-9_]*\|mm_[a-zA-Z_][a-zA-Z0-9_]*\|usmm_[a-zA-Z_][a-zA-Z0-9_]*" \
                      "$TESTS_DIR"/*.c | sort -u | wc -l)
    
    log_info "API coverage estimation:"
    log_info "  API functions: $API_FUNCTIONS"
    log_info "  Tested functions: $TESTED_FUNCTIONS"
    
    if [ $API_FUNCTIONS -gt 0 ]; then
        COVERAGE=$((TESTED_FUNCTIONS * 100 / API_FUNCTIONS))
        log_info "  Estimated coverage: ${COVERAGE}%"
    fi
fi

# ========================== Final Report ==========================

echo ""
log_info "Test Summary:"
echo "============================================="

if [ $FUNCTIONAL_RESULT -eq 0 ]; then
    log_success "✓ Functional tests: PASSED"
else
    log_error "✗ Functional tests: FAILED"
fi

if [ $PERFORMANCE_RESULT -eq 0 ]; then
    log_success "✓ Performance tests: COMPLETED"
else
    log_warning "△ Performance tests: COMPLETED WITH ISSUES"
fi

log_success "✓ Static analysis: COMPLETED"
log_success "✓ Memory analysis: COMPLETED"
log_success "✓ Code coverage analysis: COMPLETED"

echo ""
if [ $FUNCTIONAL_RESULT -eq 0 ]; then
    log_success "User Space Memory Management testing completed successfully!"
    echo ""
    log_info "Next steps:"
    log_info "1. Review any warnings from static analysis"
    log_info "2. Commit the USMM implementation"
    log_info "3. Create pull request for Issue #29"
    exit 0
else
    log_error "User Space Memory Management testing completed with failures!"
    echo ""
    log_info "Please review the test failures before proceeding."
    exit 1
fi
