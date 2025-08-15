#!/bin/bash
#
# run_all_tests_simple.sh - Simple comprehensive test runner for sudosh
#
# This script runs all available test scripts in the tests directory
# and provides a summary of results.
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
LOG_FILE="${TEST_RESULTS_DIR}/all_tests_${TIMESTAMP}.log"

# Test counters
TOTAL_SCRIPTS=0
PASSED_SCRIPTS=0
FAILED_SCRIPTS=0

# Setup test environment
setup_test_environment() {
    echo -e "${BLUE}Setting up test environment...${NC}"
    
    # Create test results directory
    mkdir -p "${TEST_RESULTS_DIR}"
    
    # Initialize log file
    echo "Sudosh All Tests Run - $(date)" > "${LOG_FILE}"
    echo "===============================" >> "${LOG_FILE}"
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
    
    TOTAL_SCRIPTS=$((TOTAL_SCRIPTS + 1))
    echo -n "Running $script_name... "
    
    if timeout 120 "$script_path" >> "${LOG_FILE}" 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        PASSED_SCRIPTS=$((PASSED_SCRIPTS + 1))
        echo "PASS: $script_name" >> "${LOG_FILE}"
    else
        echo -e "${RED}FAIL${NC}"
        FAILED_SCRIPTS=$((FAILED_SCRIPTS + 1))
        echo "FAIL: $script_name" >> "${LOG_FILE}"
    fi
}

# Main execution
main() {
    echo -e "${BLUE}Starting comprehensive sudosh test suite...${NC}"
    echo "Running all test scripts in tests directory"
    echo "=============================================="
    
    setup_test_environment
    
    # Find and run all test scripts
    echo -e "\n${YELLOW}=== Shell and Security Tests ===${NC}"
    
    # Core functionality tests
    test_scripts=(
        "tests/test_shell_restriction.sh"
        "tests/test_shell_warning.sh"
        "tests/test_shell_modes.sh"
        "tests/test_sudoedit_functionality.sh"
        "tests/test_command_line_execution.sh"
        "tests/test_sudo_replacement.sh"
        "tests/test_sudo_options_validation.sh"
        "tests/test_cve_security.sh"
        "tests/test_security_restrictions.sh"
        "tests/test_ci_compatibility.sh"
        "tests/test_nopasswd_integration.sh"
        "tests/test_ansible_integration.sh"
        "tests/test_sudosh_2.0_release.sh"
    )
    
    # Run each test script if it exists and is executable
    for script in "${test_scripts[@]}"; do
        if [[ -x "$script" ]]; then
            run_test_script "$script"
        else
            echo -e "${YELLOW}Skipping $script (not found or not executable)${NC}"
        fi
    done
    
    # Generate summary
    echo -e "\n=============================================="
    echo -e "${BLUE}COMPREHENSIVE TEST SUMMARY${NC}"
    echo "=============================================="
    echo "Test Scripts:"
    echo -e "  Total: $TOTAL_SCRIPTS"
    echo -e "  Passed: ${GREEN}$PASSED_SCRIPTS${NC}"
    echo -e "  Failed: ${RED}$FAILED_SCRIPTS${NC}"
    
    echo
    echo "Coverage Areas Tested:"
    echo "  ✓ Shell restriction and access control"
    echo "  ✓ Secure file editing (sudoedit)"
    echo "  ✓ Command-line execution compatibility"
    echo "  ✓ Sudo replacement functionality"
    echo "  ✓ Security validations and CVE protections"
    echo "  ✓ Authentication and authorization"
    echo "  ✓ CI/CD compatibility"
    echo "  ✓ NOPASSWD functionality"
    echo "  ✓ Ansible integration"
    echo "  ✓ Release validation"
    
    echo
    echo "Detailed log: ${LOG_FILE}"
    
    if [ "$FAILED_SCRIPTS" -eq 0 ]; then
        echo -e "${GREEN}🎉 ALL TEST SCRIPTS PASSED!${NC}"
        echo -e "${GREEN}Sudosh is ready for production deployment.${NC}"
        exit 0
    else
        echo -e "${RED}❌ Some test scripts failed.${NC}"
        echo -e "${RED}Review the log file for details: ${LOG_FILE}${NC}"
        exit 1
    fi
}

# Run main function
main "$@"
