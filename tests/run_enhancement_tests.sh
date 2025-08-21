#!/bin/bash

# Comprehensive test runner for sudosh security enhancements
# This script compiles and runs all tests for the three security enhancements

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
TEST_DIR="$SCRIPT_DIR"
UNIT_DIR="$TEST_DIR/unit"
SECURITY_DIR="$TEST_DIR/security"
INTEGRATION_DIR="$TEST_DIR/integration"
REPORTS_DIR="$TEST_DIR/reports"

# Create reports directory
mkdir -p "$REPORTS_DIR"

# Compilation flags
CFLAGS="-Wall -Wextra -std=c99 -O2 -I$PROJECT_ROOT/src -I$TEST_DIR"
LDFLAGS="-lpam"

# Test results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
FAILED_TEST_NAMES=()

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Sudosh Security Enhancement Test Suite${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Function to print section headers
print_section() {
    echo -e "${YELLOW}$1${NC}"
    echo -e "${YELLOW}$(printf '=%.0s' $(seq 1 ${#1}))${NC}"
}

# Function to compile and run a test
run_test() {
    local test_file="$1"
    local test_name="$2"
    local test_category="$3"
    
    echo -n "Compiling $test_name... "
    
    # Get all object files except main.o
    local obj_files=""
    for obj in "$PROJECT_ROOT"/obj/*.o; do
        if [[ "$(basename "$obj")" != "main.o" ]]; then
            obj_files="$obj_files $obj"
        fi
    done
    
    # Compile the test
    local test_binary="$REPORTS_DIR/${test_name}_test"
    if gcc $CFLAGS "$test_file" $obj_files -o "$test_binary" $LDFLAGS 2>"$REPORTS_DIR/${test_name}_compile.log"; then
        echo -e "${GREEN}OK${NC}"
        
        echo -n "Running $test_name... "
        
        # Run the test and capture output
        local test_output="$REPORTS_DIR/${test_name}_output.log"
        local test_result="$REPORTS_DIR/${test_name}_result.log"
        
        if timeout 60 "$test_binary" > "$test_output" 2>&1; then
            local exit_code=$?
            echo "$exit_code" > "$test_result"
            
            if [ $exit_code -eq 0 ]; then
                echo -e "${GREEN}PASS${NC}"
                PASSED_TESTS=$((PASSED_TESTS + 1))
            else
                echo -e "${RED}FAIL${NC}"
                FAILED_TESTS=$((FAILED_TESTS + 1))
                FAILED_TEST_NAMES+=("$test_name")
            fi
        else
            echo -e "${RED}TIMEOUT/ERROR${NC}"
            echo "255" > "$test_result"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            FAILED_TEST_NAMES+=("$test_name")
        fi
        
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        
        # Clean up binary
        rm -f "$test_binary"
    else
        echo -e "${RED}COMPILE FAILED${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        FAILED_TEST_NAMES+=("$test_name (compile)")
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
    fi
}

# Function to check if sudosh is built
check_build() {
    if [ ! -f "$PROJECT_ROOT/bin/sudosh" ] || [ ! -d "$PROJECT_ROOT/obj" ]; then
        echo -e "${YELLOW}Building sudosh first...${NC}"
        cd "$PROJECT_ROOT"
        if ! make clean && make; then
            echo -e "${RED}Failed to build sudosh. Please run 'make' in the project root.${NC}"
            exit 1
        fi
        cd "$SCRIPT_DIR"
    fi
}

# Function to run basic smoke tests
run_smoke_tests() {
    print_section "Smoke Tests"
    
    echo -n "Checking sudosh binary... "
    if [ -x "$PROJECT_ROOT/bin/sudosh" ]; then
        echo -e "${GREEN}OK${NC}"
    else
        echo -e "${RED}MISSING${NC}"
        return 1
    fi
    
    echo -n "Checking object files... "
    if [ -d "$PROJECT_ROOT/obj" ] && [ "$(ls -1 "$PROJECT_ROOT/obj"/*.o 2>/dev/null | wc -l)" -gt 0 ]; then
        echo -e "${GREEN}OK${NC}"
    else
        echo -e "${RED}MISSING${NC}"
        return 1
    fi
    
    echo -n "Testing basic compilation... "
    if gcc $CFLAGS -c "$PROJECT_ROOT/src/sudosh.h" -o /tmp/test_compile.o 2>/dev/null; then
        rm -f /tmp/test_compile.o
        echo -e "${GREEN}OK${NC}"
    else
        echo -e "${RED}FAILED${NC}"
        return 1
    fi
    
    echo ""
}

# Function to generate test report
generate_report() {
    local report_file="$REPORTS_DIR/test_summary.txt"
    
    {
        echo "Sudosh Security Enhancement Test Report"
        echo "======================================="
        echo "Generated: $(date)"
        echo ""
        echo "Test Results:"
        echo "  Total Tests: $TOTAL_TESTS"
        echo "  Passed: $PASSED_TESTS"
        echo "  Failed: $FAILED_TESTS"
        echo ""
        
        if [ $FAILED_TESTS -gt 0 ]; then
            echo "Failed Tests:"
            for test_name in "${FAILED_TEST_NAMES[@]}"; do
                echo "  - $test_name"
            done
            echo ""
        fi
        
        echo "Detailed Results:"
        echo "=================="
        
        for category in "NSS Enhancement" "Pipeline Security" "Text Processing" "Security Regression" "Integration"; do
            echo ""
            echo "$category Tests:"
            echo "$(printf '=%.0s' $(seq 1 $((${#category} + 7))))"
            
            # Find relevant test output files
            for output_file in "$REPORTS_DIR"/*_output.log; do
                if [ -f "$output_file" ]; then
                    local test_name=$(basename "$output_file" _output.log)
                    case "$test_name" in
                        *nss*) test_cat="NSS Enhancement" ;;
                        *pipeline*) test_cat="Pipeline Security" ;;
                        *text*) test_cat="Text Processing" ;;
                        *security*) test_cat="Security Regression" ;;
                        *integration*) test_cat="Integration" ;;
                        *) test_cat="Other" ;;
                    esac
                    
                    if [ "$test_cat" = "$category" ]; then
                        echo ""
                        echo "$test_name:"
                        echo "$(printf '-%.0s' $(seq 1 $((${#test_name} + 1))))"
                        cat "$output_file"
                    fi
                fi
            done
        done
    } > "$report_file"
    
    echo -e "${BLUE}Full test report saved to: $report_file${NC}"
}

# Main execution
main() {
    # Check if sudosh is built
    check_build
    
    # Run smoke tests
    if ! run_smoke_tests; then
        echo -e "${RED}Smoke tests failed. Cannot continue.${NC}"
        exit 1
    fi
    
    # Run unit tests
    print_section "Unit Tests"
    
    if [ -f "$UNIT_DIR/test_nss_enhancements.c" ]; then
        run_test "$UNIT_DIR/test_nss_enhancements.c" "nss_enhancements" "unit"
    fi
    
    if [ -f "$UNIT_DIR/test_pipeline_security.c" ]; then
        run_test "$UNIT_DIR/test_pipeline_security.c" "pipeline_security" "unit"
    fi
    
    if [ -f "$UNIT_DIR/test_text_processing_redirection.c" ]; then
        run_test "$UNIT_DIR/test_text_processing_redirection.c" "text_processing_redirection" "unit"
    fi

    if [ -f "$UNIT_DIR/test_alias_validation.c" ]; then
        run_test "$UNIT_DIR/test_alias_validation.c" "alias_validation" "unit"
    fi

    if [ -f "$UNIT_DIR/test_redirection_parsing.c" ]; then
        run_test "$UNIT_DIR/test_redirection_parsing.c" "redirection_parsing" "unit"
    fi
    
    echo ""
    
    # Run security tests
    print_section "Security Tests"
    
    if [ -f "$SECURITY_DIR/test_security_enhancements.c" ]; then
        run_test "$SECURITY_DIR/test_security_enhancements.c" "security_enhancements" "security"
    fi
    
    echo ""
    
    # Run integration tests
    print_section "Integration Tests"
    
    if [ -f "$INTEGRATION_DIR/test_enhancement_integration.c" ]; then
        run_test "$INTEGRATION_DIR/test_enhancement_integration.c" "enhancement_integration" "integration"
    fi

    if [ -f "$INTEGRATION_DIR/test_pipeline_redirection_fix.c" ]; then
        run_test "$INTEGRATION_DIR/test_pipeline_redirection_fix.c" "pipeline_redirection_fix" "integration"
    fi
    
    echo ""
    
    # Print summary
    print_section "Test Summary"
    
    echo "Total Tests: $TOTAL_TESTS"
    echo "Passed: $PASSED_TESTS"
    echo "Failed: $FAILED_TESTS"
    
    if [ $FAILED_TESTS -gt 0 ]; then
        echo ""
        echo -e "${RED}Failed Tests:${NC}"
        for test_name in "${FAILED_TEST_NAMES[@]}"; do
            echo -e "${RED}  - $test_name${NC}"
        done
    fi
    
    echo ""
    
    # Generate detailed report
    generate_report
    
    # Exit with appropriate code
    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${GREEN}All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}Some tests failed. Check the detailed report for more information.${NC}"
        exit 1
    fi
}

# Handle command line arguments
case "${1:-}" in
    --help|-h)
        echo "Usage: $0 [options]"
        echo ""
        echo "Options:"
        echo "  --help, -h     Show this help message"
        echo "  --clean        Clean up test reports before running"
        echo "  --verbose      Show detailed output"
        echo ""
        echo "This script compiles and runs comprehensive tests for the three"
        echo "sudosh security enhancements:"
        echo "  1. NSS-based user/group information retrieval"
        echo "  2. Secure pipeline support with command auditing"
        echo "  3. Safe text processing and controlled file redirection"
        exit 0
        ;;
    --clean)
        echo "Cleaning up test reports..."
        rm -rf "$REPORTS_DIR"
        mkdir -p "$REPORTS_DIR"
        shift
        ;;
    --verbose)
        set -x
        shift
        ;;
esac

# Run main function
main "$@"
