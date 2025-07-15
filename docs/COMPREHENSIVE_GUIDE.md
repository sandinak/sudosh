# Sudosh - User Guide

**Author**: Branson Matheson <branson@sandsite.org>
**Version**: 1.5.0

## Overview

Sudosh is a secure, interactive sudo shell that provides elevated privileges with extensive logging, security protections, and audit capabilities. This guide covers installation, usage, and administration of sudosh.

## Table of Contents

1. [Quick Start](#quick-start)
2. [Installation](#installation)
3. [Command Line Options](#command-line-options)
4. [Interactive Features](#interactive-features)
5. [Security Features](#security-features)
6. [Configuration](#configuration)
7. [Logging and Auditing](#logging-and-auditing)
8. [Troubleshooting](#troubleshooting)
9. [Advanced Usage](#advanced-usage)

## Quick Start

### Basic Usage
```bash
# Start sudosh with default settings
sudo sudosh

# Start with session logging to timestamped file
sudo sudosh -l

# Start with session logging to specific file
sudo sudosh -l /var/log/my-session.log

# Start with verbose output
sudo sudosh -v

# Run commands as specific user
sudo sudosh -u username
```

### First Time Setup
1. Ensure you're in the `wheel` or `sudo` group
2. Run `sudo sudosh` to start an interactive session
3. Use `help` command to see available features
4. Use `history` to see command history
5. Use `exit` or Ctrl-D to end session

## Installation

### Prerequisites
- Linux or macOS system
- GCC compiler
- PAM development libraries
- Make utility

### Build from Source
```bash
git clone https://github.com/sandinak/sudosh.git
cd sudosh
make
sudo make install
```

### Package Installation
```bash
# Debian/Ubuntu
sudo apt-get install sudosh

# Red Hat/CentOS
sudo yum install sudosh

# macOS with Homebrew
brew install sudosh
```

## Command Line Options

### Available Options
- `-h, --help`: Show help message and exit
- `--version`: Show version information and exit
- `-v, --verbose`: Enable verbose output for debugging
- `-l, --log-session [FILE]`: Log session to file (timestamped if no file specified)
- `-u, --user USER`: Run commands as target user (requires sudoers permission)

### Session Logging Examples
```bash
# Create timestamped log in current directory
sudosh -l
# Creates: sudosh-session-20250715-143022.log

# Log to specific file
sudosh -l /var/log/admin-session.log

# Combine with other options
sudosh -l -v -u postgres
```

## Security Features

### Enhanced Security Controls
- **Shell Blocking**: Prevents execution of interactive shells (`bash`, `sh`, `zsh`, etc.)
- **Dangerous Command Detection**: Warns about potentially dangerous operations
- **Path Validation**: Validates command paths and prevents path traversal
- **Input Sanitization**: Comprehensive input validation and sanitization

### System Protection
- **Critical Directory Protection**: Prevents operations in `/dev`, `/proc`, `/sys`
- **Recursive Operation Warnings**: Alerts for `rm -rf`, `chmod -R`, `chown -R`
- **Init System Protection**: Blocks direct init system manipulation
- **Environment Sanitization**: Cleans and secures environment variables

### Access Control
- **Privilege Validation**: Real-time privilege checking
- **Command Filtering**: Configurable command allow/deny lists
- **User Validation**: Comprehensive user existence and permission checking
- **Target User Support**: Run commands as specific users with proper validation

## Interactive Enhancements

### Line Editing and Navigation
- **Arrow Key Navigation**: Up/Down arrows for command history
- **Line Editing**: Ctrl-A/E (beginning/end), Ctrl-B/F (back/forward)
- **Text Manipulation**: Ctrl-K (kill to end), Ctrl-U (kill line), Ctrl-D (delete/exit)
- **Tab Completion**: Path completion for files and directories

### Command History
- **Immediate Availability**: Commands instantly available via up arrow
- **History Expansion**: `!42` (execute command #42), `!!` (last command)
- **Persistent Storage**: History preserved across sessions
- **Timestamped Entries**: All history entries include execution timestamps

### User Experience
- **Graceful Exit**: Ctrl-D exits cleanly with proper cleanup
- **Session Timeout**: 300-second inactivity timeout for security
- **Clear Feedback**: Informative messages and error reporting
- **Professional Interface**: Clean, intuitive command-line experience

## Target User Functionality

### Running Commands as Different Users
```bash
# Run sudosh as specific target user
sudo sudosh -u www-data

# Commands execute as target user
www-data@hostname:/path## whoami
www-data

www-data@hostname:/path## id
uid=33(www-data) gid=33(www-data) groups=33(www-data)
```

### Permission Validation
- **Sudoers Integration**: Validates user has permission to run as target user
- **Real-time Checking**: Permissions verified before command execution
- **Secure Switching**: Proper privilege dropping and environment setup
- **Audit Trail**: All target user operations logged with context

### Environment Management
- **Home Directory**: Proper HOME environment variable setting
- **User Context**: USER and LOGNAME variables updated
- **Group Membership**: Supplementary groups properly initialized
- **Shell Environment**: Clean, secure environment for target user

## Installation and Configuration

### Building from Source
```bash
# Clone repository
git clone https://github.com/sandinak/sudosh.git
cd sudosh

# Build
make

# Install (requires root)
sudo make install
```

### System Configuration
```bash
# Configure syslog (Linux)
echo ':programname, isequal, "sudosh" /var/log/sudosh.log' | sudo tee /etc/rsyslog.d/sudosh.conf
sudo systemctl restart rsyslog

# Add user to sudoers
sudo usermod -aG sudo username    # Debian/Ubuntu
sudo usermod -aG wheel username   # RHEL/CentOS
```

### Viewing Logs

#### Linux Systems
```bash
# View recent sudo activity
sudo journalctl -u sudo -f

# Traditional syslog locations
sudo tail -f /var/log/auth.log        # Debian/Ubuntu
sudo tail -f /var/log/secure          # RHEL/CentOS/Fedora

# Search for specific user
sudo grep "username" /var/log/auth.log
```

#### macOS Systems
```bash
# Real-time sudo monitoring
sudo log stream --predicate 'process == "sudo" OR process == "sudosh"'

# Recent activity
sudo log show --predicate 'process == "sudo"' --last 1h

# Export logs for analysis
sudo log collect --start "2024-12-15" --output sudo_logs.logarchive
```

## Usage Examples

### Basic Usage
```bash
# Start sudosh
sudo sudosh

# Execute commands
root@hostname:/home/user## ls -la /root
root@hostname:/home/user## systemctl status nginx
root@hostname:/home/user## cd /var/log
root@hostname:/var/log## pwd
root@hostname:/var/log## history
root@hostname:/var/log## exit
```

### Session Logging
```bash
# Start with session logging
sudo sudosh -l /tmp/admin_session_$(date +%Y%m%d_%H%M%S).log

# All input/output captured to file
```

### Target User Operations
```bash
# Run as specific user
sudo sudosh -u www-data

# Execute commands as target user
www-data@hostname:/path## systemctl --user status
www-data@hostname:/path## ls -la ~/
```

### History and Navigation
```bash
# Navigate command history
root@hostname:/path## ls -la
root@hostname:/path## pwd
root@hostname:/path## ↑    # Shows: pwd
root@hostname:/path## ↑    # Shows: ls -la

# Execute previous commands
root@hostname:/path## !1   # Execute first command in history
root@hostname:/path## !!   # Execute last command
```

## Security Testing

### Comprehensive Security Assessment
The sudosh security framework includes extensive testing for:

- **Authentication Bypass Attempts**: Validates all authentication mechanisms
- **Command Injection**: Tests input sanitization and command validation
- **Privilege Escalation**: Ensures proper privilege boundaries
- **Logging Evasion**: Verifies all actions are properly logged
- **Race Conditions**: Tests concurrent access and signal handling

### Security Test Categories
1. **Input Validation**: Null bytes, special characters, buffer overflows
2. **Command Filtering**: Shell execution, dangerous commands, path traversal
3. **Authentication**: PAM integration, password handling, session management
4. **Logging**: Audit trail completeness, log tampering prevention
5. **Environment**: Variable sanitization, path security, file permissions

### Running Security Tests
```bash
# Run comprehensive security assessment
make security-test

# Run specific security test categories
make test-auth-bypass
make test-command-injection
make test-privilege-escalation
```

## Troubleshooting

### Common Issues

#### Permission Denied
```bash
# Check sudosh permissions
ls -l /usr/local/bin/sudosh
# Should show: -rwsr-xr-x ... root root ... sudosh

# Fix permissions if needed
sudo chmod 4755 /usr/local/bin/sudosh
```

#### Authentication Issues
```bash
# Check PAM configuration
ls /etc/pam.d/sudo

# Verify user in sudoers
sudo -l -U username
```

#### Logging Problems
```bash
# Check syslog configuration
sudo systemctl status rsyslog

# Verify log permissions
ls -la /var/log/auth.log
```

### Debug Mode
```bash
# Build with debug symbols
make debug

# Run with verbose output
sudo ./bin/sudosh -v
```

## Development and Contributing

### Code Organization
```
sudosh/
├── src/           # Source code
├── tests/         # Test suite
├── docs/          # Documentation
└── Makefile       # Build system
```

### Testing Framework
- **Unit Tests**: Core functionality validation
- **Integration Tests**: End-to-end testing
- **Security Tests**: Comprehensive security validation
- **Performance Tests**: Resource usage and timing

### Contributing Guidelines
1. **Code Quality**: Follow existing style and conventions
2. **Testing**: All changes must include appropriate tests
3. **Documentation**: Update documentation for new features
4. **Security**: Security-related changes require thorough review

### Build Targets
```bash
make              # Build sudosh
make test         # Run all tests
make unit-test    # Run unit tests only
make security-test # Run security tests
make install      # Install system-wide
make clean        # Clean build files
```

## Conclusion

Sudosh provides a comprehensive, secure solution for interactive privileged operations with extensive logging and audit capabilities. Its combination of security features, user-friendly interface, and robust testing framework makes it suitable for both development and production environments.

For additional information, support, or to report issues, please refer to the project repository and documentation.

**Author**: Branson Matheson <branson@sandsite.org>
