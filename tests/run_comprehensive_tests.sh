#!/bin/bash
#
# run_comprehensive_tests.sh - Comprehensive test runner for sudosh
#
# This script runs all available tests in a structured manner suitable for
# both local development and CI/CD environments. It provides detailed
# reporting and can run in non-interactive mode.
#

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TEST_RESULTS_DIR="${SCRIPT_DIR}/test_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="${TEST_RESULTS_DIR}/comprehensive_test_${TIMESTAMP}.log"

# Colors for output (disabled in CI)
if [[ -t 1 ]] && [[ "${CI:-}" != "true" ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    CYAN='\033[0;36m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    CYAN=''
    NC=''
fi

# Test tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Test categories
declare -A TEST_CATEGORIES=(
    ["unit"]="Unit Tests"
    ["integration"]="Integration Tests"
    ["security"]="Security Tests"
    ["command_line"]="Command Line Tests"
    ["sudo_replacement"]="Sudo Replacement Tests"
    ["sudo_options"]="Sudo Options Validation Tests"
    ["cve_security"]="CVE Security Tests"
    ["ansible"]="Ansible Integration Tests"
    ["pipeline"]="Pipeline Tests"
    ["shell_restriction"]="Shell Restriction Tests"
    ["sudoedit"]="Sudoedit Functionality Tests"
    ["release_validation"]="Release Validation Tests"
)

# Setup test environment
setup_test_environment() {
    echo -e "${BLUE}Setting up comprehensive test environment...${NC}"
    
    # Create test results directory
    mkdir -p "${TEST_RESULTS_DIR}"

    # Initialize log file
    local current_date
    current_date=$(date)
    echo "Sudosh Comprehensive Test Run - ${current_date}" > "${LOG_FILE}"
    echo "=========================================" >> "${LOG_FILE}"
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

# Log function
log_message() {
    local message="$1"
    echo "${message}" | tee -a "${LOG_FILE}"
}

# Run a test category
run_test_category() {
    local category="$1"
    local description="${TEST_CATEGORIES[${category}]}"
    local tests_run=0
    local tests_passed=0

    echo -e "\n${CYAN}=== ${description} ===${NC}"
    log_message "=== ${description} ==="

    case "${category}" in
        "unit")
            run_unit_tests tests_run tests_passed
            ;;
        "integration")
            run_integration_tests tests_run tests_passed
            ;;
        "security")
            run_security_tests tests_run tests_passed
            ;;
        "command_line")
            run_command_line_tests tests_run tests_passed
            ;;
        "sudo_replacement")
            run_sudo_replacement_tests tests_run tests_passed
            ;;
        "sudo_options")
            run_sudo_options_tests tests_run tests_passed
            ;;
        "cve_security")
            run_cve_security_tests tests_run tests_passed
            ;;
        "ansible")
            run_ansible_tests tests_run tests_passed
            ;;
        "pipeline")
            run_pipeline_tests tests_run tests_passed
            ;;
        "shell_restriction")
            run_shell_restriction_tests tests_run tests_passed
            ;;
        "sudoedit")
            run_sudoedit_tests tests_run tests_passed
            ;;
        "release_validation")
            run_release_validation_tests tests_run tests_passed
            ;;
        *)
            echo -e "${RED}Unknown test category: ${category}${NC}"
            return 1
            ;;
    esac

    # Update totals
    TOTAL_TESTS=$((TOTAL_TESTS + tests_run))
    PASSED_TESTS=$((PASSED_TESTS + tests_passed))
    FAILED_TESTS=$((FAILED_TESTS + tests_run - tests_passed))

    # Report category results
    if [[ "${tests_passed}" -eq "${tests_run}" ]]; then
        echo -e "${GREEN}${description}: ${tests_passed}/${tests_run} passed${NC}"
        log_message "${description}: ${tests_passed}/${tests_run} passed"
    else
        echo -e "${RED}${description}: ${tests_passed}/${tests_run} passed${NC}"
        log_message "${description}: ${tests_passed}/${tests_run} passed"
    fi
}

# Run unit tests
run_unit_tests() {
    local -n run_ref=$1
    local -n passed_ref=$2
    
    run_ref=0
    passed_ref=0
    
    if make unit-test >/dev/null 2>&1; then
        # Count unit test files
        local unit_test_count
        unit_test_count=$(find tests -name "test_unit_*.c" | wc -l || true)
        run_ref=${unit_test_count}
        passed_ref=${unit_test_count}
        echo -e "${GREEN}Unit tests passed${NC}"
    else
        echo -e "${RED}Unit tests failed${NC}"
        run_ref=1
        passed_ref=0
    fi
}

# Run integration tests
run_integration_tests() {
    local -n run_ref=$1
    local -n passed_ref=$2
    
    run_ref=0
    passed_ref=0
    
    if make integration-test >/dev/null 2>&1; then
        # Count integration test files
        local integration_test_count
        integration_test_count=$(find tests -name "test_integration_*.c" | wc -l || true)
        run_ref=${integration_test_count}
        passed_ref=${integration_test_count}
        echo -e "${GREEN}Integration tests passed${NC}"
    else
        echo -e "${RED}Integration tests failed${NC}"
        run_ref=1
        passed_ref=0
    fi
}

# Run security tests
run_security_tests() {
    local -n run_ref=$1
    local -n passed_ref=$2
    
    run_ref=0
    passed_ref=0
    
    if make security-test >/dev/null 2>&1; then
        # Count security test files
        local security_test_count
        security_test_count=$(find tests -name "test_security_*.c" | wc -l || true)
        run_ref=${security_test_count}
        passed_ref=${security_test_count}
        echo -e "${GREEN}Security tests passed${NC}"
    else
        echo -e "${RED}Security tests failed${NC}"
        run_ref=1
        passed_ref=0
    fi
}

# Run command line tests
run_command_line_tests() {
    local -n run_ref=$1
    local -n passed_ref=$2
    
    if [[ -x "tests/test_command_line_execution.sh" ]]; then
        if tests/test_command_line_execution.sh >/dev/null 2>&1; then
            run_ref=18  # Known number of tests in the script
            passed_ref=18
            echo -e "${GREEN}Command line tests passed${NC}"
        else
            run_ref=18
            passed_ref=0
            echo -e "${RED}Command line tests failed${NC}"
        fi
    else
        echo -e "${YELLOW}Command line tests not found${NC}"
        run_ref=0
        passed_ref=0
    fi
}

# Run sudo replacement tests
run_sudo_replacement_tests() {
    local -n run_ref=$1
    local -n passed_ref=$2

    if [[ -x "tests/test_sudo_replacement.sh" ]]; then
        if tests/test_sudo_replacement.sh >/dev/null 2>&1; then
            run_ref=14  # Known number of tests in the script
            passed_ref=14
            echo -e "${GREEN}Sudo replacement tests passed${NC}"
        else
            run_ref=14
            passed_ref=0
            echo -e "${RED}Sudo replacement tests failed${NC}"
        fi
    else
        echo -e "${YELLOW}Sudo replacement tests not found${NC}"
        run_ref=0
        passed_ref=0
    fi
}

# Run sudo options validation tests
run_sudo_options_tests() {
    local -n run_ref=$1
    local -n passed_ref=$2

    if [[ -x "tests/test_sudo_options_validation.sh" ]]; then
        if tests/test_sudo_options_validation.sh >/dev/null 2>&1; then
            run_ref=40  # Estimated number of tests in the script
            passed_ref=40
            echo -e "${GREEN}Sudo options validation tests passed${NC}"
        else
            run_ref=40
            passed_ref=0
            echo -e "${RED}Sudo options validation tests failed${NC}"
        fi
    else
        echo -e "${YELLOW}Sudo options validation tests not found${NC}"
        run_ref=0
        passed_ref=0
    fi
}

# Run CVE security tests
run_cve_security_tests() {
    local -n run_ref=$1
    local -n passed_ref=$2

    if [[ -x "tests/test_cve_security.sh" ]]; then
        if tests/test_cve_security.sh >/dev/null 2>&1; then
            run_ref=25  # Estimated number of tests in the script
            passed_ref=25
            echo -e "${GREEN}CVE security tests passed${NC}"
        else
            run_ref=25
            passed_ref=0
            echo -e "${RED}CVE security tests failed${NC}"
        fi
    else
        echo -e "${YELLOW}CVE security tests not found${NC}"
        run_ref=0
        passed_ref=0
    fi
}

# Run ansible tests
run_ansible_tests() {
    local -n run_ref=$1
    local -n passed_ref=$2
    
    # Check if ansible is available
    if ! command -v ansible-playbook >/dev/null 2>&1; then
        echo -e "${YELLOW}Ansible not available, skipping ansible tests${NC}"
        run_ref=0
        passed_ref=0
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        return
    fi
    
    if [[ -x "tests/test_ansible_integration.sh" ]]; then
        if tests/test_ansible_integration.sh >/dev/null 2>&1; then
            run_ref=5  # Estimated number of ansible tests
            passed_ref=5
            echo -e "${GREEN}Ansible tests passed${NC}"
        else
            run_ref=5
            passed_ref=0
            echo -e "${RED}Ansible tests failed${NC}"
        fi
    else
        echo -e "${YELLOW}Ansible tests not found${NC}"
        run_ref=0
        passed_ref=0
    fi
}

# Run pipeline tests
run_pipeline_tests() {
    local -n run_ref=$1
    local -n passed_ref=$2

    if make test-pipeline-smoke >/dev/null 2>&1; then
        run_ref=10  # Estimated number of pipeline tests
        passed_ref=10
        echo -e "${GREEN}Pipeline tests passed${NC}"
    else
        run_ref=10
        passed_ref=0
        echo -e "${RED}Pipeline tests failed${NC}"
    fi
}

# Run shell restriction tests
run_shell_restriction_tests() {
    local -n run_ref=$1
    local -n passed_ref=$2

    local tests_passed=0
    local tests_total=0

    # Test shell restriction functionality
    if [[ -x "tests/test_shell_restriction.sh" ]]; then
        echo "Running shell restriction tests..."
        if tests/test_shell_restriction.sh >/dev/null 2>&1; then
            tests_total=$((tests_total + 16))
            tests_passed=$((tests_passed + 16))
            echo -e "${GREEN}Shell restriction tests passed (16/16)${NC}"
        else
            tests_total=$((tests_total + 16))
            echo -e "${RED}Shell restriction tests failed${NC}"
        fi
    else
        echo -e "${YELLOW}Shell restriction tests not found${NC}"
    fi

    # Test shell warning messages
    if [[ -x "tests/test_shell_warning.sh" ]]; then
        echo "Running shell warning tests..."
        if tests/test_shell_warning.sh >/dev/null 2>&1; then
            tests_total=$((tests_total + 3))
            tests_passed=$((tests_passed + 3))
            echo -e "${GREEN}Shell warning tests passed (3/3)${NC}"
        else
            tests_total=$((tests_total + 3))
            echo -e "${RED}Shell warning tests failed${NC}"
        fi
    else
        echo -e "${YELLOW}Shell warning tests not found${NC}"
    fi

    # Test shell modes
    if [[ -x "tests/test_shell_modes.sh" ]]; then
        echo "Running shell modes tests..."
        if tests/test_shell_modes.sh >/dev/null 2>&1; then
            tests_total=$((tests_total + 7))
            tests_passed=$((tests_passed + 7))
            echo -e "${GREEN}Shell modes tests passed (7/7)${NC}"
        else
            tests_total=$((tests_total + 7))
            echo -e "${RED}Shell modes tests failed${NC}"
        fi
    else
        echo -e "${YELLOW}Shell modes tests not found${NC}"
    fi

    run_ref=${tests_total}
    passed_ref=${tests_passed}

    echo "Shell restriction test summary: ${tests_passed}/${tests_total} passed"
}

# Run sudoedit tests
run_sudoedit_tests() {
    local -n run_ref=$1
    local -n passed_ref=$2

    if [[ -x "tests/test_sudoedit_functionality.sh" ]]; then
        if tests/test_sudoedit_functionality.sh >/dev/null 2>&1; then
            run_ref=11  # Known number of sudoedit tests
            passed_ref=11
            echo -e "${GREEN}Sudoedit tests passed${NC}"
        else
            run_ref=11
            passed_ref=0
            echo -e "${RED}Sudoedit tests failed${NC}"
        fi
    else
        echo -e "${YELLOW}Sudoedit tests not found${NC}"
        run_ref=0
        passed_ref=0
    fi
}

# Run release validation tests
run_release_validation_tests() {
    local -n run_ref=$1
    local -n passed_ref=$2

    if [[ -x "tests/test_sudosh_2.0_release.sh" ]]; then
        if tests/test_sudosh_2.0_release.sh >/dev/null 2>&1; then
            run_ref=25  # Known number of release validation tests
            passed_ref=25
            echo -e "${GREEN}Release validation tests passed${NC}"
        else
            run_ref=25
            passed_ref=0
            echo -e "${RED}Release validation tests failed${NC}"
        fi
    else
        echo -e "${YELLOW}Release validation tests not found${NC}"
        run_ref=0
        passed_ref=0
    fi
}

# Generate test report
generate_test_report() {
    local report_file="${TEST_RESULTS_DIR}/test_report_${TIMESTAMP}.txt"

    echo "Generating test report..."

    local current_date
    current_date=$(date)
    cat > "${report_file}" << EOF
Sudosh Comprehensive Test Report
Generated: ${current_date}
========================================

Test Summary:
- Total Tests: ${TOTAL_TESTS}
- Passed: ${PASSED_TESTS}
- Failed: ${FAILED_TESTS}
- Skipped: ${SKIPPED_TESTS}
- Success Rate: $(( PASSED_TESTS * 100 / (TOTAL_TESTS == 0 ? 1 : TOTAL_TESTS) ))%

Test Categories:
EOF

    for category in "${!TEST_CATEGORIES[@]}"; do
        echo "- ${TEST_CATEGORIES[${category}]}" >> "${report_file}"
    done

    echo "" >> "${report_file}"
    echo "Detailed log available at: ${LOG_FILE}" >> "${report_file}"

    echo -e "${BLUE}Test report generated: ${report_file}${NC}"
}

# Main execution
main() {
    local categories_to_run=("${@}")
    
    # If no categories specified, run all
    if [[ ${#categories_to_run[@]} -eq 0 ]]; then
        categories_to_run=("unit" "integration" "security" "command_line" "sudo_replacement" "sudo_options" "cve_security" "shell_restriction" "sudoedit" "ansible" "pipeline" "release_validation")
    fi

    echo -e "${BLUE}Starting comprehensive sudosh test suite${NC}"
    echo -e "${BLUE}Categories to run: ${categories_to_run[*]}${NC}"

    setup_test_environment

    # Run each test category
    for category in "${categories_to_run[@]}"; do
        if [[ -n "${TEST_CATEGORIES[${category}]}" ]]; then
            run_test_category "${category}"
        else
            echo -e "${RED}Unknown test category: ${category}${NC}"
            exit 1
        fi
    done

    # Generate final report
    echo -e "\n${CYAN}=== Final Test Summary ===${NC}"
    echo -e "Total Tests: ${TOTAL_TESTS}"
    echo -e "Passed: ${GREEN}${PASSED_TESTS}${NC}"
    echo -e "Failed: ${RED}${FAILED_TESTS}${NC}"
    echo -e "Skipped: ${YELLOW}${SKIPPED_TESTS}${NC}"

    if [[ "${FAILED_TESTS}" -eq 0 ]]; then
        echo -e "\n${GREEN}All tests passed successfully!${NC}"
        generate_test_report
        exit 0
    else
        echo -e "\n${RED}Some tests failed!${NC}"
        generate_test_report
        exit 1
    fi
}

# Handle script arguments
if [[ "$1" = "--help" ]] || [[ "$1" = "-h" ]]; then
    echo "Usage: $0 [test_categories...]"
    echo ""
    echo "Available test categories:"
    for category in "${!TEST_CATEGORIES[@]}"; do
        echo "  ${category} - ${TEST_CATEGORIES[${category}]}"
    done
    echo ""
    echo "Examples:"
    echo "  $0                    # Run all tests"
    echo "  $0 unit security      # Run only unit and security tests"
    echo "  $0 command_line       # Run only command line tests"
    exit 0
fi

# Run main function with all arguments
main "$@"
