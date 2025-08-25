#!/bin/bash

# Demo script for the enhanced 'which' command
# This script demonstrates the enhanced which command that identifies both binaries and aliases

echo "=== Enhanced 'which' Command Demonstration ==="
echo "=============================================="
echo
echo "This demonstration shows the enhanced 'which' command that can identify:"
echo "1. Built-in shell commands"
echo "2. Aliases"
echo "3. Binaries in PATH"
echo
echo "The implementation is in sudosh/src/shell_env.c in the handle_which_command() function."
echo

# Create test aliases file
ALIAS_FILE="$HOME/.sudosh_aliases"
BACKUP_FILE=""

# Backup existing aliases if they exist
if [ -f "$ALIAS_FILE" ]; then
    BACKUP_FILE="$ALIAS_FILE.demo_backup.$$"
    cp "$ALIAS_FILE" "$BACKUP_FILE"
    echo "Backed up existing aliases to $BACKUP_FILE"
fi

# Create demo aliases
cat > "$ALIAS_FILE" << 'EOF'
# Demo aliases for enhanced which command
ll=ls -la
la=ls -A
grep=grep --color=auto
mygrep=grep -n --color=always
demo=echo "This is a demo alias"
EOF

echo "Created demo aliases:"
cat "$ALIAS_FILE"
echo

echo "=== Code Changes Made ==="
echo "========================"
echo
echo "Modified sudosh/src/shell_env.c in handle_which_command() function:"
echo "- Added alias checking using get_alias_value() function"
echo "- Enhanced the search order: builtins -> aliases -> PATH binaries"
echo "- Added proper output formatting for aliases"
echo
echo "The enhanced logic now follows this order:"
echo "1. Check if command is a built-in (cd, pwd, etc.)"
echo "2. Check if command is an alias"
echo "3. Check if command is a binary in PATH"
echo "4. Report 'not found' if none match"
echo

echo "=== Expected Behavior ==="
echo "========================"
echo
echo "When you run the enhanced 'which' command, you should see:"
echo
echo "which cd          -> cd: shell builtin"
echo "which ll          -> ll: aliased to \`ls -la'"
echo "which ls          -> /bin/ls (or /usr/bin/ls)"
echo "which nonexistent -> nonexistent: not found"
echo

echo "=== Manual Testing Instructions ==="
echo "=================================="
echo
echo "To test the enhanced which command:"
echo "1. Run: ./bin/sudosh"
echo "2. Try these commands:"
echo "   which cd"
echo "   which ll"
echo "   which la"
echo "   which grep"
echo "   which ls"
echo "   which nonexistent"
echo "   which cd ll ls nonexistent"
echo "3. Type 'exit' to quit sudosh"
echo

echo "=== Implementation Details ==="
echo "============================"
echo
echo "The enhanced which command implementation:"
echo "- Uses existing alias system (get_alias_value function)"
echo "- Maintains compatibility with standard which behavior"
echo "- Supports multiple command arguments"
echo "- Provides clear output format for each command type"
echo
echo "Files modified:"
echo "- sudosh/src/shell_env.c: Enhanced handle_which_command() function"
echo "- sudosh/src/sudosh.h: Uncommented extern char **environ declaration"
echo

# Cleanup function
cleanup() {
    if [ -n "$BACKUP_FILE" ]; then
        mv "$BACKUP_FILE" "$ALIAS_FILE"
        echo "Restored original aliases"
    else
        rm -f "$ALIAS_FILE"
        echo "Removed demo aliases"
    fi
}

# Set up cleanup on exit
trap cleanup EXIT

echo "Demo aliases are active. You can now test the enhanced which command."
echo "Run './bin/sudosh' to start the shell and test the functionality."
echo
echo "Press Ctrl+C to exit this demo and clean up the aliases."

# Keep the script running so aliases remain active
while true; do
    sleep 1
done
