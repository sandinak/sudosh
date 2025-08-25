#!/bin/bash

echo "=== Testing New Security Model ==="

echo ""
echo "1. Testing rules command display (should show ALL/ANY indicators):"
echo 'rules' | SUDOSH_TEST_MODE=1 bin/sudosh 2>&1 | grep -A 5 "Direct Sudoers Rules"

echo ""
echo "2. Testing safe commands section header:"
echo 'rules' | SUDOSH_TEST_MODE=1 bin/sudosh 2>&1 | grep "Always safe commands"

echo ""
echo "3. Testing conditionally blocked command (mount - should work with ALL privileges):"
echo 'mount' | SUDOSH_TEST_MODE=1 bin/sudosh 2>&1 | grep -v "Warning:" | tail -1

echo ""
echo "4. Testing always blocked command (sudo - should be blocked):"
echo 'sudo ls' | SUDOSH_TEST_MODE=1 bin/sudosh 2>&1 | grep "privilege escalation"

echo ""
echo "5. Testing shell redirection (bash via sudo symlink):"
echo 'bash' | SUDOSH_TEST_MODE=1 ./sudo 2>&1 | grep "redirecting"

echo ""
echo "6. Testing new blocked commands section:"
echo 'rules' | SUDOSH_TEST_MODE=1 bin/sudosh 2>&1 | grep -A 3 "Conditionally Blocked Commands"

echo ""
echo "=== Security Model Test Complete ==="
