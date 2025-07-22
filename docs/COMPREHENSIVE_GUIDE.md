# Sudosh - Comprehensive Guide

**Author**: Branson Matheson <branson@sandsite.org>
**Note**: This project was primarily developed using Augment Code AI assistance

## Overview

Sudosh is a secure, interactive sudo shell that provides elevated privileges with extensive logging, security protections, and audit capabilities. This comprehensive guide covers all aspects of sudosh including features, security enhancements, implementation details, and troubleshooting.

## Key Features (v1.9.0)

- **Enhanced Tab Completion**: Comprehensive intelligent completion system with context awareness
- **Directory Path Completion Fix**: Critical fix for `ls /etc/<Tab>` behavior
- **Authentication Caching**: Secure credential caching similar to sudo (15-minute default)
- **Color Support**: Automatic color inheritance from calling shell's PS1 environment
- **Package Generation**: Comprehensive packaging for RPM and DEB based systems
- **Enhanced Security**: Multi-layered command validation and blocking
- **Comprehensive Logging**: Detailed audit trails and session management

## Table of Contents

1. [Core Features](#core-features)
2. [Security Features](#security-features)
3. [Interactive Enhancements](#interactive-enhancements)
4. [Target User Functionality](#target-user-functionality)
5. [Installation and Configuration](#installation-and-configuration)
6. [Usage Examples](#usage-examples)
7. [Security Testing](#security-testing)
8. [Troubleshooting](#troubleshooting)
9. [Development and Contributing](#development-and-contributing)

## Core Features

### Interactive Shell Environment
- **Command Prompt**: Professional `root@hostname:/path##` prompt
- **Session Management**: Complete session logging and audit trails
- **Command History**: Persistent history with timestamps in `~/.sudosh_history`
- **Built-in Commands**: `help`, `history`, `commands`, `cd`, `pwd`, `exit`

### Authentication and Authorization
- **PAM Integration**: Secure authentication using system PAM modules
- **Sudoers Validation**: Comprehensive sudoers file parsing and validation
- **Group Membership**: Support for `wheel` and `sudo` groups
- **SSSD Integration**: Enterprise authentication with Active Directory/LDAP

### Comprehensive Logging
- **Syslog Integration**: All commands logged to system syslog
- **Session Logging**: Optional complete session recording
- **Command Auditing**: Detailed command execution tracking
- **Security Events**: Authentication failures and security violations

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

### Enhanced Tab Completion System (v1.9.0)
Sudosh features a comprehensive intelligent tab completion system with context awareness:

#### **Empty Line Completion**
- **Behavior**: Pressing `<Tab>` on an empty line displays all available commands
- **Coverage**: Shows built-in commands and all executables in PATH
- **Display**: Clean multi-column format for easy browsing
- **Example**:
  ```
  sudosh:/home/user# <Tab>
  cat       cd        commands  cp        echo      exit      find      grep
  help      history   ls        mkdir     path      pwd       quit      rm
  rules     sort      version   vi        wc
  ```

#### **Context-Aware Argument Completion**
- **Command Arguments**: `ls <Tab>` shows files and directories in current directory
- **CD Command**: `cd <Tab>` shows directories only for efficient navigation
- **Smart Filtering**: Different completion behavior based on command context

#### **Directory Path Completion Fix**
- **Critical Fix**: `ls /etc/<Tab>` now displays all files in /etc/ instead of auto-completing to first entry
- **Preserved Behavior**: `ls /etc/host<Tab>` still auto-completes to matching files
- **Path Support**: Works with absolute paths (/etc/) and relative paths (src/)
- **Edge Cases**: Handles spaces, tabs, and complex nested paths correctly

#### **Completion Examples**
```bash
# Empty argument completion - shows files and directories
sudosh:/home/user# ls <Tab>
Documents/    Downloads/    Pictures/     file1.txt     file2.log

# CD command completion - shows directories only
sudosh:/home/user# cd <Tab>
Documents/    Downloads/    Pictures/     Projects/

# Directory path completion - shows directory contents
sudosh:/home/user# ls /etc/<Tab>
afpovertcp.cfg    aliases         aliases.db      apache2/
apparmor/         apparmor.d/     apport/         apt/

# Partial completion - auto-completes to matching files
sudosh:/home/user# ls /etc/host<Tab>
sudosh:/home/user# ls /etc/hosts
```

### Line Editing and Navigation
- **Arrow Key Navigation**: Up/Down arrows for command history
- **Line Editing**: Ctrl-A/E (beginning/end), Ctrl-B/F (back/forward)
- **Text Manipulation**: Ctrl-K (kill to end), Ctrl-U (kill line), Ctrl-D (delete/exit)
- **Enhanced Tab Completion**: Intelligent context-aware completion system

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
