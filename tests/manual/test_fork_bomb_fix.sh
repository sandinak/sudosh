#!/bin/bash

# Test script to verify fork bomb fix
echo "=== Testing Fork Bomb Fix ==="

# Create symlink
ln -sf bin/sudosh sudo

echo "1. Testing basic shell redirection (should not hang)..."
echo 'exit' | SUDOSH_TEST_MODE=1 timeout 5 ./sudo bash 2>&1 | head -3

echo ""
echo "2. Testing file locking warning behavior..."
SUDOSH_TEST_MODE=1 echo 'exit' | ./sudo -c 'echo test' 2>&1 | head -3

echo ""
echo "3. Testing that sudosh runs without hanging..."
echo 'exit' | SUDOSH_TEST_MODE=1 bin/sudosh 2>&1 | head -3

echo ""
echo "=== Fork Bomb Fix Test Complete ==="
