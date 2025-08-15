# Changelog

All notable changes to sudosh will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2024-12-24

### 🚀 Major Features Added
- **Enhanced Shell Access Restriction**: Universal shell blocking for all users without explicit permissions
- **Unified Behavior**: Both command-line and interactive modes show warning and continue in secure environment
- **Extended Shell Coverage**: Added `su` command to blocked shells list
- **Secure File Editing (sudoedit)**: Protected file editing with `-e` option that prevents shell escapes
- **Editor Preference Support**: Respects `SUDO_EDITOR`, `VISUAL`, `EDITOR` environment variables or defaults to `vi`
- **System Integration**: Sudo replacement now uses `/usr/bin/sudo` instead of `/usr/local/bin/sudo`
- **Manpage Integration**: Installs sudosh manpage as `sudo.8`, backs up original to `osudo.8`
- **Consolidated Documentation**: All documentation moved to `docs/` directory

### 🔧 Technical Improvements
- **Backup Strategy**: Original sudo backed up to `osudo` (instead of `sudo.orig`)
- **Installation Paths**: Updated to use system-standard paths for better compatibility
- **Shell Restriction Logic**: Simplified and unified shell access control across all modes
- **Warning Messages**: Simplified to "Shell access restricted, putting you in sudosh interactive mode"
- **Permission System**: Enhanced shell permission checking with group-based access (`sudo-shells` group)

### 🧪 Testing & Quality
- **Comprehensive Test Suite**: 25+ validation tests with 100% pass rate including sudoedit functionality
- **Mode Behavior Testing**: Validation of command-line vs interactive mode behavior
- **Sudoedit Testing**: 11 comprehensive tests for secure file editing functionality
- **Integration Testing**: Enhanced CI/CD pipeline with regression testing
- **Security Testing**: CVE-specific security validation

### 📚 Documentation
- **Release Notes**: Comprehensive 2.0.0 release documentation
- **Migration Guide**: Detailed upgrade instructions from 1.9.x
- **Consolidated Structure**: All docs moved to `docs/` directory
- **Updated README**: Enhanced with 2.0 features and installation instructions

### ⚠️ Breaking Changes
- **Installation Path**: Sudo replacement now installs to `/usr/bin/sudo` (system path)
- **Backup Names**: Original sudo backed up to `osudo` instead of `sudo.orig`
- **Shell Behavior**: Users without shell permissions see different behavior (graceful fallback vs hard rejection)
- **Manpage Location**: When installed as sudo replacement, appears as `sudo.8`

### 🔄 Migration Notes
- Uninstall previous version before upgrading
- Review shell access permissions for users
- Update any scripts referencing old installation paths
- Test shell restriction behavior with your user base

### 🐛 Bug Fixes
- **Environment Variable Handling**: Fixed test mode environment variable scope
- **Shell Detection**: Improved shell command detection and validation
- **Permission Checking**: Enhanced user permission validation logic
- **Build System**: Resolved compilation warnings and build issues

## [1.9.3] - 2025-07-23

### Fixed
- **Critical Tab Completion Bug**: Fixed prefix duplication issue where `rm 10<tab>` would incorrectly expand to `rm 1010.58.98.229` instead of `rm 10.58.98.229`
- **Robust Completion Logic**: Completely rewrote `insert_completion()` function to properly replace prefixes instead of just inserting
- **Position Validation**: Added comprehensive validation to ensure prefix position calculations are correct
- **Buffer Safety**: Enhanced bounds checking and prefix verification to prevent edge case failures
- **Terminal State Management**: Fixed terminal settings not being properly restored when command execution is interrupted or reset
- **Signal Handling**: Enhanced signal handlers to properly restore terminal state on SIGTERM/SIGQUIT

### Technical
- **Prefix Replacement**: Changed from insertion-based to replacement-based completion logic
- **Error Handling**: Added graceful handling of invalid positions and mismatched prefixes
- **Memory Safety**: Improved buffer manipulation to prevent overruns and corruption
- **Terminal Restoration**: Added global terminal state management with proper cleanup on exit
- **Signal Safety**: Improved signal handling to ensure terminal state is always restored

## [1.9.2] - 2025-07-22

### Added
- **Complete RPM Build System**: Fully functional RPM packaging with proper dependencies and installation scripts
- **Enhanced Build Quality**: Zero warnings with strictest compiler flags (-Wall -Wextra -Wpedantic -Wformat=2 -Wconversion)
- **Memory Safety Verification**: Valgrind-verified clean memory management with no leaks
- **Comprehensive Testing**: All security tests passing including CVE-2023+ protections

### Fixed
- **RPM Build Issues**: Fixed DESTDIR handling, date formatting, and debug package conflicts
- **Compiler Warnings**: Eliminated all redundant declarations and format truncation warnings
- **Build System**: Enhanced Makefile with proper packaging support and dependency management
- **Code Quality**: Removed duplicate function declarations and improved header organization

### Security
- **CVE Protection Verified**: All 15 CVE-2023+ security tests passing
- **Environment Sanitization**: 43 dangerous variables properly sanitized
- **Memory Safety**: Zero memory leaks confirmed by Valgrind analysis
- **Input Validation**: Enhanced null byte injection and command injection protection

### Technical
- **Production Ready**: Clean builds, comprehensive testing, and verified RPM packaging
- **Quality Assurance**: Strict compiler compliance and static analysis clean
- **Documentation**: Complete man pages, README updates, and release notes

## [1.9.1] - 2024-12-22

### Added
- **CVE-2023-22809 Protection**: Added comprehensive protection against sudoedit privilege escalation attacks
  - Sudoedit commands are now blocked with informative error messages
  - Enhanced environment variable sanitization for EDITOR, VISUAL, and SUDO_EDITOR
  - New security test suite for CVE-2023-22809 validation
- **Enhanced Environment Sanitization**: Expanded dangerous environment variable removal
  - Added protection against library injection (LD_PRELOAD, PYTHONPATH, PERL5LIB, RUBYLIB, etc.)
  - Enhanced editor-related variable sanitization (FCEDIT, LESSSECURE, LESSOPEN, etc.)
  - Added protection against man page and groff command injection
  - Improved Java and scripting language security (JAVA_TOOL_OPTIONS, CLASSPATH, etc.)
- **Null Byte Injection Protection**: Advanced detection and blocking of embedded null byte attacks
  - New `validate_command_with_length()` function for proper null byte detection
  - Enhanced security testing for null byte injection scenarios
- **Refactored Error Handling Framework**: New centralized error handling system
  - Added `sudosh_common.h` with standardized error codes and macros
  - Implemented safe memory management utilities with RAII-style cleanup
  - Added comprehensive input validation utilities
  - Centralized logging functions with consistent formatting
- **Configuration Management System**: New centralized configuration framework
  - Added `config.c` with support for file-based configuration
  - Configurable timeouts, command length limits, and security settings
  - Runtime configuration validation and error reporting
- **Enhanced Security Testing**: Comprehensive test suite for recent CVE vulnerabilities
  - New `test_security_cve_2023_fixes.c` with 15+ security tests
  - Automated testing for environment sanitization
  - Library injection protection validation
  - Command validation and null byte injection tests

### Security
- **CVE-2023-22809**: Fixed sudoedit privilege escalation vulnerability
- **Enhanced Command Validation**: Improved detection of command injection attempts
- **Environment Hardening**: Comprehensive sanitization against injection attacks
- **Memory Safety**: Enhanced memory management with bounds checking
- **Input Validation**: Strengthened validation against malformed input

### Changed
- **Version**: Updated to 1.9.1 across all documentation and build files
- **Man Page**: Enhanced documentation with comprehensive CVE protection details
- **README**: Updated with latest security features and CVE protection information
- **Build System**: Added new source files to Makefile for refactored components
- **Error Handling**: Migrated to centralized error handling framework
- **Function Declarations**: Fixed function declaration conflicts in header files

### Improved
- **Code Organization**: Reduced code duplication through centralized utilities
- **Memory Management**: Safer allocation and cleanup patterns
- **Security Logging**: Enhanced logging with function-specific context
- **Documentation**: Updated inline documentation for all new features
- **Test Coverage**: Expanded security test coverage for recent vulnerabilities

## [1.9.0] - 2024-12-XX

### Added
- **Enhanced Tab Completion System**: Comprehensive intelligent completion with context awareness
  - **Empty line completion**: `<Tab>` on empty line displays all available commands (built-ins + PATH executables)
  - **Context-aware completion**: Different behavior for commands vs arguments
  - **CD command optimization**: `cd <Tab>` shows directories only for efficient navigation
  - **Clean multi-column display**: Professional formatting for completion lists
  - **Maintained compatibility**: All existing tab completion behavior preserved

### Fixed
- **🚨 CRITICAL FIX - Directory Path Completion**: Permanently resolved tab completion issue
  - **Fixed**: `ls /etc/<Tab>` now displays all files in /etc/ instead of auto-completing to first entry
  - **Preserved**: `ls /etc/host<Tab>` still auto-completes to matching files (existing behavior)
  - **Enhanced**: Works with absolute paths (/etc/) and relative paths (src/)
  - **Robust**: Handles edge cases (spaces, tabs, complex nested paths)
  - **Tested**: Comprehensive regression tests prevent future issues

### Technical Improvements
- **Directory end detection**: Smart algorithm detects when cursor is at end of directory path
- **Context-aware logic**: Different completion behavior for commands vs arguments
- **Memory optimization**: Efficient completion with proper cleanup
- **Performance**: Minimal overhead for enhanced functionality

### Testing & Quality
- **Comprehensive test coverage**: 18+ test suites with 100+ individual test cases
- **Regression prevention**: Specific tests for directory completion fix
- **Security validation**: All security tests passing
- **Backward compatibility**: Zero breaking changes

## [1.8.0] - 2024-11-XX

### Added
- **File Locking System**: Prevents concurrent editing of the same file by multiple users with comprehensive conflict detection and automatic cleanup
- **Authentication Cache Enhancements**: Improved security and performance for credential caching

## [1.5.0] - 2024-10-XX

### Added
- **Ctrl-C Line Clearing**: Press Ctrl-C to cancel current line editing and start fresh, matching standard shell behavior
- **PATH Command**: New `path` built-in command displays PATH environment variable with minimal, clean output
- **PATH Validator Tool**: Standalone `path-validator` utility to validate and clean PATH for security issues
- **Enhanced CD Tab Completion**: Tab completion for `cd` command now shows only directories, improving usability

### Improved
- **PATH Command Output**: Simplified to show only raw PATH value and inaccessible directories for clean, scriptable output

### Fixed
- **Secure Editor Regression**: Fixed issue where secure editors (vi, vim, nano, pico) were being blocked instead of running in constrained environment - 2024-12-15

### Major Security Release - Comprehensive Security Audit & Fixes

This release represents a major security enhancement with comprehensive vulnerability fixes and extensive security testing infrastructure.

### Security Fixes
- **Command Injection Protection**: Fixed 8 critical command injection vulnerabilities
  - Shell metacharacter injection blocking (`;`, `|`, `&&`, `||`, backticks, `$()`)
  - Null byte injection detection and prevention
  - Path traversal attack prevention (`../`, `..\`)
  - Environment variable injection blocking
  - I/O redirection attack prevention (`>`, `>>`, `<`, `2>`)
  - Unicode and encoding attack protection
  - Format string injection prevention
- **Privilege Escalation Protection**: Fixed 2 critical privilege escalation vulnerabilities
  - PATH hijacking protection with hardcoded secure PATH
  - File descriptor manipulation prevention
- **Authentication Security**: Enhanced authentication validation
  - Comprehensive username validation with character filtering
  - Suspicious username pattern detection
  - Race condition protection in authentication cache with file locking
- **Environment Security**: Comprehensive environment sanitization
  - Removal of 28+ dangerous environment variables
  - Secure PATH enforcement
  - Enhanced environment isolation

### Security Infrastructure
- **Comprehensive Test Suite**: 6 security test categories with 95%+ vulnerability coverage
- **Regression Prevention**: Unit and integration tests to prevent future vulnerabilities
- **Security Documentation**: Detailed security testing framework documentation
- **File Locking**: Race condition protection with atomic file operations
- **Enhanced Validation**: 20+ security checks for input validation

### Documentation
- **Security Testing Summary**: Comprehensive security test documentation
- **Testing Guide**: Complete testing framework documentation
- **Enhanced README**: Updated with comprehensive testing information
- **Security Features**: Detailed documentation of all security protections

### Technical Improvements
- **Enhanced Command Validation**: Multi-layer security validation
- **Atomic Cache Operations**: Race condition prevention in authentication cache
- **Child Process Security**: File descriptor cleanup and isolation
- **Environment Hardening**: Comprehensive dangerous variable removal

## [1.4.0] - 2024-12-15

### Major Release - Authentication Caching & Packaging Support

This release introduces significant new features that enhance both user experience and distribution capabilities.

### Added
- **Authentication Caching**: Secure credential caching similar to sudo (15-minute default)
- **Color Support**: Automatic color inheritance from calling shell's PS1 environment
- **Package Generation**: Comprehensive packaging for RPM and DEB based systems
- **Cache Management**: Automatic cache file creation, validation, and cleanup
- **Session Isolation**: Separate cache files per user and TTY for enhanced security

### Enhanced
- **User Experience**: Eliminates repeated password prompts and provides colored prompts
- **Distribution Support**: Easy installation on major Linux distributions
- **Security Integration**: All new features work seamlessly with existing security measures
- **Documentation**: Comprehensive guides for packaging and new features

### Technical
- **Cache Directory**: Secure cache storage in `/var/run/sudosh`
- **Template System**: Flexible packaging templates for RPM and DEB formats
- **Color Detection**: Intelligent terminal and color capability detection
- **Test Coverage**: Comprehensive test suites for all new functionality

## [1.3.0] - 2024-12-15

## [1.3.0] - 2024-12-15

### Added
- **SSH Command Blocking**: Prevents outbound SSH connections when running as root
- **Safe Command Access**: Allows basic commands (ls, pwd, whoami, etc.) without sudoers privileges
- **Interactive Editor Blocking**: Prevents shell escapes through vi/vim/emacs/nano/etc.
- **Comprehensive Security Pipeline**: Multi-layered command validation and blocking

### Enhanced
- **Security Architecture**: Three-tier security validation (shells, SSH, editors, dangerous commands)
- **User Experience**: Clear error messages with helpful guidance for blocked commands
- **Privilege Management**: Granular access control with safe command exceptions
- **Attack Surface Reduction**: Prevents major privilege escalation vectors

### Security
- **SSH Blocking**: Prevents privilege escalation through SSH key forwarding and outbound connections
- **Editor Blocking**: Prevents shell escapes through interactive editors (vi :!command, emacs M-x shell, etc.)
- **Safe Commands**: Allows information gathering without full privileges (reduces need for broad sudoers access)
- **Comprehensive Logging**: All security violations logged with detailed context

### Technical
- **Enhanced Test Suite**: 31 comprehensive unit tests covering all security features
- **Modular Security Functions**: Clean separation of security validation logic
- **Performance Optimized**: Efficient command parsing and validation
- **Cross-Platform**: Works on Linux and macOS with proper editor detection

## [1.2.0] - 2024-12-15

### Added
- **Inactivity Timeout**: 300-second (5-minute) automatic session timeout for enhanced security
- **Enhanced Signal Handling**: Ctrl-C now passes to running commands instead of exiting sudosh
- **Ctrl-Z Ignore**: Ctrl-Z is completely ignored to prevent shell suspension
- **Comprehensive Documentation**: Consolidated guides and manpage enhancements
- **Gut Check Documentation**: Complete manpage documentation of dangerous command warnings
- **Automated Testing**: Enhanced test suite with no interactive input requirements

### Changed
- **Startup Interface**: Removed version printing for cleaner, professional appearance
- **Repository Organization**: Moved manpage source to src/ directory for better organization
- **Documentation Structure**: Consolidated multiple documents into comprehensive guides
- **Signal Behavior**: Improved signal handling for better user experience and security
- **Test Suite**: Fixed interactive test issues for reliable automated testing

### Enhanced
- **Security Features**: Session timeout prevents abandoned privileged sessions
- **User Experience**: More intuitive signal handling similar to standard shells
- **Command Interaction**: Better support for interactive and long-running commands
- **Documentation Quality**: Professional, comprehensive documentation throughout

### Fixed
- **Test Reliability**: Eliminated interactive input requirements in test suite
- **Signal Passing**: Proper signal delivery to child processes
- **Session Management**: Prevents accidental loss of privileged sessions

### Security
- **Session Timeout**: Automatic termination after 5 minutes of inactivity
- **Signal Protection**: Prevents accidental exit or suspension of privileged shell
- **Command Isolation**: Proper signal isolation between shell and executed commands

## [1.1.1] - 2024-12-14

### Added
- Interactive shell enhancements with command history and line editing
- Target user functionality with -u flag support
- Comprehensive security testing framework
- Enhanced logging and auditing capabilities
- Cross-platform documentation for Linux and macOS

### Enhanced
- Command validation and dangerous command detection
- Shell blocking and security protections
- Professional author attribution throughout codebase
- Comprehensive test coverage

### Fixed
- Ctrl-D exit handling for graceful termination
- Immediate history availability via arrow keys
- Repository organization and cleanup

## [1.0.0] - 2024-12-01

### Added
- Initial release of sudosh
- Basic interactive sudo shell functionality
- Command logging to syslog
- PAM authentication integration
- Basic security features and command validation

---

**Author**: Branson Matheson <branson@sandsite.org>
