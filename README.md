# Sudosh - Secure Interactive Sudo Shell

**Author**: Branson Matheson <branson@sandsite.org>
**Development**: This project was primarily developed using [Augment Code](https://www.augmentcode.com) AI assistance

[![Version](https://img.shields.io/badge/version-1.9.1-blue.svg)](https://github.com/sandinak/sudosh)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Security](https://img.shields.io/badge/security-enhanced-red.svg)](docs/ENHANCED_SECURITY_FEATURES.md)

Sudosh is a comprehensive, secure interactive shell that provides elevated privileges with extensive logging, security protections, and audit capabilities. It's designed for system administrators who need secure, monitored access to privileged operations.

## 🚀 **Features**

### **Core Functionality**
- **Interactive sudo shell** with comprehensive privilege management
- **Authentication caching** - Secure credential caching similar to sudo (15-minute default)
- **Color support** - Automatic color inheritance from calling shell's PS1 environment
- **Target user support** - Run commands as specific users with `-u {user}`
- **Enhanced security** - Shell blocking, dangerous command detection, system directory protection
- **Comprehensive logging** - All commands logged to syslog and optional session files
- **Command history** - Full history with arrow key navigation and `!{number}` recall
- **Enhanced tab completion** - Comprehensive intelligent completion system
  - **Empty line completion** - `<Tab>` on empty line shows all available commands
  - **Context-aware completion** - Different behavior for commands vs arguments
  - **Directory path completion** - `ls /etc/<Tab>` displays directory contents
  - **CD command optimization** - Shows directories only for `cd` command
  - **Clean column formatting** - Multi-column display for better readability
- **Executable filtering** - Tab completion shows only executables in command position
- **Package generation** - Professional RPM and DEB packages for easy distribution

### **Security Features**
- 🔒 **Shell command blocking** - Prevents bash, sh, python -c, etc.
- ⚠️ **Smart warning system** - Simplified prompts for dangerous operations
- 🛡️ **System directory protection** - Monitors access to /etc, /dev, /proc, etc.
- 📦 **Archive extraction safety** - Warns about potentially destructive archive operations
- 🔑 **Authentication caching** - Secure credential caching similar to sudo (15-minute default)
- 📝 **Comprehensive audit trail** - All actions logged with context regardless of warnings
- 🔐 **Sudoers integration** - Full compatibility with existing sudo rules
- 🎯 **Privilege-aware warnings** - Users with ALL commands skip warnings but maintain logging
- 🛡️ **CVE vulnerability protection** - Audited against major bash CVEs (2014-2024)
  - **CVE-2023-22809**: Sudoedit privilege escalation protection
  - **CVE-2022-3715**: Bash heap buffer overflow mitigation
  - **CVE-2019-9924**: Restricted shell bypass prevention
  - **Shellshock variants**: CVE-2014-6271, CVE-2014-7169, and related vulnerabilities
- 🔐 **Environment hardening** - Comprehensive sanitization against injection attacks
  - Enhanced editor variable sanitization (EDITOR, VISUAL, SUDO_EDITOR)
  - Library injection protection (LD_PRELOAD, PYTHONPATH, etc.)
  - Null byte injection detection and blocking
- 🧪 **Security testing** - Automated CVE-specific regression tests

### **User Experience**
- **Enhanced prompt** - Shows current directory and target user context
- **Color support** - Inherits colors from calling shell's PS1 environment
- **Tab completion** - Path completion for commands and arguments
- **Command history** - Persistent history with timestamps
- **Line editing** - Full readline-style editing capabilities
- **Session logging** - Optional complete session recording

### **Shell Enhancements**
- 🔗 **Alias management** - Create and manage command aliases with security validation
- 🌍 **Environment variables** - Secure modification of whitelisted environment variables
- 📁 **Directory stack** - pushd/popd/dirs commands for directory navigation
- 🔍 **Command information** - which/type commands for command introspection
- 💾 **Persistent storage** - Aliases automatically saved and restored between sessions
- 🛡️ **Security validation** - All enhancements include comprehensive security checks

### **Color Support**
Sudosh automatically inherits and applies colors from your shell's environment:
- **Automatic detection** - Detects terminal color capabilities via `TERM` and `COLORTERM`
- **PS1 parsing** - Extracts color codes from your shell's `PS1` environment variable
- **Environment preservation** - Maintains color settings during security sanitization
- **Graceful fallback** - Falls back to plain text when colors aren't supported
- **Multiple formats** - Supports `\033[`, `\e[`, and direct ANSI escape sequences

### **Authentication Caching**
Sudosh implements secure authentication caching similar to sudo:
- **Automatic caching** - Successful authentications are cached for 15 minutes (configurable)
- **Secure storage** - Cache files stored in `/var/run/sudosh` with strict permissions (0600, root-owned)
- **Session isolation** - Separate cache files per user and TTY for security
- **Automatic cleanup** - Expired cache files are automatically removed
- **Cache invalidation** - Failed authentications clear existing cache entries

## 📦 **Installation**

### **Build from Source**
```bash
git clone https://github.com/sandinak/sudosh.git
cd sudosh
make
sudo make install
```

### **Package Installation**

#### **RPM-based Systems (RHEL, CentOS, Fedora, openSUSE)**
```bash
# Build RPM package
make rpm

# Install the package
sudo rpm -ivh dist/sudosh-*.rpm
```

#### **DEB-based Systems (Ubuntu, Debian, Mint)**
```bash
# Build DEB package
make deb

# Install the package
sudo dpkg -i dist/sudosh_*.deb
sudo apt-get install -f  # Fix any dependency issues
```

#### **Build Both Package Types**
```bash
make packages
```

For detailed packaging instructions, see [docs/PACKAGING.md](docs/PACKAGING.md).

### **Requirements**
- **Linux/Unix system** with PAM support
- **GCC compiler** with C99 support
- **PAM development libraries** (`libpam0g-dev` on Debian/Ubuntu)
- **Sudo** installed and configured
- **Root privileges** for installation

## 🎯 **Usage**

### **Basic Usage**
```bash
# Start interactive sudo shell
sudo sudosh

# Run as specific target user
sudo sudosh -u www-data

# Enable session logging
sudo sudosh -l /var/log/sudosh-session.log

# Verbose mode
sudo sudosh -v

# List available commands (similar to sudo -l)
sudo sudosh -l
```

### **Command Line Options**
```text
Usage: sudosh [options]

Options:
  -h, --help              Show help message
      --version           Show version information
  -v, --verbose           Enable verbose output
  -l, --list              List available commands showing each permission source separately
  -L, --log-session FILE  Log entire session to FILE
  -u, --user USER         Run commands as target USER
```

### **Permission Listing (-l option)**
The `-l` option provides comprehensive sudo permission analysis with detailed source attribution:

```bash
# List all sudo permissions with source files
sudo sudosh -l
```

**Example Output:**
```text
Sudo privileges for user on hostname:
=====================================

Defaults Configuration:
    env_reset, env_keep+=BLOCKSIZE, env_keep+="COLORFGBG COLORTERM", ...

Direct Sudoers Rules:
    ALL = (ALL) NOPASSWD: ALL  [Source: /etc/sudoers.d/username]
    ALL = (ALL) NOPASSWD: /usr/bin/ls, /usr/bin/cat  [Source: /etc/sudoers.d/specific_commands]

Group-Based Privileges:
    Group 'admin': (ALL) ALL  [Source: group membership]

System-Wide Group Rules:
    ALL = (ALL) ALL  [Source: %admin group rule in /etc/sudoers]

Summary:
✓ User has direct sudoers rules
✓ User has privileges through group membership
User is authorized to run sudo commands on hostname
```

**Features:**
- **Source Attribution**: Shows exactly which file grants each permission
- **Permission Types**: Distinguishes between direct rules, group membership, and system-wide group rules
- **File-Level Detail**: Displays specific sudoers file paths for troubleshooting
- **Comprehensive Analysis**: Covers all possible sudo permission sources

### **Interactive Commands**
```bash
# Built-in commands
help                    # Show available commands
history                 # Display command history
commands                # List built-in commands
rules                   # Show sudo rules and their sources
exit                    # Exit sudosh

# History navigation
!42                     # Execute command #42 from history
!!                      # Execute last command
Up/Down arrows          # Navigate through history

# Line editing
Ctrl-A/E               # Beginning/end of line
Ctrl-K/U               # Kill to end/kill entire line
Ctrl-D                 # Exit gracefully

# Enhanced tab completion
Tab                    # Intelligent context-aware completion
                       # - Empty line: shows all commands
                       # - After command: shows files/directories
                       # - After 'cd': shows directories only
                       # - Directory paths: shows directory contents

# Shell enhancements (bash/zsh-like features)
alias ll='ls -la'      # Create command aliases
unalias ll             # Remove aliases
export EDITOR=vim      # Set environment variables (safe ones only)
unset EDITOR           # Remove environment variables
env                    # Show all environment variables
which ls               # Show command location
type ll                # Show command type (builtin, alias, file)
pushd /tmp             # Push directory onto stack and change to it
popd                   # Pop directory from stack and change to it
dirs                   # Show directory stack
```

## 🔐 **Security Model**

### **Permission Validation**
Sudosh validates permissions at multiple levels:

1. **User Authentication** - PAM-based authentication
2. **Sudo Privileges** - Validates user is in sudoers
3. **Target User Permissions** - Checks runas permissions for `-u` option
4. **Command Validation** - Security checks for all commands

### **Security Protections**
```bash
# Shell commands are blocked
sudosh:/home/user## bash
sudosh: shell commands are not permitted

# Dangerous commands require confirmation (simplified prompts)
sudosh:/home/user## rm -rf /important/data
⚠️  Command uses recursive/force flags
Continue? (y/N):

# Archive extraction warnings
sudosh:/home/user## tar -xf archive.tar /etc/
⚠️  Archive extraction may overwrite files
Continue? (y/N):

# System directory access is monitored
sudosh:/home/user## ls /etc
⚠️  Accesses system directories
Continue? (y/N):

# Users with ALL commands in sudoers skip warnings (but commands are still logged)
sudosh:/home/admin## rm -rf /tmp/test
# No warning for users with unrestricted access, but logged to syslog
```

## 📝 **Configuration**

### **Sudoers Configuration**
For basic sudosh access:
```bash
# /etc/sudoers or /etc/sudoers.d/sudosh
username ALL=(ALL) ALL
%admin ALL=(ALL) ALL
```

For target user functionality:
```bash
# Allow running as specific users
webadmin ALL=(www-data,nginx) ALL
dbadmin ALL=(postgres,mysql) ALL

# Allow running as any user
sysadmin ALL=(ALL) ALL
```

### **Logging Configuration**
Sudosh logs to syslog by default. Configure your syslog daemon to handle sudosh logs:

#### **Linux (rsyslog/syslog-ng)**
```bash
# /etc/rsyslog.d/sudosh.conf
:programname, isequal, "sudosh" /var/log/sudosh.log
& stop
```

#### **macOS (syslog)**
```bash
# View sudosh logs in real-time
sudo log stream --predicate 'process == "sudosh"'

# View recent sudosh logs
sudo log show --predicate 'process == "sudosh"' --last 1h

# View all sudosh logs from today
sudo log show --predicate 'process == "sudosh"' --start "$(date '+%Y-%m-%d')"
```

## 📊 **Logging and Auditing**

### **Command Logging**
All commands are logged with full context:
```text
Dec 15 10:30:15 hostname sudosh[1234]: user: ls -la (SUCCESS)
Dec 15 10:30:20 hostname sudosh[1234]: user: systemctl restart nginx (as www-data) (SUCCESS)
Dec 15 10:30:25 hostname sudosh[1234]: user: SECURITY VIOLATION: shell command blocked
```

### **Viewing System Logs**

#### **Linux Systems**
```bash
# View recent sudo activity
sudo journalctl -u sudo -f

# View sudosh logs specifically
sudo journalctl | grep sudosh

# View logs from specific date
sudo journalctl --since "2024-12-15" | grep sudo

# Traditional syslog locations
sudo tail -f /var/log/auth.log        # Debian/Ubuntu
sudo tail -f /var/log/secure          # RHEL/CentOS/Fedora
sudo tail -f /var/log/messages        # Some distributions

# Search for specific user activity
sudo grep "username" /var/log/auth.log
```

#### **macOS Systems**
```bash
# View sudo logs in real-time
sudo log stream --predicate 'process == "sudo" OR process == "sudosh"'

# View recent sudo activity (last hour)
sudo log show --predicate 'process == "sudo"' --last 1h

# View sudosh logs specifically
sudo log show --predicate 'process == "sudosh"' --last 1d

# View logs from specific date
sudo log show --predicate 'process == "sudo"' --start "2024-12-15"

# Search for specific user in logs
sudo log show --predicate 'process == "sudo" AND eventMessage CONTAINS "username"' --last 1d

# Export logs to file for analysis
sudo log collect --start "2024-12-15" --output sudo_logs.logarchive
```

#### **Advanced Log Analysis**
```bash
# Linux: Parse sudo logs for failed attempts
sudo grep "authentication failure" /var/log/auth.log

# Linux: Show sudo sessions by user
sudo grep "sudo:" /var/log/auth.log | awk '{print $5, $6, $7, $8, $9}'

# macOS: Filter for authentication failures
sudo log show --predicate 'process == "sudo" AND eventMessage CONTAINS "authentication failure"' --last 1d

# macOS: Show command execution details
sudo log show --predicate 'process == "sudo" AND eventMessage CONTAINS "COMMAND"' --last 1h
```

### **Session Logging**
Optional session logging captures complete terminal sessions:
```bash
sudo sudosh -l /var/log/sessions/user-$(date +%Y%m%d-%H%M%S).log
```

### **Command History**
Persistent command history with timestamps:
```bash
# ~/.sudosh_history
[2024-12-15 10:30:15] ls -la
[2024-12-15 10:30:20] systemctl restart nginx
[2024-12-15 10:30:25] history
```

### **Viewing Sudosh Logs**

#### **macOS Unified Logging**
On macOS, sudo logs are managed by the unified logging system. Use these commands to view sudosh logs:

**View Recent Sudosh Logs:**
```bash
# View last 1 hour of sudosh logs
log show --last 1h --predicate 'process == "sudosh"'

# View logs from specific time period
log show --start '2024-12-15 10:00:00' --end '2024-12-15 11:00:00' --predicate 'process == "sudosh"'

# Follow live sudosh logs
log stream --predicate 'process == "sudosh"'

# View all sudosh logs from today
log show --start 'today 00:00:00' --predicate 'process == "sudosh"'
```

**View System-wide Sudo Activity:**
```bash
# View all sudo activity (including sudosh)
log show --last 1h --predicate 'process == "sudo" OR process == "sudosh"'

# View authentication logs
log show --last 1h --predicate 'subsystem == "com.apple.authorization"'

# Search for specific user activity
log show --last 1h --predicate 'process == "sudosh" AND eventMessage CONTAINS "username"'
```

**Export Logs for Analysis:**
```bash
# Export sudosh logs to file
log show --last 24h --predicate 'process == "sudosh"' > sudosh_logs.txt

# Export in JSON format for parsing
log show --last 24h --predicate 'process == "sudosh"' --style json > sudosh_logs.json
```

#### **Linux Syslog Systems**
On Linux, sudo logs are typically managed by syslog. Location and commands vary by distribution:

**Ubuntu/Debian Systems:**
```bash
# View sudosh logs in auth.log
sudo tail -f /var/log/auth.log | grep sudosh

# View recent sudosh activity
sudo grep sudosh /var/log/auth.log | tail -20

# View logs from specific date
sudo grep "Dec 15" /var/log/auth.log | grep sudosh

# View all sudo activity (including sudosh)
sudo grep sudo /var/log/auth.log | tail -20
```

**RHEL/CentOS/Fedora Systems:**
```bash
# View sudosh logs in secure log
sudo tail -f /var/log/secure | grep sudosh

# View recent sudosh activity
sudo grep sudosh /var/log/secure | tail -20

# Using journalctl (systemd systems)
sudo journalctl -u sudo -f | grep sudosh
sudo journalctl --since "1 hour ago" | grep sudosh
```

**SUSE Systems:**
```bash
# View sudosh logs
sudo tail -f /var/log/messages | grep sudosh
sudo grep sudosh /var/log/messages | tail -20
```

**Using Journalctl (Modern Linux):**
```bash
# View sudosh logs with journalctl
sudo journalctl -t sudosh

# Follow live sudosh logs
sudo journalctl -t sudosh -f

# View logs from last hour
sudo journalctl -t sudosh --since "1 hour ago"

# View logs from specific time range
sudo journalctl -t sudosh --since "2024-12-15 10:00:00" --until "2024-12-15 11:00:00"

# View logs with context
sudo journalctl -t sudosh -n 50

# Export logs to file
sudo journalctl -t sudosh --since "today" > sudosh_logs.txt
```

#### **Custom Log Configuration**
If you've configured custom logging, check your specific log files:

**Custom Syslog Configuration:**
```bash
# If using custom sudosh.conf in rsyslog
sudo tail -f /var/log/sudosh.log

# If using custom facility
sudo tail -f /var/log/local0.log
```

**Session Log Files:**
```bash
# If using -l option for session logging
tail -f /var/log/sessions/user-20241215-103000.log

# View all session logs
ls -la /var/log/sessions/
```

#### **Log Analysis Examples**

**Find Security Violations:**
```bash
# macOS
log show --last 24h --predicate 'process == "sudosh" AND eventMessage CONTAINS "SECURITY VIOLATION"'

# Linux
sudo grep "SECURITY VIOLATION" /var/log/auth.log | grep sudosh
```

**Track User Activity:**
```bash
# macOS - specific user
log show --last 24h --predicate 'process == "sudosh" AND eventMessage CONTAINS "username:"'

# Linux - specific user
sudo grep "username:" /var/log/auth.log | grep sudosh
```

**Monitor Failed Commands:**
```bash
# macOS
log show --last 24h --predicate 'process == "sudosh" AND eventMessage CONTAINS "FAILURE"'

# Linux
sudo grep "FAILURE" /var/log/auth.log | grep sudosh
```

**Real-time Monitoring:**
```bash
# macOS - live monitoring
log stream --predicate 'process == "sudosh"' --color always

# Linux - live monitoring
sudo tail -f /var/log/auth.log | grep --color=always sudosh
```

## 🎯 **Use Cases**

### **System Administration**
```bash
# Full system access
sudo sudosh -u root
root@hostname:/## systemctl restart services
```

### **Web Server Management**
```bash
# Manage web server as www-data
sudo sudosh -u www-data
www-data@hostname:/var/www## chown www-data:www-data uploads/
```

### **Database Administration**
```bash
# Database operations as postgres user
sudo sudosh -u postgres
postgres@hostname:/var/lib/postgresql## pg_dump mydb > backup.sql
```

### **Application Deployment**
```bash
# Deploy as application user
sudo sudosh -u appuser
appuser@hostname:/opt/app## git pull origin main
```

## 🧪 **Testing**

Sudosh includes a comprehensive test suite with extensive security testing to ensure reliability and protection against vulnerabilities.

### **Run All Tests**
```bash
# Run complete test suite
make test

# Run all security tests
make security-tests
```

### **Security Test Categories**
```bash
# Command injection protection tests
./bin/test_security_command_injection

# Privilege escalation protection tests
./bin/test_security_privilege_escalation

# Authentication bypass protection tests
./bin/test_security_auth_bypass

# Logging evasion protection tests
./bin/test_security_logging_evasion

# Race condition protection tests
./bin/test_security_race_conditions

# Enhanced security features tests
./bin/test_security_enhanced_fixes

# Comprehensive security assessment
./bin/test_security_comprehensive
```

### **Unit and Integration Tests**
```bash
# Authentication unit tests
./bin/test_unit_auth

# Security unit tests
./bin/test_unit_security

# Utility function tests
./bin/test_unit_utils

# Integration tests
./bin/test_integration_basic

# Color functionality tests
./bin/test_color_functionality

# Shell enhancement tests
./bin/test_shell_enhancements

# Logging comprehensive tests
./bin/test_logging_comprehensive

# Authentication cache tests
./bin/test_auth_cache
```

### **Test Results Interpretation**
- **SECURE**: Attack was blocked - system is protected ✅
- **VULNERABLE**: Attack succeeded - security issue detected ❌
- **PASSED**: Functionality works correctly ✅
- **FAILED**: Functionality issue detected ❌

## 📋 **Changelog**

### **Version 1.9.1** (Latest)
#### Enhanced Tab Completion System and Directory Path Completion Fix

**🆕 MAJOR FEATURES:**
- **Enhanced Tab Completion System**: Comprehensive intelligent completion with context awareness
  - **Empty line completion**: `<Tab>` on empty line displays all available commands (built-ins + PATH executables)
  - **Empty argument completion**: Context-aware file/directory listings based on command type
  - **CD command optimization**: `cd <Tab>` shows directories only for efficient navigation
  - **Clean multi-column display**: Professional formatting for completion lists
  - **Maintained compatibility**: All existing tab completion behavior preserved

- **🚨 CRITICAL FIX - Directory Path Completion**: Permanently resolved tab completion issue
  - **Fixed**: `ls /etc/<Tab>` now displays all files in /etc/ instead of auto-completing to first entry
  - **Preserved**: `ls /etc/host<Tab>` still auto-completes to matching files (existing behavior)
  - **Enhanced**: Works with absolute paths (/etc/) and relative paths (src/)
  - **Robust**: Handles edge cases (spaces, tabs, complex nested paths)
  - **Tested**: Comprehensive regression tests prevent future issues

**🔧 TECHNICAL IMPROVEMENTS:**
- **Directory end detection**: Smart algorithm detects when cursor is at end of directory path
- **Context-aware logic**: Different completion behavior for commands vs arguments
- **Memory optimization**: Efficient completion with proper cleanup
- **Performance**: Minimal overhead for enhanced functionality

**🧪 TESTING & QUALITY:**
- **Comprehensive test coverage**: 18+ test suites with 100+ individual test cases
- **Regression prevention**: Specific tests for directory completion fix
- **Security validation**: All security tests passing
- **Backward compatibility**: Zero breaking changes

### **Version 1.8.0**
#### File Locking System and Authentication Cache Enhancements

**🆕 NEW FEATURES:**
- **File locking system**: Prevents concurrent access to critical files
- **Enhanced authentication cache**: Improved security and performance
- **Color support improvements**: Better terminal compatibility

### **Version 1.7.1**
#### Critical Security Bugfix - Sudo Permission Enforcement + Enhanced User Experience

**🚨 CRITICAL SECURITY FIX:**
- **Fixed privilege escalation vulnerability**: Sudosh was allowing any command to be run when users had general sudo privileges, instead of enforcing specific command permissions from sudoers configuration
- **Added `check_command_permission()` function**: Uses `sudo -l -U username command` to validate specific command permissions against sudoers rules
- **Enhanced main command loop**: Now validates each command against sudoers configuration before execution
- **Improved security logging**: Added comprehensive audit trail for denied commands and security violations
- **Maintains principle of least privilege**: Users now limited to commands explicitly allowed in their sudoers configuration

**🆕 NEW FEATURES:**
- **Added 'rules' built-in command**: Shows detailed sudo rules and their sources for the current user
  - Displays the same information as `sudo -l` but from within the interactive shell
  - Shows direct sudoers rules, group-based privileges, and system-wide group rules
  - Includes source file attribution for complete transparency
  - Accessible via the `rules` command in the sudosh prompt

**🔧 ENHANCED ERROR MESSAGES:**
- **Improved I/O redirection error messages**: Now provides detailed explanation of security risks
  - Explains how redirection operators can overwrite critical system files
  - Details how redirects can bypass file permissions and access controls
  - Warns about privilege escalation vectors
  - Suggests using individual commands without redirection

- **Enhanced pipe error messages**: More informative explanations for pipe blocking
  - Explains how pipes can chain commands and bypass security controls
  - Details risks of passing sensitive data to unauthorized commands
  - Warns about complex command injection attacks
  - Suggests running commands individually instead of chaining

**📚 DOCUMENTATION UPDATES:**
- Updated help system to include 'rules' command
- Enhanced man page with detailed 'rules' command documentation
- Updated README with new command examples
- Added comprehensive test suite for new features

**Security Impact:**
- Prevents privilege escalation through unrestricted command execution
- Ensures proper enforcement of sudoers constraints
- Provides detailed audit logging for compliance and security monitoring

**Testing:**
- Added comprehensive test suite (`test_sudo_permission_enforcement.c`)
- Added `test_rules_command.c` for new feature validation
- All existing functionality preserved with zero regressions
- Enhanced error messages and user feedback

**⚠️ SECURITY NOTICE**: This is a critical security update. All sudosh deployments should be updated immediately to prevent potential privilege escalation attacks.

### **Version 1.7.0**
#### Enhanced User Interface, Security Protections, and CVE Audit

**Shell Experience Improvements:**
- **Authentic shell behavior**: Removed verbose command feedback messages for Unix philosophy compliance
  - Eliminated "Changed to: /path/directory" message from `cd` command
  - Removed "Goodbye!" message from `exit` command
  - Silent operation for successful commands, errors only when needed
- **Single question mark help**: Added `?` as a shortcut for the help command
- **Enhanced error messages**: Improved pipe and redirection rejection messages with specific explanations
  - Pipes: "pipes are not supported - only single commands are supported"
  - Redirection: "file redirection is blocked for security reasons"

**Comprehensive CVE Security Audit:**
- **Systematic vulnerability review**: Audited against major bash CVEs (2014-2024)
- **CVE-2014-6271 (Shellshock)**: Protected via environment sanitization and input validation
- **CVE-2014-7169 (Shellshock variant)**: Mitigated through controlled execution model
- **CVE-2014-7186/7187 (Memory corruption)**: Prevented by input length limits and validation
- **CVE-2019-9924 (rbash bypass)**: Not applicable - custom security model used
- **CVE-2022-3715 (Heap overflow)**: Enhanced parameter validation implemented
- **Security audit documentation**: Complete analysis in SECURITY_AUDIT.md

**Extended Security Protections:**
- **Secure pager execution**: Pagers now run with security restrictions instead of being blocked entirely
  - `less`, `more`, `most`, `pg` allowed with shell escape prevention
  - Environment variables set to disable dangerous features (LESSSECURE=1, EDITOR=/bin/false)
  - Shell command execution disabled while preserving file viewing functionality
  - All pager usage logged for security auditing
- **Enhanced parameter validation**: Additional protections against heap buffer overflow attacks
- **Environment hardening**: Comprehensive bash-related environment variable sanitization
- **CVE-specific protections**: Targeted mitigations for known vulnerability patterns

**Testing and Quality Assurance:**
- **Security test suite**: Comprehensive CVE-specific test cases in tests/security_cve_tests.sh
- **Regression testing**: Automated tests for all major CVE attack vectors
- **Shell behavior validation**: Tests for authentic shell experience improvements

**Documentation Updates:**
- **Security audit report**: Complete CVE analysis and risk assessment in SECURITY_AUDIT.md
- **Enhanced man page**: Updated with security protections and shell improvements
- **Comprehensive README**: Detailed feature descriptions and security documentation

### **Version 1.6.0**
#### Simplified Security Warnings and Enhanced Archive Safety

**New Features:**
- **Simplified warning system**: Shorter, clearer prompts (y/N instead of yes/no)
- **Archive extraction safety**: New warnings for potentially destructive archive operations
  - Detects `tar -x`, `unzip`, `gunzip`, and other extraction commands
  - Warns when extracting to existing directories or system paths
  - Checks for dangerous flags like `--overwrite` and `--force`
- **Privilege-aware warnings**: Users with ALL commands in sudoers skip warnings while maintaining audit logs
- **Reduced warning fatigue**: Removed common administrative commands from dangerous list
  - Package management: `apt`, `yum`, `dnf`, `rpm`, `dpkg`
  - Service management: `systemctl`, `service`, `chkconfig`
  - Scheduling: `crontab`, `at`, `batch`

**Improvements:**
- More intuitive warning prompts with shorter messages
- Better user experience for experienced administrators
- Maintained comprehensive security logging for compliance
- Enhanced documentation with updated examples

### **Version 1.5.0**
#### Enhanced Permission Analysis and Source Attribution

**New Features:**
- **Enhanced `-l` option**: Now shows detailed permission source attribution
  - Displays specific sudoers file names for each rule
  - Separates direct rules, group membership, and system-wide group rules
  - Provides comprehensive permission analysis similar to `sudo -l`
- **Improved option consistency**: Changed log option to `-L` to match sudo conventions
- **Source file tracking**: All sudoers rules now include their source file path
- **Better error handling**: Enhanced null pointer checking and memory management

**Improvements:**
- Fixed segmentation faults in permission checking
- Enhanced sudoers parsing to handle all file sources correctly
- Improved documentation with detailed examples
- Better man page with comprehensive option descriptions

**Bug Fixes:**
- Fixed issue where sudoers.d files weren't being parsed correctly
- Resolved memory leaks in sudoers configuration handling
- Fixed null pointer dereferences in group membership checking

### **Version 1.4.0**
#### Target User Support and Enhanced Security

**New Features:**
- Target user functionality with `-u` option
- Enhanced authentication caching
- Comprehensive security testing framework
- Professional package generation (RPM/DEB)

**Security Enhancements:**
- Shell command blocking
- Dangerous command detection
- System directory protection
- Comprehensive audit logging

## 📚 **Documentation**

Comprehensive documentation is available in the `docs/` directory:

- **[Enhanced Security Features](docs/ENHANCED_SECURITY_FEATURES.md)** - Detailed security protections
- **[Target User Functionality](docs/TARGET_USER_FUNCTIONALITY.md)** - Multi-user capabilities
- **[Security Testing Summary](docs/SECURITY_TESTING_SUMMARY.md)** - Security validation framework
- **[Testing Guide](docs/TESTING_GUIDE.md)** - Comprehensive testing documentation
- **[Comprehensive Guide](docs/COMPREHENSIVE_GUIDE.md)** - Complete feature documentation
- **[Packaging Guide](docs/PACKAGING.md)** - Package creation and distribution

## 🤝 **Contributing**

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### **Development Guidelines**
- Follow existing code style and conventions
- Add tests for new functionality
- Update documentation for user-facing changes
- Ensure all security tests pass

## 📄 **License**

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🔒 **Security**

Sudosh is designed with security as the primary concern. If you discover a security vulnerability, please report it responsibly:

1. **Do not** open a public issue
2. Email security concerns to the maintainers
3. Provide detailed information about the vulnerability
4. Allow time for assessment and patching



## 📞 **Support**

- **Issues**: [GitHub Issues](https://github.com/sandinak/sudosh/issues)
- **Documentation**: [docs/](docs/)
- **Security**: See security reporting guidelines above

## 🏆 **Acknowledgments**

- **[Augment Code](https://www.augmentcode.com)** - Primary development assistance using AI-powered coding
- Inspired by the original sudosh project
- Built with security best practices from the sudo project
- Thanks to all contributors and security researchers

---

**Sudosh** - Secure, auditable, user-friendly privilege escalation for system administrators.
