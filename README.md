# Sudosh - Secure Interactive Sudo Shell

**Author**: Branson Matheson <branson@sandsite.org>
**Development**: This project was primarily developed using [Augment Code](https://www.augmentcode.com) AI assistance

[![Version](https://img.shields.io/badge/version-1.6.0-blue.svg)](https://github.com/sandinak/sudosh)
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

### **User Experience**
- **Enhanced prompt** - Shows current directory and target user context
- **Color support** - Inherits colors from calling shell's PS1 environment
- **Tab completion** - Path completion for commands and arguments
- **Command history** - Persistent history with timestamps
- **Line editing** - Full readline-style editing capabilities
- **Session logging** - Optional complete session recording

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
```
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
```
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
exit                    # Exit sudosh

# History navigation
!42                     # Execute command #42 from history
!!                      # Execute last command
Up/Down arrows          # Navigate through history

# Line editing
Ctrl-A/E               # Beginning/end of line
Ctrl-K/U               # Kill to end/kill entire line
Ctrl-D                 # Exit gracefully
Tab                    # Path completion
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
```
Dec 15 10:30:15 hostname sudosh[1234]: user: ls -la (SUCCESS)
Dec 15 10:30:20 hostname sudosh[1234]: user: systemctl restart nginx (as www-data) (SUCCESS)
Dec 15 10:30:25 hostname sudosh[1234]: user: SECURITY VIOLATION: shell command blocked
```

### **Viewing Sudo Logs**

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

### **Viewing Sudo Logs**

#### **macOS Systems**
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

#### **Linux Systems**
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

### **Version 1.6.0** (Latest)
**Simplified Security Warnings and Enhanced Archive Safety**

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
**Enhanced Permission Analysis and Source Attribution**

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
**Target User Support and Enhanced Security**

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
