#!/bin/bash
# IKOS Advanced Memory Management Test Script - Issue #27
# Builds and tests all advanced memory management components

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build"
LOG_FILE="$BUILD_DIR/memory_test.log"
VERBOSE=0

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}[$(date '+%H:%M:%S')] ${message}${NC}"
}

print_info() {
    print_status "$BLUE" "$1"
}

print_success() {
    print_status "$GREEN" "$1"
}

print_warning() {
    print_status "$YELLOW" "$1"
}

print_error() {
    print_status "$RED" "$1"
}

# Function to run command with logging
run_command() {
    local cmd="$1"
    local description="$2"
    
    print_info "Running: $description"
    
    if [ $VERBOSE -eq 1 ]; then
        echo "Command: $cmd"
    fi
    
    if eval "$cmd" >> "$LOG_FILE" 2>&1; then
        print_success "$description completed successfully"
        return 0
    else
        print_error "$description failed"
        echo "Check $LOG_FILE for details"
        return 1
    fi
}

# Function to check if file exists
check_file() {
    local file="$1"
    local description="$2"
    
    if [ -f "$file" ]; then
        print_success "$description exists"
        return 0
    else
        print_error "$description not found: $file"
        return 1
    fi
}

# Function to display usage
usage() {
    cat << EOF
Usage: $0 [OPTIONS] [TARGETS]

Advanced Memory Management Test Script for IKOS Kernel

OPTIONS:
    -h, --help      Show this help message
    -v, --verbose   Enable verbose output
    -c, --clean     Clean build directory before building
    -l, --log FILE  Use custom log file (default: build/memory_test.log)

TARGETS:
    all             Build and test all components (default)
    build           Build only, no testing
    test            Run tests only (assumes already built)
    clean           Clean build directory
    buddy           Test buddy allocator only
    slab            Test slab allocator only
    numa            Test NUMA allocator only
    compression     Test memory compression only
    paging          Test demand paging only
    stress          Run stress tests only
    integration     Run integration tests only

EXAMPLES:
    $0                      # Build and test everything
    $0 -v build            # Build with verbose output
    $0 -c test             # Clean build and run tests
    $0 buddy slab          # Test only buddy and slab allocators
    $0 --verbose stress    # Run stress tests with verbose output

EOF
}

# Parse command line arguments
CLEAN=0
TARGETS=()

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -c|--clean)
            CLEAN=1
            shift
            ;;
        -l|--log)
            LOG_FILE="$2"
            shift 2
            ;;
        all|build|test|clean|buddy|slab|numa|compression|paging|stress|integration)
            TARGETS+=("$1")
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Default target if none specified
if [ ${#TARGETS[@]} -eq 0 ]; then
    TARGETS=("all")
fi

# Initialize
print_info "IKOS Advanced Memory Management Test Suite"
print_info "=========================================="

# Create build directory
mkdir -p "$BUILD_DIR"

# Initialize log file
echo "IKOS Advanced Memory Management Test Log" > "$LOG_FILE"
echo "Started: $(date)" >> "$LOG_FILE"
echo "Arguments: $*" >> "$LOG_FILE"
echo "========================================" >> "$LOG_FILE"

# Clean if requested
if [ $CLEAN -eq 1 ]; then
    print_info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"/*
    print_success "Build directory cleaned"
fi

# Function to build components
build_components() {
    print_info "Building advanced memory management components..."
    
    # Build individual components
    run_command "make memory-advanced" "Advanced memory management compilation"
    
    # Build test executable
    run_command "make test_advanced_memory.o" "Advanced memory test compilation"
    run_command "make advanced_memory_test" "Advanced memory test linking"
    
    # Verify build artifacts
    check_file "$BUILD_DIR/buddy_allocator.o" "Buddy allocator object"
    check_file "$BUILD_DIR/slab_allocator.o" "Slab allocator object"
    check_file "$BUILD_DIR/numa_allocator.o" "NUMA allocator object"
    check_file "$BUILD_DIR/memory_compression.o" "Memory compression object"
    check_file "$BUILD_DIR/demand_paging.o" "Demand paging object"
    check_file "$BUILD_DIR/advanced_memory_manager.o" "Advanced memory manager object"
    check_file "$BUILD_DIR/advanced_memory_test" "Advanced memory test executable"
    
    print_success "All components built successfully"
}

# Function to run specific test
run_specific_test() {
    local test_name="$1"
    local test_description="$2"
    
    print_info "Running $test_description..."
    
    if [ ! -f "$BUILD_DIR/advanced_memory_test" ]; then
        print_error "Test executable not found. Run build first."
        return 1
    fi
    
    # Create a test-specific log
    local test_log="$BUILD_DIR/test_${test_name}.log"
    
    if run_command "$BUILD_DIR/advanced_memory_test $test_name" "$test_description"; then
        print_success "$test_description passed"
        return 0
    else
        print_error "$test_description failed"
        if [ -f "$test_log" ]; then
            print_info "Test output:"
            cat "$test_log"
        fi
        return 1
    fi
}

# Function to run all tests
run_all_tests() {
    print_info "Running comprehensive memory management tests..."
    
    local tests_passed=0
    local tests_total=0
    
    # Basic functionality tests
    ((tests_total++))
    if run_specific_test "basic" "Basic allocation tests"; then
        ((tests_passed++))
    fi
    
    # Component-specific tests
    ((tests_total++))
    if run_specific_test "buddy" "Buddy allocator tests"; then
        ((tests_passed++))
    fi
    
    ((tests_total++))
    if run_specific_test "slab" "Slab allocator tests"; then
        ((tests_passed++))
    fi
    
    ((tests_total++))
    if run_specific_test "numa" "NUMA allocator tests"; then
        ((tests_passed++))
    fi
    
    ((tests_total++))
    if run_specific_test "compression" "Memory compression tests"; then
        ((tests_passed++))
    fi
    
    ((tests_total++))
    if run_specific_test "paging" "Demand paging tests"; then
        ((tests_passed++))
    fi
    
    # Stress tests
    ((tests_total++))
    if run_specific_test "stress" "Stress tests"; then
        ((tests_passed++))
    fi
    
    # Integration tests
    ((tests_total++))
    if run_specific_test "integration" "Integration tests"; then
        ((tests_passed++))
    fi
    
    # Summary
    print_info "Test Results Summary"
    print_info "==================="
    print_info "Tests passed: $tests_passed/$tests_total"
    
    if [ $tests_passed -eq $tests_total ]; then
        print_success "All tests passed! ✓"
        return 0
    else
        local failed=$((tests_total - tests_passed))
        print_error "$failed test(s) failed ✗"
        return 1
    fi
}

# Main execution logic
exit_code=0

for target in "${TARGETS[@]}"; do
    case "$target" in
        all)
            build_components || exit_code=1
            run_all_tests || exit_code=1
            ;;
        build)
            build_components || exit_code=1
            ;;
        test)
            run_all_tests || exit_code=1
            ;;
        clean)
            print_info "Cleaning build directory..."
            rm -rf "$BUILD_DIR"/*
            print_success "Build directory cleaned"
            ;;
        buddy)
            run_specific_test "buddy" "Buddy allocator tests" || exit_code=1
            ;;
        slab)
            run_specific_test "slab" "Slab allocator tests" || exit_code=1
            ;;
        numa)
            run_specific_test "numa" "NUMA allocator tests" || exit_code=1
            ;;
        compression)
            run_specific_test "compression" "Memory compression tests" || exit_code=1
            ;;
        paging)
            run_specific_test "paging" "Demand paging tests" || exit_code=1
            ;;
        stress)
            run_specific_test "stress" "Stress tests" || exit_code=1
            ;;
        integration)
            run_specific_test "integration" "Integration tests" || exit_code=1
            ;;
        *)
            print_error "Unknown target: $target"
            exit_code=1
            ;;
    esac
done

# Final summary
echo "========================================" >> "$LOG_FILE"
echo "Completed: $(date)" >> "$LOG_FILE"

if [ $exit_code -eq 0 ]; then
    print_success "Advanced memory management testing completed successfully"
    print_info "Log file: $LOG_FILE"
else
    print_error "Advanced memory management testing completed with errors"
    print_info "Check log file for details: $LOG_FILE"
fi

exit $exit_code
