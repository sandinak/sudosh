#!/bin/bash
#
# run_all_tests.sh - Comprehensive test suite runner for sudosh
#
# This script runs all test suites in unattended mode and provides
# a comprehensive summary of test results.
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TOTAL_SUITES=0
PASSED_SUITES=0
TOTAL_TESTS=0
PASSED_TESTS=0

# Setup test environment
setup_test_env() {
    echo -e "${BLUE}Setting up test environment...${NC}"
    export SUDOSH_TEST_MODE=1
    
    # Ensure sudosh binary exists
    if [ ! -x "./bin/sudosh" ]; then
        echo -e "${RED}ERROR: sudosh binary not found at ./bin/sudosh${NC}"
        echo "Please run 'make' first to build sudosh"
        exit 1
    fi
}

# Cleanup test environment
cleanup_test_env() {
    echo -e "${BLUE}Cleaning up test environment...${NC}"
    unset SUDOSH_TEST_MODE
}

# Run a test suite
run_test_suite() {
    local suite_name="$1"
    local command="$2"
    local timeout_seconds="${3:-60}"

    TOTAL_SUITES=$((TOTAL_SUITES + 1))
    echo -e "\n${YELLOW}=== Running $suite_name ===${NC}"

    # Always enforce a timeout, even if the system lacks timeout/gtimeout
    set +e
    if command -v timeout >/dev/null 2>&1; then
        timeout "$timeout_seconds" bash -c "$command" > /tmp/test_output_$$ 2>&1
        status=$?
    elif command -v gtimeout >/dev/null 2>&1; then
        gtimeout "$timeout_seconds" bash -c "$command" > /tmp/test_output_$$ 2>&1
        status=$?
    else
        # Fallback: background the process and kill the PID after timeout
        bash -c "$command" > /tmp/test_output_$$ 2>&1 &
        cmd_pid=$!
        (
            trap '' TERM INT
            sleep "$timeout_seconds"
            if kill -0 "$cmd_pid" >/dev/null 2>&1; then
                kill -TERM "$cmd_pid" >/dev/null 2>&1 || true
                sleep 1
                kill -KILL "$cmd_pid" >/dev/null 2>&1 || true
            fi
        ) >/dev/null 2>&1 &
        watcher_pid=$!
        wait "$cmd_pid" || true
        status=$?
        kill "$watcher_pid" >/dev/null 2>&1 || true
    fi
    set -e

    if [ $status -eq 0 ]; then
        echo -e "${GREEN}✓ $suite_name PASSED${NC}"
        PASSED_SUITES=$((PASSED_SUITES + 1))

        # Extract test counts if available
        local tests_run=$(grep -o "Tests run: [0-9]*" /tmp/test_output_$$ | tail -1 | grep -o "[0-9]*" || echo "0")
        local tests_passed=$(grep -o "Tests passed: [0-9]*" /tmp/test_output_$$ | tail -1 | grep -o "[0-9]*" || echo "0")

        if [ "$tests_run" -gt 0 ]; then
            TOTAL_TESTS=$((TOTAL_TESTS + tests_run))
            PASSED_TESTS=$((PASSED_TESTS + tests_passed))
            echo "  Tests: $tests_passed/$tests_run passed"
        fi
    else
        echo -e "${RED}✗ $suite_name FAILED${NC}"
        echo "Error output:"
        cat /tmp/test_output_$$
    fi

    rm -f /tmp/test_output_$$
}

# Main test execution
main() {
    echo -e "${BLUE}Starting comprehensive sudosh test suite...${NC}"
    echo "=============================================="
    
    setup_test_env
    
    # Unit tests
    echo -e "\n${BLUE}=== Running Unit Tests ===${NC}"
    run_test_suite "Authentication Unit Tests" \
        "bin/test_unit_auth" 30

    run_test_suite "Security Unit Tests" \
        "bin/test_unit_security" 30

    run_test_suite "Utility Unit Tests" \
        "bin/test_unit_utils" 30

    run_test_suite "Shell Enhancements Tests" \
        "bin/test_shell_enhancements" 30

    run_test_suite "Color Functionality Tests" \
        "bin/test_color_functionality" 30

    # Integration tests
    echo -e "\n${BLUE}=== Running Integration Tests ===${NC}"
    run_test_suite "Basic Integration Tests" \
        "bin/test_integration_basic" 30

    run_test_suite "Tab Completion Tests" \
        "bash ./tests/integration/test_tab_completion.sh" 30

    run_test_suite "Which Command Tests" \
        "bash ./tests/integration/test_which_command.sh" 30

    run_test_suite "Simple Auth Tests" \
        "bash ./tests/integration/simple_auth_test.sh" 30
    
    # Security tests
    echo -e "\n${BLUE}=== Running Security Tests ===${NC}"
    run_test_suite "CVE Security Tests" \
        "bash ./tests/security/security_cve_tests.sh" 30

    run_test_suite "Privilege Escalation Tests" \
        "bin/test_security_privilege_escalation" 30

    run_test_suite "Command Injection Tests" \
        "bin/test_security_command_injection" 30

    run_test_suite "Authentication Bypass Tests" \
        "bin/test_security_auth_bypass" 30

    run_test_suite "Logging Evasion Tests" \
        "bin/test_security_logging_evasion" 30
    
    # Regression tests
    echo -e "\n${BLUE}=== Running Regression Tests ===${NC}"
    run_test_suite "File Locking System Tests" \
        "./tests/regression/test_file_locking_system.sh" 45

    run_test_suite "New Features Tests" \
        "./tests/regression/test_new_features.sh" 30

    run_test_suite "Secure Editor Fix Tests" \
        "./tests/regression/test_secure_editor_fix.sh" 30

    # Advanced security tests (may have some expected failures in test environment)
    echo -e "\n${BLUE}=== Running Advanced Security Tests ===${NC}"
    run_test_suite "Race Condition Tests" \
        "bin/test_security_race_conditions" 45

    run_test_suite "Logging Comprehensive Tests" \
        "bin/test_logging_comprehensive" 30
    
    cleanup_test_env
    
    # Print comprehensive summary
    echo -e "\n=============================================="
    echo -e "${BLUE}COMPREHENSIVE TEST SUMMARY${NC}"
    echo "=============================================="
    echo "Test Suites:"
    echo -e "  Total: $TOTAL_SUITES"
    echo -e "  Passed: ${GREEN}$PASSED_SUITES${NC}"
    echo -e "  Failed: ${RED}$((TOTAL_SUITES - PASSED_SUITES))${NC}"
    
    if [ "$TOTAL_TESTS" -gt 0 ]; then
        echo
        echo "Individual Tests:"
        echo -e "  Total: $TOTAL_TESTS"
        echo -e "  Passed: ${GREEN}$PASSED_TESTS${NC}"
        echo -e "  Failed: ${RED}$((TOTAL_TESTS - PASSED_TESTS))${NC}"
    fi
    
    echo
    echo "Coverage Areas Tested:"
    echo "  ✓ Command-line execution (sudo-like functionality)"
    echo "  ✓ Interactive shell mode"
    echo "  ✓ AI detection and blocking (Augment, Copilot, ChatGPT)"
    echo "  ✓ Ansible session detection and integration"
    echo "  ✓ Security validations and CVE protections"
    echo "  ✓ Authentication and authorization"
    echo "  ✓ Logging and audit capabilities"
    echo "  ✓ Shell enhancements and tab completion"
    echo "  ✓ Pipeline and command parsing"
    echo "  ✓ Privilege escalation protections"
    echo "  ✓ Command injection protections"
    echo "  ✓ File locking and concurrent access"
    
    echo
    if [ "$PASSED_SUITES" -eq "$TOTAL_SUITES" ]; then
        echo -e "${GREEN}🎉 ALL TEST SUITES PASSED!${NC}"
        echo -e "${GREEN}Sudosh is ready for production deployment.${NC}"
        exit 0
    else
        echo -e "${YELLOW}⚠️  Some test suites failed.${NC}"
        echo -e "${YELLOW}Review the output above for details.${NC}"
        echo -e "${YELLOW}Note: Some failures may be expected in test environments.${NC}"
        exit 1
    fi
}

# Run main function
main "$@"
