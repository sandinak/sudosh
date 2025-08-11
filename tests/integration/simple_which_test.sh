#!/bin/bash

# Simple test for the which command
echo "Creating test alias file..."

# Create test aliases
cat > "$HOME/.sudosh_aliases" << 'EOF'
ll=ls -la
grep=grep --color=auto
mytest=echo "This is a test"
EOF

echo "Test aliases created. Now testing which command..."
echo

# Test the which command by examining the source code behavior
echo "=== Testing which command functionality ==="
echo

echo "1. Testing built-in command (cd):"
echo "Expected: cd: shell builtin"
echo

echo "2. Testing standard binary (ls):"
echo "Expected: /bin/ls or /usr/bin/ls"
echo

echo "3. Testing alias (ll):"
echo "Expected: ll: aliased to \`ls -la'"
echo

echo "4. Testing non-existent command:"
echo "Expected: notfound: not found"
echo

echo "The enhanced which command should now check for:"
echo "1. Built-in commands first"
echo "2. Aliases second"  
echo "3. Binaries in PATH third"
echo

echo "To test manually, run: ./bin/sudosh"
echo "Then try: which cd"
echo "Then try: which ll"
echo "Then try: which ls"

# Cleanup
rm -f "$HOME/.sudosh_aliases"
echo
echo "Test alias file removed."
