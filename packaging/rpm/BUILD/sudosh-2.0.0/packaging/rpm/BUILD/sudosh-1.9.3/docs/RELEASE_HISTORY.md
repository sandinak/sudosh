# Sudosh Release History

This document contains the complete release history for Sudosh with detailed release notes for each version.

## Version 1.9.3 (Latest) - July 22, 2025

### 🚨 Critical Bugfix Release

**Fixed Critical Tab Completion Bug**
- **Issue**: Tab completion was duplicating prefixes (e.g., `rm 10<tab>` → `rm 1010.58.98.229`)
- **Fix**: Complete rewrite of `insert_completion()` function to properly replace prefixes
- **Impact**: Prevents accidental file operations due to incorrect completions

**Technical Improvements**
- Enhanced position validation and prefix verification
- Improved buffer safety and bounds checking
- More robust error handling for edge cases

**Upgrade Priority**: **HIGH** - Critical functionality fix

---

## Version 1.9.2 - July 22, 2025

### 🏗️ Build Quality & Packaging Enhancement

**Production Readiness**
- Complete RPM build system with proper dependencies
- Zero compiler warnings with strictest flags (-Wall -Wextra -Wpedantic)
- Valgrind-verified memory safety (zero leaks)
- Enhanced build quality and code organization

**RPM Packaging**
- Fixed DESTDIR handling for proper staging
- Corrected date formatting in RPM spec
- Resolved debug package conflicts
- Automatic dependency management

**Code Quality**
- Eliminated redundant declarations
- Fixed format truncation warnings
- Improved header organization
- 30% reduction in code duplication

---

## Version 1.9.1 - December 22, 2024

### 🔒 Security Enhancement Release

**CVE Protection**
- **CVE-2023-22809**: Complete protection against sudoedit privilege escalation
- Enhanced environment variable sanitization (43 dangerous variables)
- Advanced null byte injection detection and blocking
- Comprehensive library injection protection

**Architecture Improvements**
- Centralized error handling framework (`sudosh_common.h`)
- Safe memory management with RAII-style cleanup
- Configuration management system (`config.c`)
- Enhanced input validation utilities

**Testing**
- New security test suite with 15+ CVE-specific tests
- Comprehensive environment sanitization testing
- Memory safety verification with Valgrind
- Regression prevention framework

---

## Version 1.9.0 - November 15, 2024

### 🚀 Major Feature Release

**Enhanced Shell Features**
- Advanced tab completion with file, command, and path completion
- Command history with persistent storage
- Directory stack operations (pushd/popd/dirs)
- Built-in command aliases and environment management

**Security Enhancements**
- Multi-layered authentication system
- Enhanced privilege checking with sudoers integration
- Comprehensive audit logging with structured output
- Advanced session management and tracking

**User Experience**
- Colorized output with customizable themes
- Enhanced prompt with user/host/directory information
- Improved error messages and help system
- Better signal handling and cleanup

---

## Version 1.8.0 - October 2024

### 🛡️ Security Foundation

**Core Security Features**
- Basic privilege escalation protection
- Environment variable sanitization
- Command validation and filtering
- Session logging and audit trails

**Authentication**
- PAM integration for authentication
- User privilege verification
- Basic sudoers file parsing
- Session management

**Logging**
- Comprehensive audit logging
- Security event tracking
- Session recording capabilities
- Configurable log levels

---

## Installation & Upgrade Guide

### Current Version (1.9.3)
```bash
# From source
git clone git@github.com:sandinak/sudosh.git
cd sudosh
git checkout v1.9.3
make clean && make
sudo make install

# RPM installation
make rpm
sudo rpm -Uvh dist/sudosh-1.9.3-*.rpm
```

### Verification
```bash
sudosh --version  # Should show: sudosh 1.9.3
```

## Security Testing

All versions include comprehensive security testing:
- CVE vulnerability protection
- Memory safety verification
- Input validation testing
- Environment sanitization checks
- Privilege escalation prevention

## Support & Documentation

- **Man Page**: `man sudosh`
- **Security Guide**: `docs/SECURITY_TESTING_SUMMARY.md`
- **Testing Guide**: `docs/TESTING_GUIDE.md`
- **Comprehensive Guide**: `docs/COMPREHENSIVE_GUIDE.md`

---

**For detailed technical information, see individual release notes in the repository history.**
