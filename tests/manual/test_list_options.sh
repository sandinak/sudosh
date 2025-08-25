#!/bin/bash

echo "=== Testing Enhanced -l and -ll Options ==="

echo ""
echo "1. Testing -l option (basic rules only):"
echo "   Should show sudo rules without command categories"
bin/sudosh -l 2>&1 | head -10

echo ""
echo "2. Testing -ll option (detailed rules + categories):"
echo "   Should show sudo rules AND command categories"
bin/sudosh -ll 2>&1 | head -10

echo ""
echo "3. Verifying -l does NOT include command categories:"
if bin/sudosh -l 2>&1 | grep -q "Always safe commands"; then
    echo "   ❌ FAIL: -l option includes command categories (should not)"
else
    echo "   ✅ PASS: -l option shows only rules"
fi

echo ""
echo "4. Verifying -ll DOES include command categories:"
if bin/sudosh -ll 2>&1 | grep -q "Always safe commands"; then
    echo "   ✅ PASS: -ll option includes command categories"
else
    echo "   ❌ FAIL: -ll option missing command categories"
fi

echo ""
echo "5. Testing help text for new options:"
bin/sudosh --help 2>&1 | grep -E "^\s*-ll?\s"

echo ""
echo "6. Testing interactive rules command (should be detailed):"
if echo 'rules' | SUDOSH_TEST_MODE=1 bin/sudosh 2>&1 | grep -q "Always safe commands"; then
    echo "   ✅ PASS: Interactive rules command shows detailed output"
else
    echo "   ❌ FAIL: Interactive rules command missing categories"
fi

echo ""
echo "7. Testing --list long option:"
if bin/sudosh --list 2>&1 | grep -q "Sudo privileges"; then
    echo "   ✅ PASS: --list option works"
else
    echo "   ❌ FAIL: --list option not working"
fi

echo ""
echo "=== Enhanced List Options Test Complete ==="
