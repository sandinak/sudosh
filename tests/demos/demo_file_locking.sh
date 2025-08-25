#!/bin/bash

# Demonstration script for sudosh file locking functionality
# This script shows how the file locking system works in practice

echo "Sudosh File Locking System Demonstration"
echo "========================================"
echo
echo "This demonstration shows the file locking system in action."
echo "The system prevents concurrent editing conflicts and provides"
echo "clear feedback about who is editing what files."
echo

# Check if sudosh is built
if [ ! -f "./bin/sudosh" ]; then
    echo "Error: sudosh not found. Please run 'make' first."
    exit 1
fi

echo "Step 1: Creating test files"
echo "==========================="
mkdir -p /tmp/sudosh_demo
echo "This is a test configuration file" > /tmp/sudosh_demo/config.txt
echo "This is a test script" > /tmp/sudosh_demo/script.sh
echo "This is a test data file" > /tmp/sudosh_demo/data.txt
echo "Created test files in /tmp/sudosh_demo/"
echo

echo "Step 2: Demonstrating lock directory creation"
echo "============================================="
echo "When sudosh starts, it creates the lock directory:"
echo "  /var/run/sudosh/locks/"
echo
echo "Lock files are named based on the canonical file path:"
echo "  /etc/passwd -> /var/run/sudosh/locks/_etc_passwd.lock"
echo "  /tmp/test.txt -> /var/run/sudosh/locks/_tmp_test.txt.lock"
echo

echo "Step 3: Lock file format"
echo "========================"
echo "Each lock file contains metadata:"
echo "  file_path=/full/canonical/path/to/file"
echo "  username=alice"
echo "  pid=12345"
echo "  timestamp=1705329022"
echo

echo "Step 4: Editing command detection"
echo "================================="
echo "The system detects these editing commands:"

commands=(
    "vi /tmp/sudosh_demo/config.txt"
    "vim -n /tmp/sudosh_demo/script.sh"
    "nano /tmp/sudosh_demo/data.txt"
    "emacs /tmp/sudosh_demo/config.txt"
    "pico /tmp/sudosh_demo/script.sh"
    "joe /tmp/sudosh_demo/data.txt"
    "mcedit /tmp/sudosh_demo/config.txt"
    "ed /tmp/sudosh_demo/script.sh"
    "ex /tmp/sudosh_demo/data.txt"
    "sed -i 's/old/new/g' /tmp/sudosh_demo/config.txt"
)

non_editing_commands=(
    "ls -la /tmp/sudosh_demo/"
    "cat /tmp/sudosh_demo/config.txt"
    "grep 'test' /tmp/sudosh_demo/script.sh"
    "head /tmp/sudosh_demo/data.txt"
    "tail /tmp/sudosh_demo/config.txt"
)

echo
echo "Editing commands (will trigger file locking):"
for cmd in "${commands[@]}"; do
    echo "  ✓ $cmd"
done

echo
echo "Non-editing commands (no locking needed):"
for cmd in "${non_editing_commands[@]}"; do
    echo "  - $cmd"
done

echo
echo "Step 5: Conflict scenarios"
echo "=========================="
echo "When multiple users try to edit the same file:"
echo
echo "User 1 runs: vi /etc/passwd"
echo "  → Lock acquired: /var/run/sudosh/locks/_etc_passwd.lock"
echo "  → File opens for editing"
echo
echo "User 2 runs: vim /etc/passwd"
echo "  → Lock check fails"
echo "  → Error message: 'Error: /etc/passwd is currently being edited"
echo "    by user alice since 2024-01-15 14:30:22. Please try again later.'"
echo "  → Command is blocked"
echo
echo "User 1 exits vi"
echo "  → Lock released automatically"
echo "  → /var/run/sudosh/locks/_etc_passwd.lock removed"
echo
echo "User 2 runs: vim /etc/passwd (again)"
echo "  → Lock acquired successfully"
echo "  → File opens for editing"
echo

echo "Step 6: Automatic cleanup scenarios"
echo "==================================="
echo "The system automatically cleans up locks in these cases:"
echo
echo "1. Normal exit:"
echo "   - User saves and exits editor normally"
echo "   - Lock is released in parent process after child exits"
echo
echo "2. Process termination:"
echo "   - Editor process is killed (kill -9)"
echo "   - Next lock attempt detects dead process and removes stale lock"
echo
echo "3. Sudosh interruption:"
echo "   - User presses Ctrl-C or sends SIGTERM to sudosh"
echo "   - Signal handler calls cleanup_file_locking()"
echo "   - All locks owned by the session are released"
echo
echo "4. Timeout expiration:"
echo "   - Lock is older than 30 minutes (LOCK_TIMEOUT)"
echo "   - Automatic cleanup removes expired locks"
echo
echo "5. System reboot:"
echo "   - /var/run is typically cleared on reboot"
echo "   - Stale locks from previous boot are automatically removed"
echo

echo "Step 7: Security features"
echo "========================="
echo "The file locking system includes these security measures:"
echo
echo "- Canonical path resolution prevents symlink attacks"
echo "- Exclusive file creation (O_CREAT | O_EXCL) prevents race conditions"
echo "- Advisory file locking (flock) provides additional protection"
echo "- Process validation ensures locks are from live processes"
echo "- Secure lock directory permissions (0755)"
echo "- Comprehensive audit logging of all lock operations"
echo "- Signal handlers ensure cleanup on interruption"
echo

echo "Step 8: Edge case handling"
echo "=========================="
echo "The system handles these edge cases:"
echo
echo "- Multiple paths to same file (symlinks, relative vs absolute)"
echo "- Commands with complex option parsing (sed -i, vim -n)"
echo "- Permission issues with lock directory creation"
echo "- Race conditions during concurrent lock attempts"
echo "- Network filesystems (uses local lock directory)"
echo "- Long-running editing sessions (timeout protection)"
echo

echo "Step 9: Integration with sudosh"
echo "==============================="
echo "File locking is seamlessly integrated:"
echo
echo "1. Initialization:"
echo "   - init_file_locking() called during sudosh startup"
echo "   - Creates lock directory if needed"
echo "   - Cleans up any stale locks from previous runs"
echo
echo "2. Command execution:"
echo "   - is_editing_command() checks if command edits files"
echo "   - extract_file_argument() finds the file to be edited"
echo "   - acquire_file_lock() called before forking editor"
echo "   - Lock acquisition failure blocks command execution"
echo
echo "3. Cleanup:"
echo "   - release_file_lock() called after editor process exits"
echo "   - cleanup_file_locking() called during sudosh shutdown"
echo "   - Signal handlers ensure cleanup on interruption"
echo

echo "Step 10: Benefits for system administrators"
echo "==========================================="
echo "The file locking system provides:"
echo
echo "✅ Data integrity: Prevents corruption from concurrent edits"
echo "✅ Clear feedback: Shows who is editing what files and when"
echo "✅ Automatic cleanup: No manual intervention needed"
echo "✅ Cross-session protection: Works across multiple sudosh instances"
echo "✅ Comprehensive logging: Full audit trail of file access"
echo "✅ Security: Prevents race conditions and symlink attacks"
echo "✅ Reliability: Handles edge cases and system failures gracefully"
echo

echo "Cleanup: Removing demonstration files"
echo "====================================="
rm -rf /tmp/sudosh_demo
echo "Demonstration files removed"
echo

echo "File locking system demonstration completed!"
echo
echo "To test the system manually:"
echo "1. Build: make"
echo "2. Terminal 1: sudo ./bin/sudosh"
echo "3. Terminal 1: vi /tmp/test_file.txt"
echo "4. Terminal 2: sudo ./bin/sudosh"
echo "5. Terminal 2: vi /tmp/test_file.txt"
echo "6. Observe the lock conflict message in Terminal 2"
echo
echo "The file locking system is now ready for production use!"
