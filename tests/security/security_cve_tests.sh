#!/bin/bash

# Sudosh CVE Security Test Suite
# Tests for protection against known bash CVE vulnerabilities

set -e

# Ensure non-interactive behavior for tests
export SUDOSH_TEST_MODE=1

SUDOSH_BIN="./bin/sudosh"
TEST_LOG="/tmp/sudosh_security_test.log"
FAILED_TESTS=0
TOTAL_TESTS=0

# Helper: run with timeout if available (supports macOS gtimeout)
run_with_timeout() {
    local seconds="$1"; shift
    if command -v timeout >/dev/null 2>&1; then
        set +e
        SUDOSH_TEST_MODE=1 timeout "$seconds" "$@"
        local status=$?
        set -e
        return $status
    elif command -v gtimeout >/dev/null 2>&1; then
        set +e
        SUDOSH_TEST_MODE=1 gtimeout "$seconds" "$@"
        local status=$?
        set -e
        return $status
    else
        # Start the command; prefer setsid if available to kill process group, else just run in background
        if command -v setsid >/dev/null 2>&1; then
            SUDOSH_TEST_MODE=1 setsid "$@" &
            local cmd_pid=$!
            local pgid
            pgid=$(ps -o pgid= -p "$cmd_pid" | tr -d ' ')
            (
                trap 'exit 0' TERM INT
                sleep "$seconds"
                if kill -0 "$cmd_pid" >/dev/null 2>&1; then
                    kill -TERM -"$pgid" >/dev/null 2>&1 || kill -TERM "$cmd_pid" >/dev/null 2>&1 || true
                    sleep 1
                    kill -KILL -"$pgid" >/dev/null 2>&1 || kill -KILL "$cmd_pid" >/dev/null 2>&1 || true
                fi
            ) >/dev/null 2>&1 &
            local watcher_pid=$!
            wait "$cmd_pid"
            local status=$?
            kill "$watcher_pid" >/dev/null 2>&1 || true
            return $status
        else
            SUDOSH_TEST_MODE=1 "$@" &
            local cmd_pid=$!
            (
                trap 'exit 0' TERM INT
                sleep "$seconds"
                if kill -0 "$cmd_pid" >/dev/null 2>&1; then
                    kill -TERM "$cmd_pid" >/dev/null 2>&1 || true
                    sleep 1
                    kill -KILL "$cmd_pid" >/dev/null 2>&1 || true
                fi
            ) >/dev/null 2>&1 &
            local watcher_pid=$!
            wait "$cmd_pid"
            local status=$?
            kill "$watcher_pid" >/dev/null 2>&1 || true
            return $status
        fi
    fi
}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=== Sudosh CVE Security Test Suite ==="
echo "Testing protection against known bash CVE vulnerabilities"
echo

# Test helper functions
run_test() {
    local test_name="$1"
    local test_command="$2"
    local expected_result="$3"  # "BLOCKED" or "ALLOWED"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -n "Testing $test_name... "

    # Choose a slightly longer timeout for expected ALLOWED commands
    local timeout_seconds=6
    if [ "$expected_result" = "ALLOWED" ]; then
        timeout_seconds=12
    fi

    # Capture outputs and exit code
    local stdout_file stderr_file status result
    stdout_file=$(mktemp)
    stderr_file=$(mktemp)

    set +e
    run_with_timeout "$timeout_seconds" $SUDOSH_BIN -c "$test_command" >"$stdout_file" 2>"$stderr_file"
    status=$?
    set -e
    if [ $status -eq 0 ]; then
        result="ALLOWED"
    else
        result="BLOCKED"
    fi

    if [ "$result" = "$expected_result" ]; then
        echo -e "${GREEN}PASS${NC}"
    else
        echo -e "${RED}FAIL${NC} (Expected: $expected_result, Got: $result)"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        {
          echo "  Command: $test_command"
          echo "  Expected: $expected_result, Got: $result"
          echo "  Exit code: $status"
          if [ -s "$stderr_file" ]; then
            echo "  Stderr:"
            sed 's/^/    /' "$stderr_file"
          else
            echo "  Stderr: (empty)"
          fi
        } >> "$TEST_LOG"
    fi

    # Cleanup
    rm -f "$stdout_file" "$stderr_file"
}

# CVE-2014-6271 (Shellshock) Tests
echo "=== CVE-2014-6271 (Shellshock) Tests ==="
run_test "Shellshock environment injection" "env x='() { :;}; echo vulnerable' bash -c 'echo test'" "BLOCKED"
run_test "Shellshock function definition" "bash -c '() { :;}; echo vulnerable'" "BLOCKED"
run_test "Shellshock with command substitution" "bash -c 'env x=\'() { :;}; \$(echo vulnerable)\' bash'" "BLOCKED"

# CVE-2014-7169 (Shellshock Variant) Tests  
echo "=== CVE-2014-7169 (Shellshock Variant) Tests ==="
# Using printf to safely construct tricky quotes without breaking the script
run_test "Shellshock incomplete fix bypass" "$(printf "env x='() { (a)=>\'' bash -c 'echo test'")" "BLOCKED"
run_test "Shellshock variant with redirection" "bash -c 'env x=\'() { _; } >_[\$(\$(echo vulnerable))]\' bash'" "BLOCKED"

# CVE-2014-7186 & CVE-2014-7187 (Memory Corruption) Tests
echo "=== CVE-2014-7186 & CVE-2014-7187 (Memory Corruption) Tests ==="
run_test "Bash parser overflow attempt" "bash -c 'for((i=0;i<10000;i++)); do echo \$i; done'" "BLOCKED"
run_test "Malformed input buffer overflow" "bash -c '\$(printf \"%*s\" 10000 \"\")'" "BLOCKED"

# CVE-2019-9924 (rbash Bypass) Tests
echo "=== CVE-2019-9924 (rbash Bypass) Tests ==="
run_test "BASH_CMDS modification attempt" "BASH_CMDS[a]=/bin/sh; a" "BLOCKED"
run_test "rbash bypass via environment" "env BASH_CMDS[sh]=/bin/bash sh" "BLOCKED"

# CVE-2022-3715 (Heap Buffer Overflow) Tests
echo "=== CVE-2022-3715 (Heap Buffer Overflow) Tests ==="
run_test "Parameter expansion overflow" "echo \${x:0:999999999}" "BLOCKED"
run_test "Malformed parameter transformation" "echo \${x//\$(printf \"%*s\" 1000 \"\")/y}" "BLOCKED"
run_test "Unicode escape overflow" "echo \$'\\u{41414141}'" "BLOCKED"

# CVE-2023-22809 (Sudoedit Privilege Escalation) Tests
echo "=== CVE-2023-22809 (Sudoedit Privilege Escalation) Tests ==="
run_test "Sudoedit bypass attempt" "sudoedit /etc/passwd" "BLOCKED"
run_test "Sudoedit with EDITOR manipulation" "EDITOR='sh -c \"id; sh\"' sudoedit /tmp/test" "BLOCKED"
run_test "Sudoedit with VISUAL manipulation" "VISUAL='sh -c \"id; sh\"' sudoedit /tmp/test" "BLOCKED"
run_test "Sudoedit environment injection" "env EDITOR='/bin/sh -c id' sudoedit /tmp/test" "BLOCKED"

# CVE-2024 Environment Variable Injection Tests
echo "=== CVE-2024 Environment Variable Injection Tests ==="
run_test "SUDO_EDITOR injection" "SUDO_EDITOR='sh -c \"id; sh\"' sudo -e /tmp/test" "BLOCKED"
run_test "Complex environment bypass" "env -i EDITOR='sh -c id' sudoedit /tmp/test" "BLOCKED"
run_test "PATH manipulation with sudoedit" "PATH=/tmp:\$PATH EDITOR=malicious sudoedit /tmp/test" "BLOCKED"

# Additional Security Tests
echo "=== Additional Security Tests ==="
run_test "Command injection via semicolon" "ls; cat /etc/passwd" "BLOCKED"
run_test "Command injection via pipe" "ls | cat /etc/passwd" "BLOCKED"
run_test "Command injection via ampersand" "ls & cat /etc/passwd" "BLOCKED"
run_test "Command injection via backticks" "ls \`cat /etc/passwd\`" "BLOCKED"
run_test "Command injection via dollar-paren" "ls \$(cat /etc/passwd)" "BLOCKED"
run_test "Path traversal attempt" "cat ../../etc/passwd" "BLOCKED"
run_test "Null byte injection" "cat /etc/passwd\x00.txt" "BLOCKED"

# Environment Variable Security Tests
echo "=== Environment Variable Security Tests ==="
run_test "BASH_ENV exploitation attempt" "BASH_ENV=/tmp/malicious.sh ls" "BLOCKED"
run_test "LD_PRELOAD attempt" "LD_PRELOAD=/tmp/malicious.so ls" "BLOCKED"
run_test "PATH manipulation" "PATH=/tmp:\$PATH malicious_command" "BLOCKED"

# Safe Commands Tests (should be allowed)
echo "=== Safe Commands Tests ==="
run_test "Simple ls command" "ls" "ALLOWED"
# Skip cat/less/more on systems where /etc/hostname may not exist or be restricted
if [ -r /etc/hostname ]; then
  run_test "Cat with safe file" "cat /etc/hostname" "ALLOWED"
  run_test "Less with safe file" "less /etc/hostname" "ALLOWED"
  run_test "More with safe file" "more /etc/hostname" "ALLOWED"
fi
run_test "Echo with safe text" "echo 'Hello World'" "ALLOWED"
run_test "Date command" "date" "ALLOWED"
run_test "Whoami command" "whoami" "ALLOWED"

# Results Summary
echo
echo "=== Test Results Summary ==="
echo "Total Tests: $TOTAL_TESTS"
echo "Passed: $((TOTAL_TESTS - FAILED_TESTS))"
echo "Failed: $FAILED_TESTS"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}All tests passed! Sudosh is protected against tested CVE vulnerabilities.${NC}"
    exit 0
else
    echo -e "${RED}$FAILED_TESTS tests failed. Check $TEST_LOG for details.${NC}"
    echo "Failed test details:"
    cat $TEST_LOG
    exit 1
fi
