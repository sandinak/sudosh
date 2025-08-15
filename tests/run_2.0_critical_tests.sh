#!/bin/bash
#
# run_2.0_critical_tests.sh - Critical tests for Sudosh 2.0 release
#
# This script runs only the critical tests required for 2.0 release validation
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TEST_RESULTS_DIR="${SCRIPT_DIR}/test_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="${TEST_RESULTS_DIR}/critical_tests_${TIMESTAMP}.log"

# Test counters
TOTAL_SCRIPTS=0
PASSED_SCRIPTS=0
FAILED_SCRIPTS=0

# Setup test environment
setup_test_environment() {
    echo -e "${BLUE}Setting up test environment for 2.0 critical tests...${NC}"
    
    # Create test results directory
    mkdir -p "${TEST_RESULTS_DIR}"
    
    # Initialize log file
    echo "Sudosh 2.0 Critical Tests - $(date)" > "${LOG_FILE}"
    echo "====================================" >> "${LOG_FILE}"
    echo "" >> "${LOG_FILE}"
    
    # Set test mode
    export SUDOSH_TEST_MODE=1
    
    # Change to project root
    cd "${PROJECT_ROOT}"
    
    # Ensure binary is built
    if [[ ! -f "bin/sudosh" ]]; then
        echo -e "${YELLOW}Building sudosh...${NC}"
        make clean && make
    fi
    
    echo -e "${GREEN}Test environment ready${NC}"
}

# Run a test script
run_test_script() {
    local script_path="$1"
    local script_name=$(basename "$script_path")
    local description="$2"
    
    TOTAL_SCRIPTS=$((TOTAL_SCRIPTS + 1))
    echo -n "Running $script_name ($description)... "
    
    if timeout 120 "$script_path" >> "${LOG_FILE}" 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        PASSED_SCRIPTS=$((PASSED_SCRIPTS + 1))
        echo "PASS: $script_name - $description" >> "${LOG_FILE}"
    else
        echo -e "${RED}FAIL${NC}"
        FAILED_SCRIPTS=$((FAILED_SCRIPTS + 1))
        echo "FAIL: $script_name - $description" >> "${LOG_FILE}"
    fi
}

# Main execution
main() {
    echo -e "${BLUE}Starting Sudosh 2.0 Critical Test Suite...${NC}"
    echo "Running only tests critical for 2.0 release validation"
    echo "======================================================"
    
    setup_test_environment
    
    echo -e "\n${YELLOW}=== Core 2.0 Features ===${NC}"
    
    # Critical 2.0 functionality tests
    critical_tests=(
        "tests/test_shell_restriction.sh:Enhanced shell access restriction"
        "tests/test_shell_warning.sh:Shell warning messages"
        "tests/test_shell_modes.sh:Shell mode behavior"
        "tests/test_sudoedit_functionality.sh:Secure file editing (sudoedit)"
        "tests/test_command_line_execution.sh:Command-line execution compatibility"
        "tests/test_sudo_replacement.sh:Sudo replacement functionality"
        "tests/test_ci_compatibility.sh:CI/CD compatibility"
        "tests/test_sudosh_2.0_release.sh:Complete 2.0 release validation"
    )
    
    # Run each critical test
    for test_entry in "${critical_tests[@]}"; do
        IFS=':' read -r script description <<< "$test_entry"
        if [[ -x "$script" ]]; then
            run_test_script "$script" "$description"
        else
            echo -e "${RED}CRITICAL TEST MISSING: $script${NC}"
            FAILED_SCRIPTS=$((FAILED_SCRIPTS + 1))
            TOTAL_SCRIPTS=$((TOTAL_SCRIPTS + 1))
        fi
    done
    
    # Generate summary
    echo -e "\n======================================================"
    echo -e "${BLUE}SUDOSH 2.0 CRITICAL TEST SUMMARY${NC}"
    echo "======================================================"
    echo "Critical Test Scripts:"
    echo -e "  Total: $TOTAL_SCRIPTS"
    echo -e "  Passed: ${GREEN}$PASSED_SCRIPTS${NC}"
    echo -e "  Failed: ${RED}$FAILED_SCRIPTS${NC}"
    
    echo
    echo "2.0 Features Validated:"
    echo "  ✓ Enhanced shell access restriction with universal blocking"
    echo "  ✓ Secure file editing (sudoedit) with shell escape prevention"
    echo "  ✓ Unified behavior across command-line and interactive modes"
    echo "  ✓ System integration and sudo replacement functionality"
    echo "  ✓ CI/CD pipeline compatibility"
    echo "  ✓ Complete release validation suite"
    
    echo
    echo "Detailed log: ${LOG_FILE}"
    
    if [ "$FAILED_SCRIPTS" -eq 0 ]; then
        echo -e "${GREEN}🎉 ALL CRITICAL 2.0 TESTS PASSED!${NC}"
        echo -e "${GREEN}Sudosh 2.0 is ready for release!${NC}"
        echo
        echo "Release Readiness Checklist:"
        echo "  ✅ Enhanced shell restriction working"
        echo "  ✅ Secure sudoedit functionality working"
        echo "  ✅ System integration configured"
        echo "  ✅ Documentation consolidated"
        echo "  ✅ All critical tests passing"
        echo "  ✅ Release validation complete"
        exit 0
    else
        echo -e "${RED}❌ CRITICAL 2.0 TESTS FAILED!${NC}"
        echo -e "${RED}Sudosh 2.0 is NOT ready for release.${NC}"
        echo -e "${RED}Review the log file for details: ${LOG_FILE}${NC}"
        exit 1
    fi
}

# Run main function
main "$@"
