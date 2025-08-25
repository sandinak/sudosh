#!/bin/bash

# Test script for the enhanced 'which' command
# This script tests that the which command can identify both binaries in PATH and aliases

echo "Testing enhanced 'which' command functionality..."
echo "================================================"

# Create a temporary test environment
TEMP_DIR=$(mktemp -d)
ALIAS_FILE="$HOME/.sudosh_aliases"
BACKUP_ALIAS_FILE=""

# Backup existing alias file if it exists
if [ -f "$ALIAS_FILE" ]; then
    BACKUP_ALIAS_FILE="$ALIAS_FILE.backup.$$"
    cp "$ALIAS_FILE" "$BACKUP_ALIAS_FILE"
    echo "Backed up existing alias file to $BACKUP_ALIAS_FILE"
fi

# Create test aliases
cat > "$ALIAS_FILE" << 'EOF'
# Test aliases for which command
ll=ls -la
grep=grep --color=auto
la=ls -A
mytest=echo "This is a test alias"
EOF

echo "Created test aliases in $ALIAS_FILE"
echo

# Function to run sudosh command and capture output
run_which_test() {
    local cmd="$1"
    local description="$2"
    
    echo "Test: $description"
    echo "Command: which $cmd"
    
    # Use expect or a simple echo pipe to test the which command
    echo "which $cmd" | timeout 5 ./bin/sudosh 2>/dev/null | grep -v "sudosh>" | grep -v "Sudosh" | grep -v "Type" | grep -v "^$" | head -1
    echo
}

# Test cases
echo "Running test cases..."
echo "===================="

# Test 1: Built-in command
run_which_test "cd" "Built-in command"

# Test 2: Standard binary in PATH
run_which_test "ls" "Standard binary in PATH"

# Test 3: Alias
run_which_test "ll" "Alias (should show alias definition)"

# Test 4: Another alias
run_which_test "grep" "Another alias"

# Test 5: Non-existent command
run_which_test "nonexistentcommand123" "Non-existent command"

# Test 6: Multiple commands at once
echo "Test: Multiple commands"
echo "Command: which ls ll cd nonexistent"
echo "which ls ll cd nonexistent" | timeout 5 ./bin/sudosh 2>/dev/null | grep -v "sudosh>" | grep -v "Sudosh" | grep -v "Type" | grep -v "^$"
echo

# Cleanup
if [ -n "$BACKUP_ALIAS_FILE" ]; then
    mv "$BACKUP_ALIAS_FILE" "$ALIAS_FILE"
    echo "Restored original alias file"
else
    rm -f "$ALIAS_FILE"
    echo "Removed test alias file"
fi

rm -rf "$TEMP_DIR"

echo "Test completed!"
