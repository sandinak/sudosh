# sudosh Demo

This document demonstrates the functionality of sudosh, an interactive sudo shell with comprehensive logging.

## Features Demonstrated

### 1. Command Line Interface
```bash
# Show help
./bin/sudosh --help

# Show version
./bin/sudosh --version
```

### 2. Interactive Shell
When you run `./bin/sudosh`, it will:

1. **Check User Privileges**: Verify the user is in the wheel/sudo group
2. **Authenticate**: Prompt for password (mock auth on macOS, PAM on Linux)
3. **Provide Interactive Shell**: Give you a `sudosh# ` prompt
4. **Log All Activity**: Every command is logged to syslog

### 3. Built-in Commands
- `help` - Show available commands
- `exit` or `quit` - Exit the shell
- Any other command - Execute with root privileges

### 4. Security Features

#### Environment Sanitization
- Removes dangerous environment variables (LD_PRELOAD, etc.)
- Sets secure PATH
- Sets appropriate umask

#### Command Validation
- Checks for null bytes (injection attempts)
- Validates command length
- Detects path traversal attempts

#### Logging
All activities are logged to syslog with facility `LOG_AUTHPRIV`:
- Authentication attempts (success/failure)
- Session start/end
- Command executions with full context (TTY, PWD, USER)
- Security violations

## Example Session

```bash
$ ./bin/sudosh
We trust you have received the usual lecture from the local System
Administrator. It usually boils down to these three things:

    #1) Respect the privacy of others.
    #2) Think before you type.
    #3) With great power comes great responsibility.

Mock authentication for user: branson
Password: [hidden]
sudosh 1.0.0 - Interactive sudo shell
Type 'help' for available commands, 'exit' to quit.

sudosh# help
sudosh - Interactive sudo shell

Available commands:
  help          - Show this help message
  exit, quit    - Exit sudosh
  <command>     - Execute command as root

Examples:
  ls -la /root
  systemctl status nginx
  apt update

All commands are logged to syslog.

sudosh# ls -la /tmp
[Command executes with root privileges and output is shown]

sudosh# whoami
root

sudosh# exit
Goodbye!
```

## Logging Output

Commands are logged to syslog in the same format as sudo:

```
Jul  2 15:20:01 hostname sudosh[1234]: branson : TTY=ttys001 ; PWD=/home/branson ; USER=root ; COMMAND=ls -la /tmp
Jul  2 15:20:05 hostname sudosh[1234]: branson : TTY=ttys001 ; PWD=/home/branson ; USER=root ; COMMAND=whoami
```

## Platform Differences

### Linux (with PAM)
- Uses real PAM authentication
- Integrates with system authentication (LDAP, Kerberos, etc.)
- Full sudo-like behavior

### macOS (Mock Authentication)
- Uses mock authentication for demonstration
- Accepts any non-empty password
- All other functionality identical

## Security Considerations

1. **Setuid Root**: For production use, install with setuid root permissions
2. **Group Membership**: Users must be in wheel/sudo group
3. **Logging**: All activity is logged for audit purposes
4. **Environment**: Dangerous environment variables are sanitized
5. **Validation**: Commands are validated for security issues

## Installation for Production

```bash
# Build
make

# Install (requires root)
sudo make install

# The binary will be installed as:
# -rwsr-xr-x root root /usr/local/bin/sudosh
```

## Viewing Logs

```bash
# On systems with journald
sudo journalctl -t sudosh

# On systems with traditional syslog
sudo tail -f /var/log/auth.log
sudo tail -f /var/log/secure
```

## Comparison with sudo

| Feature | sudo | sudosh |
|---------|------|--------|
| Authentication | PAM | PAM (Linux) / Mock (macOS) |
| Logging | syslog | syslog |
| Interactive Shell | No | Yes |
| Command Validation | Basic | Enhanced |
| Environment Sanitization | Yes | Yes |
| Group Checking | /etc/sudoers | wheel/sudo group |

sudosh provides a more interactive experience while maintaining the security and logging features expected from a privilege escalation tool.
