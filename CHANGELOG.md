# Changelog

All notable changes to sudosh will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.2] - 2024-12-15

### Added
- **Authentication Caching**: Secure credential caching similar to sudo's behavior
- **Cache Management**: Automatic cache file creation, validation, and cleanup
- **Session Isolation**: Separate cache files per user and TTY for enhanced security
- **Cache Security**: Strict file permissions (0600) and root ownership for cache files

### Enhanced
- **User Experience**: Eliminates repeated password prompts within 15-minute window
- **Security Integration**: Cache validation with timestamp and session verification
- **Automatic Cleanup**: Expired cache files are automatically removed
- **Cache Invalidation**: Failed authentications clear existing cache entries

### Technical
- **Cache Directory**: Secure cache storage in `/var/run/sudosh`
- **Cache Structure**: Comprehensive cache metadata including user, timestamp, session ID
- **Cache Functions**: Complete API for cache creation, validation, and cleanup
- **Test Coverage**: Comprehensive test suite for authentication caching functionality

## [1.3.1] - 2024-12-15

### Added
- **Color Support**: Automatic color inheritance from calling shell's PS1 environment
- **Terminal Detection**: Intelligent color capability detection via TERM and COLORTERM variables
- **PS1 Parsing**: Extracts and applies color codes from shell prompt configuration
- **Environment Preservation**: Maintains color-related environment variables during security sanitization

### Enhanced
- **User Experience**: Colored prompts that match the user's shell configuration
- **Compatibility**: Supports multiple PS1 color formats (\033[, \e[, direct ANSI)
- **Graceful Fallback**: Automatically disables colors when not supported
- **Security Integration**: Color preservation works seamlessly with existing security measures

### Technical
- **Color Configuration Structure**: New color_config structure for managing color settings
- **Color Detection Functions**: Comprehensive terminal and color capability detection
- **PS1 Color Parsing**: Robust parsing of various PS1 color escape sequence formats
- **Test Coverage**: New test suite specifically for color functionality validation

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
