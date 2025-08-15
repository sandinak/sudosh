#!/bin/bash

# run_pipeline_regression_tests.sh - Automated Pipeline Security Regression Testing
#
# This script runs comprehensive regression tests for the pipeline security
# features and can be integrated into CI/CD pipelines.

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT"
TEST_BINARY="$BUILD_DIR/bin/test_pipeline_regression"
LOG_FILE="$PROJECT_ROOT/pipeline_regression_test.log"

# Toolchain (allow override via environment)
CC=${CC:-gcc}
# Default CFLAGS suitable for Linux/macOS; CI passes its own CFLAGS
CFLAGS_DEFAULT="-Wall -Wextra -std=c99 -O2"
if [[ "$(uname -s)" == "Linux" ]]; then
    CFLAGS_DEFAULT+=" -D_GNU_SOURCE"
fi
CFLAGS=${CFLAGS:-$CFLAGS_DEFAULT}
LIBS=""

# Detect PAM availability and set linker flags portably
# - On Linux: prefer -lpam -lpam_misc
# - On macOS: only -lpam is available; no pam_misc
# - If PAM headers unavailable: define MOCK_AUTH and do not link PAM
_detect_pam_and_libs() {
    local os
    os=$(uname -s)
    if echo '#include <security/pam_appl.h>' | "$CC" -E - >/dev/null 2>&1; then
        if [[ "$os" == "Linux" ]]; then
            LIBS="-lpam -lpam_misc"
        elif [[ "$os" == "Darwin" ]]; then
            LIBS="-lpam"
        else
            LIBS="-lpam"
        fi
    else
        CFLAGS+=" -DMOCK_AUTH"
        LIBS=""
    fi
}

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

print_header() {
    echo
    print_status $BLUE "=================================================="
    print_status $BLUE "$1"
    print_status $BLUE "=================================================="
    echo
}

# Function to check prerequisites
check_prerequisites() {
    print_header "Checking Prerequisites"
    
    # Check if we're in the right directory
    if [[ ! -f "$PROJECT_ROOT/src/sudosh.h" ]]; then
        print_status $RED "Error: Not in sudosh project directory"
        exit 1
    fi
    
    # Check for required tools
    local tools=("gcc" "make")
    for tool in "${tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            print_status $RED "Error: $tool is required but not installed"
            exit 1
        fi
    done
    
    print_status $GREEN "✓ All prerequisites met"
}

# Function to build the test
build_test() {
    print_header "Building Pipeline Regression Test"
    
    cd "$PROJECT_ROOT"
    
    # Clean and build
    print_status $YELLOW "Cleaning previous build..."
    make clean > /dev/null 2>&1 || true
    
    print_status $YELLOW "Building sudosh..."
    if ! make > /dev/null 2>&1; then
        print_status $RED "Error: Failed to build sudosh"
        exit 1
    fi
    
    print_status $YELLOW "Building regression test (via Makefile)..."
    if ! make -s pipeline-regression-test > /dev/null 2>&1; then
        print_status $RED "Error: Failed to build regression test"
        exit 1
    fi

    print_status $GREEN "✓ Build successful"
}

# Function to run the regression test
run_regression_test() {
    print_header "Running Pipeline Security Regression Tests"
    
    if [[ ! -x "$TEST_BINARY" ]]; then
        print_status $RED "Error: Test binary not found or not executable"
        exit 1
    fi
    
    # Run the test and capture output
    print_status $YELLOW "Executing regression test suite..."
    echo "Test execution started at $(date)" > "$LOG_FILE"
    
    if "$TEST_BINARY" 2>&1 | tee -a "$LOG_FILE"; then
        local exit_code=${PIPESTATUS[0]}
        
        case $exit_code in
            0)
                print_status $GREEN "✅ ALL REGRESSION TESTS PASSED!"
                print_status $GREEN "Pipeline security is intact and functioning correctly."
                ;;
            1)
                print_status $YELLOW "⚠️  SOME REGRESSION TESTS FAILED"
                print_status $YELLOW "Review the failures and fix before deployment."
                print_status $YELLOW "Check log file: $LOG_FILE"
                return 1
                ;;
            2)
                print_status $RED "❌ CRITICAL SECURITY REGRESSION DETECTED!"
                print_status $RED "Pipeline security has been compromised."
                print_status $RED "DO NOT DEPLOY until critical failures are fixed."
                print_status $RED "Check log file: $LOG_FILE"
                return 2
                ;;
            *)
                print_status $RED "❌ UNEXPECTED TEST FAILURE"
                print_status $RED "Test suite encountered an unexpected error."
                print_status $RED "Check log file: $LOG_FILE"
                return 3
                ;;
        esac
    else
        print_status $RED "❌ FAILED TO EXECUTE REGRESSION TESTS"
        print_status $RED "Test binary crashed or failed to run."
        return 4
    fi
}

# Function to generate test report
generate_report() {
    print_header "Generating Test Report"
    
    local report_file="$PROJECT_ROOT/pipeline_regression_report.txt"
    
    cat > "$report_file" << EOF
Pipeline Security Regression Test Report
========================================
Generated: $(date)
Host: $(hostname)
User: $(whoami)
Project: $(basename "$PROJECT_ROOT")

Test Results Summary:
$(tail -10 "$LOG_FILE" | grep -E "(Tests run|Tests passed|Tests failed|Critical failures)")

Security Status:
$(if [[ $1 -eq 0 ]]; then
    echo "✅ SECURE - All regression tests passed"
elif [[ $1 -eq 1 ]]; then
    echo "⚠️  WARNING - Some tests failed, review required"
elif [[ $1 -eq 2 ]]; then
    echo "❌ CRITICAL - Security regression detected, DO NOT DEPLOY"
else
    echo "❌ ERROR - Test execution failed"
fi)

Full test log available at: $LOG_FILE

Recommendations:
$(if [[ $1 -eq 0 ]]; then
    echo "- Pipeline security is functioning correctly"
    echo "- Safe to proceed with deployment"
elif [[ $1 -eq 1 ]]; then
    echo "- Review failed tests and fix issues"
    echo "- Re-run tests before deployment"
elif [[ $1 -eq 2 ]]; then
    echo "- IMMEDIATE ACTION REQUIRED"
    echo "- Critical security boundaries have been compromised"
    echo "- Do not deploy until all critical failures are resolved"
    echo "- Review recent changes to pipeline security code"
else
    echo "- Fix test execution issues"
    echo "- Check build environment and dependencies"
fi)
EOF
    
    print_status $GREEN "✓ Report generated: $report_file"
}

# Function to run quick smoke test
run_smoke_test() {
    print_header "Running Quick Smoke Test"
    
    # Test basic pipeline functionality
    local temp_test="/tmp/pipeline_smoke_test.c"
    cat > "$temp_test" << 'EOF'
#include <stdio.h>
#include <string.h>
#include "../src/sudosh.h"

int main() {
    // Basic smoke tests
    if (!is_pipeline_command("ps | grep root")) return 1;
    if (is_pipeline_command("ls -la")) return 1;
    if (!is_whitelisted_pipe_command("ps")) return 1;
    if (is_whitelisted_pipe_command("rm")) return 1;
    printf("Smoke test passed\n");
    return 0;
}
EOF
    
    _detect_pam_and_libs
    if "$CC" -Isrc -c "$temp_test" -o /tmp/smoke_test.o 2>/dev/null && \
       "$CC" /tmp/smoke_test.o obj/pipeline.o obj/security.o obj/utils.o obj/logging.o \
       obj/auth.o obj/command.o obj/nss.o obj/sudoers.o obj/sssd.o obj/filelock.o \
       obj/shell_enhancements.o obj/shell_env.o obj/config.o \
       -o /tmp/smoke_test $LIBS 2>/dev/null && \
       /tmp/smoke_test > /dev/null 2>&1; then
        print_status $GREEN "✓ Smoke test passed"
        rm -f "$temp_test" /tmp/smoke_test.o /tmp/smoke_test
        return 0
    else
        print_status $RED "✗ Smoke test failed"
        rm -f "$temp_test" /tmp/smoke_test.o /tmp/smoke_test
        return 1
    fi
}

# Main execution
main() {
    local start_time=$(date +%s)
    
    print_header "Pipeline Security Regression Test Suite"
    print_status $BLUE "Starting automated regression testing..."
    
    # Run all checks
    check_prerequisites
    build_test
    
    # Run smoke test first
    if ! run_smoke_test; then
        print_status $RED "Smoke test failed - aborting full regression test"
        exit 1
    fi
    
    # Run full regression test
    local test_result=0
    run_regression_test || test_result=$?
    
    # Generate report
    generate_report $test_result
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    print_header "Test Suite Complete"
    print_status $BLUE "Total execution time: ${duration} seconds"
    
    # Final status
    case $test_result in
        0)
            print_status $GREEN "🎉 SUCCESS: Pipeline security regression tests passed!"
            ;;
        1)
            print_status $YELLOW "⚠️  WARNING: Some regression tests failed"
            ;;
        2)
            print_status $RED "🚨 CRITICAL: Security regression detected!"
            ;;
        *)
            print_status $RED "💥 ERROR: Test execution failed"
            ;;
    esac
    
    exit $test_result
}

# Handle script arguments
case "${1:-}" in
    --help|-h)
        echo "Usage: $0 [--help|--smoke-only]"
        echo "  --help       Show this help message"
        echo "  --smoke-only Run only quick smoke test"
        exit 0
        ;;
    --smoke-only)
        check_prerequisites
        build_test
        run_smoke_test
        exit $?
        ;;
    *)
        main "$@"
        ;;
esac
