#!/bin/bash

# Test script to verify help system shows sudo CLI options in sudo mode

echo "=== Testing Help System Sudo Options Display ==="
echo

# Test 1: Normal mode should NOT show sudo options
echo "Test 1: Normal mode help (should NOT show sudo CLI options)"
echo "-------------------------------------------------------"
NORMAL_OUTPUT=$(echo "help" | SUDOSH_TEST_MODE=1 ./bin/sudosh 2>/dev/null)

if echo "$NORMAL_OUTPUT" | grep -q "Sudo-compatible CLI options"; then
    echo "❌ FAIL: Normal mode shows sudo CLI options (should not)"
    exit 1
else
    echo "✅ PASS: Normal mode does not show sudo CLI options"
fi

if echo "$NORMAL_OUTPUT" | grep -q "Available built-in commands:"; then
    echo "✅ PASS: Normal mode shows built-in commands"
else
    echo "❌ FAIL: Normal mode does not show built-in commands"
    exit 1
fi

echo

# Test 2: Sudo mode should show sudo options
echo "Test 2: Sudo mode help (should show sudo CLI options)"
echo "-----------------------------------------------------"
SUDO_OUTPUT=$(echo "help" | SUDOSH_TEST_MODE=1 ./sudo 2>/dev/null)

if echo "$SUDO_OUTPUT" | grep -q "Sudo-compatible CLI options"; then
    echo "✅ PASS: Sudo mode shows sudo CLI options section"
else
    echo "❌ FAIL: Sudo mode does not show sudo CLI options section"
    exit 1
fi

if echo "$SUDO_OUTPUT" | grep -q "\-h, \-\-help"; then
    echo "✅ PASS: Sudo mode shows -h, --help option"
else
    echo "❌ FAIL: Sudo mode does not show -h, --help option"
    exit 1
fi

if echo "$SUDO_OUTPUT" | grep -q "\-l, \-\-list"; then
    echo "✅ PASS: Sudo mode shows -l, --list option"
else
    echo "❌ FAIL: Sudo mode does not show -l, --list option"
    exit 1
fi

if echo "$SUDO_OUTPUT" | grep -q "unsupported for security policy compliance"; then
    echo "✅ PASS: Sudo mode shows unsupported options note"
else
    echo "❌ FAIL: Sudo mode does not show unsupported options note"
    exit 1
fi

if echo "$SUDO_OUTPUT" | grep -q "Available built-in commands:"; then
    echo "✅ PASS: Sudo mode shows built-in commands"
else
    echo "❌ FAIL: Sudo mode does not show built-in commands"
    exit 1
fi

echo

# Test 3: Test '?' shortcut works the same way
echo "Test 3: '?' shortcut in sudo mode"
echo "--------------------------------"
QUESTION_OUTPUT=$(echo "?" | SUDOSH_TEST_MODE=1 ./sudo 2>/dev/null)

if echo "$QUESTION_OUTPUT" | grep -q "Sudo-compatible CLI options"; then
    echo "✅ PASS: '?' shortcut shows sudo CLI options in sudo mode"
else
    echo "❌ FAIL: '?' shortcut does not show sudo CLI options in sudo mode"
    exit 1
fi

echo

# Test 4: Verify specific sudo options are documented
echo "Test 4: Verify specific sudo options are documented"
echo "--------------------------------------------------"

# Check for key sudo options
if echo "$SUDO_OUTPUT" | grep -q "\-h.*help"; then
    echo "✅ PASS: Found -h/--help option"
else
    echo "❌ FAIL: Missing -h/--help option"
    exit 1
fi

if echo "$SUDO_OUTPUT" | grep -q "\-V.*version"; then
    echo "✅ PASS: Found -V/--version option"
else
    echo "❌ FAIL: Missing -V/--version option"
    exit 1
fi

if echo "$SUDO_OUTPUT" | grep -q "\-l.*list"; then
    echo "✅ PASS: Found -l/--list option"
else
    echo "❌ FAIL: Missing -l/--list option"
    exit 1
fi

if echo "$SUDO_OUTPUT" | grep -q "\-\-verbose"; then
    echo "✅ PASS: Found --verbose option"
else
    echo "❌ FAIL: Missing --verbose option"
    exit 1
fi

echo

echo "=== All Tests Passed! ==="
echo
echo "Summary:"
echo "✅ Normal mode help works correctly (no sudo options shown)"
echo "✅ Sudo mode help shows sudo CLI options section"
echo "✅ All expected sudo options are documented"
echo "✅ Unsupported options note is shown"
echo "✅ Built-in commands are shown in both modes"
echo "✅ '?' shortcut works identically to 'help'"
echo
echo "The help system enhancement is working correctly!"
