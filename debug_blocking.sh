#!/bin/bash

# Debug script to understand why all commands are being blocked
echo "=== Debug Blocking Issue ==="

# Try to understand what's happening
echo "1. Current process info:"
echo "   PID: $$"
echo "   PPID: $PPID"

echo "2. Environment check:"
echo "   SHELL: $SHELL"
echo "   PATH: $PATH"

echo "3. Process tree:"
/bin/ps -o pid,ppid,cmd -p $$ 2>/dev/null || echo "Could not get process info"

echo "4. Check if sudosh is in PATH:"
/usr/bin/which sudosh 2>/dev/null || echo "sudosh not found in PATH"

echo "5. Check sudo location:"
/bin/ls -la /usr/bin/sudo 2>/dev/null || echo "sudo not found"

echo "6. Check for aliases or functions:"
/usr/bin/type sudo 2>/dev/null || echo "Could not check sudo type"

echo "7. Try direct execution:"
/usr/bin/sudo --version 2>/dev/null || echo "Could not run sudo directly"

echo "=== End Debug ==="
