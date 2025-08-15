# Sudosh 2.0 Project Structure

## Overview

Sudosh 2.0 represents a major release with enhanced security features, improved system integration, and consolidated documentation. This document outlines the complete project structure and organization.

## Version 2.0 Changes

### Major Updates
- **Enhanced Shell Restriction**: Universal shell blocking with graceful fallback
- **System Integration**: Replaces `/usr/bin/sudo` for better compatibility
- **Consolidated Documentation**: All docs moved to `docs/` directory
- **Improved Testing**: Comprehensive test suite with 21+ validation tests
- **Release Management**: Professional release preparation and validation

This document describes the organization and structure of the Sudosh project repository.

## 📁 Repository Structure

```
sudosh/
├── 📄 README.md                    # Project overview and quick start
├── 📄 CHANGELOG.md                 # Version history and changes
├── 📄 CONTRIBUTING.md              # Development guidelines
├── 📄 LICENSE                      # MIT License
├── 📄 Makefile                     # Build system
├── 📄 PROJECT_STRUCTURE.md         # This file
├── 📄 RELEASE_NOTES_1.9.3.md      # Latest release notes
│
├── 📂 src/                         # Source code
│   ├── 📄 main.c                   # Main entry point
│   ├── 📄 sudosh.h                 # Main header file
│   ├── 📄 sudosh_common.h          # Common utilities and error handling
│   ├── 📄 auth.c                   # Authentication and privilege checking
│   ├── 📄 command.c                # Command processing and validation
│   ├── 📄 config.c                 # Configuration management
│   ├── 📄 filelock.c               # File locking utilities
│   ├── 📄 logging.c                # Audit logging and session tracking
│   ├── 📄 nss.c                    # Name Service Switch integration
│   ├── 📄 path_validator.c         # Path validation utility
│   ├── 📄 security.c               # Security enforcement and CVE protection
│   ├── 📄 shell_enhancements.c     # Enhanced shell features
│   ├── 📄 shell_env.c              # Environment management
│   ├── 📄 sssd.c                   # SSSD integration
│   ├── 📄 sudoers.c                # Sudoers file parsing
│   ├── 📄 sudosh.1.in              # Man page template
│   └── 📄 utils.c                  # Utilities and tab completion
│
├── 📂 tests/                       # Test suite
│   ├── 📄 test_framework.h         # Test framework header
│   ├── 📄 test_security_framework.h # Security test framework
│   ├── 📄 test_unit_auth.c         # Authentication unit tests
│   ├── 📄 test_unit_security.c     # Security unit tests
│   ├── 📄 test_unit_utils.c        # Utility unit tests
│   ├── 📄 test_security_cve_2023_fixes.c # CVE protection tests
│   ├── 📄 test_security_*.c        # Various security tests
│   ├── 📄 test_shell_*.c           # Shell enhancement tests
│   ├── 📄 test_*.c                 # Additional test files
│   └── 📄 security_cve_tests.sh    # CVE test script
│
├── 📂 docs/                        # Documentation
│   ├── 📄 README.md                # Documentation index
│   ├── 📄 COMPREHENSIVE_GUIDE.md   # Complete user guide
│   ├── 📄 SECURITY_TESTING_SUMMARY.md # Security testing documentation
│   ├── 📄 TESTING_GUIDE.md         # Testing procedures
│   ├── 📄 PACKAGING.md             # Package building guide
│   └── 📄 RELEASE_HISTORY.md       # Complete release history
│
├── 📂 packaging/                   # Package building
│   ├── 📄 sudosh.spec.in           # RPM spec template
│   ├── 📂 debian/                  # Debian packaging files
│   │   ├── 📄 control.in           # Package control template
│   │   ├── 📄 changelog.in         # Changelog template
│   │   ├── 📄 rules                # Build rules
│   │   ├── 📄 compat               # Compatibility level
│   │   ├── 📄 postinst             # Post-installation script
│   │   └── 📄 postrm               # Post-removal script
│   └── 📂 rpm/                     # RPM build directories
│       ├── 📂 BUILD/               # Build workspace
│       ├── 📂 BUILDROOT/           # Install root
│       ├── 📂 RPMS/                # Built RPM packages
│       ├── 📂 SOURCES/             # Source tarballs
│       ├── 📂 SPECS/               # Generated spec files
│       └── 📂 SRPMS/               # Source RPM packages
│
├── 📂 bin/                         # Built binaries (generated)
│   ├── 📄 sudosh                   # Main executable
│   └── 📄 path-validator           # Path validation utility
│
├── 📂 obj/                         # Object files (generated)
│   └── 📄 *.o                      # Compiled object files
│
└── 📂 dist/                        # Distribution packages (generated)
    ├── 📄 *.rpm                    # RPM packages
    └── 📄 *.deb                    # DEB packages (future)
```

## 🔧 Build Artifacts

The following directories are generated during build and can be cleaned with `make clean`:

- **`bin/`** - Compiled executables
- **`obj/`** - Object files
- **`sudosh.1`** - Generated man page
- **`dist/`** - Distribution packages
- **`packaging/rpm/BUILD/`** - RPM build artifacts

## 📖 Documentation Organization

### Primary Documentation
- **`README.md`** - Entry point with overview and quick start
- **`docs/COMPREHENSIVE_GUIDE.md`** - Complete feature documentation
- **`docs/SECURITY_TESTING_SUMMARY.md`** - Security features and testing

### Development Documentation
- **`CONTRIBUTING.md`** - Development guidelines and contribution process
- **`docs/TESTING_GUIDE.md`** - Testing procedures and validation
- **`docs/PACKAGING.md`** - Package building and distribution

### Release Documentation
- **`CHANGELOG.md`** - Version history with technical details
- **`docs/RELEASE_HISTORY.md`** - Comprehensive release notes
- **`RELEASE_NOTES_1.9.3.md`** - Latest release details

## 🧪 Test Organization

### Unit Tests
- **`test_unit_*.c`** - Component-specific unit tests
- **Authentication**: `test_unit_auth.c`
- **Security**: `test_unit_security.c`
- **Utilities**: `test_unit_utils.c`

### Security Tests
- **`test_security_*.c`** - Security-focused test suites
- **CVE Tests**: `test_security_cve_2023_fixes.c`
- **Framework**: `test_security_framework.h`

### Integration Tests
- **`test_*.c`** - Integration and comprehensive tests
- **Shell Features**: `test_shell_*.c`
- **Color Support**: `test_color_*.c`

## 🏗️ Build System

### Primary Targets
- **`make`** - Build main executable
- **`make tests`** - Build all test binaries
- **`make install`** - Install to system
- **`make rpm`** - Build RPM package
- **`make clean`** - Remove build artifacts

### Package Building
- **RPM**: `make rpm` creates packages in `dist/`
- **DEB**: Future support via `packaging/debian/`

## 🔒 Security Focus

The project structure emphasizes security:
- Dedicated security source files (`security.c`, `auth.c`)
- Comprehensive security test suite
- CVE-specific protection and testing
- Security-focused documentation

## 📋 Maintenance

### Regular Cleanup
```bash
make clean              # Remove build artifacts
git clean -fd           # Remove untracked files (careful!)
```

### Version Updates
When releasing new versions, update:
1. `src/sudosh.h` - Version definition
2. `README.md` - Version badge and recent updates
3. `src/sudosh.1.in` - Man page version
4. `CHANGELOG.md` - Version history
5. Create new release notes file

---

**This structure supports clean development, comprehensive testing, and reliable packaging for production deployment.**
