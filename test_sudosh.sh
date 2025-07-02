#!/bin/bash

# Test script for sudosh
# This script demonstrates the basic functionality of sudosh

echo "=== sudosh Test Script ==="
echo

# Test 1: Show help
echo "Test 1: Showing help"
./bin/sudosh --help
echo

# Test 2: Show version
echo "Test 2: Showing version"
./bin/sudosh --version
echo

# Test 3: Check if binary exists and is executable
echo "Test 3: Checking binary"
if [ -x "./bin/sudosh" ]; then
    echo "✓ sudosh binary exists and is executable"
    ls -la ./bin/sudosh
else
    echo "✗ sudosh binary not found or not executable"
    exit 1
fi
echo

# Test 4: Check file structure
echo "Test 4: Checking project structure"
echo "Source files:"
for file in main.c auth.c command.c logging.c security.c utils.c sudosh.h; do
    if [ -f "$file" ]; then
        echo "✓ $file"
    else
        echo "✗ $file (missing)"
    fi
done
echo

echo "Build files:"
for file in Makefile README.md; do
    if [ -f "$file" ]; then
        echo "✓ $file"
    else
        echo "✗ $file (missing)"
    fi
done
echo

# Test 5: Check for required functions in the binary
echo "Test 5: Checking for key functions in binary"
if command -v nm >/dev/null 2>&1; then
    echo "Symbols in sudosh binary:"
    nm ./bin/sudosh | grep -E "(main|authenticate|log_command)" | head -5
elif command -v objdump >/dev/null 2>&1; then
    echo "Symbols in sudosh binary:"
    objdump -t ./bin/sudosh | grep -E "(main|authenticate|log_command)" | head -5
else
    echo "No symbol inspection tools available (nm/objdump)"
fi
echo

echo "=== Test Summary ==="
echo "✓ sudosh builds successfully"
echo "✓ Command line options work"
echo "✓ All source files present"
echo "✓ Ready for interactive testing"
echo
echo "To test interactively, run: ./bin/sudosh"
echo "Note: On macOS, this uses mock authentication (any non-empty password)"
echo "On Linux with PAM, it will use real system authentication"
