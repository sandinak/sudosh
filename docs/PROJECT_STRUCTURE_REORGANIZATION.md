# Project Structure Reorganization

## Overview

The sudosh project has been reorganized to improve maintainability, testing, and documentation structure. This document outlines the changes made and the new organization.

## Directory Structure Changes

### Before Reorganization
```
sudosh/
├── src/                    # Source code
├── tests/                  # All tests mixed together
├── docs/                   # Documentation
├── test_*.sh              # Test scripts in root
├── demo_*.sh              # Demo scripts in root
├── ENHANCED_WHICH_*.md    # Documentation in root
└── ...
```

### After Reorganization
```
sudosh/
├── src/                    # Source code (unchanged)
├── tests/                  # Organized test structure
│   ├── unit/              # Unit tests (.c files)
│   ├── integration/       # Integration tests (.sh and .c files)
│   ├── security/          # Security-focused tests
│   ├── regression/        # Regression tests
│   ├── demos/             # Demo scripts
│   ├── test_framework.h   # Shared test framework
│   └── run_all_tests.sh   # Updated test runner
├── docs/                   # All documentation
│   ├── ENHANCED_WHICH_IMPLEMENTATION.md
│   └── ... (existing docs)
└── ... (build files, etc.)
```

## Changes Made

### 1. Test File Organization

**Unit Tests** (`tests/unit/`):
- `test_auth_cache.c`
- `test_color_functionality.c`
- `test_ctrl_c_feature.c`
- `test_directory_completion_fix.c`
- `test_editor_regression.c`
- `test_list_commands.c`
- `test_logging_comprehensive.c`
- `test_rules_command.c`
- `test_secure_editors_mandatory.c`
- `test_shell_enhancements.c`
- `test_unit_auth.c`
- `test_unit_security.c`
- `test_unit_utils.c`

**Integration Tests** (`tests/integration/`):
- `test_integration_basic.c`
- `ansible_sudosh_become_test.yml`
- `ansible_test_playbook.yml`
- `inventory.ini`
- `simple_auth_test.sh`
- `simple_which_test.sh`
- `test_tab_completion.sh`
- `test_which_command.sh`

**Security Tests** (`tests/security/`):
- `test_security_auth_bypass.c`
- `test_security_command_injection.c`
- `test_security_comprehensive.c`
- `test_security_enhanced_fixes.c`
- `test_security_logging_evasion.c`
- `test_security_privilege_escalation.c`
- `test_security_race_conditions.c`
- `test_security_framework.h`
- `security_cve_tests.sh`

**Regression Tests** (`tests/regression/`):
- `test_file_locking_system.sh`
- `test_new_features.sh`
- `test_secure_editor_fix.sh`

**Demo Scripts** (`tests/demos/`):
- `demo_ctrl_c.sh`
- `demo_enhanced_which.sh`
- `demo_file_locking.sh`
- `demo_path_features.sh`

### 2. Documentation Organization

**Moved to `docs/`**:
- `ENHANCED_WHICH_IMPLEMENTATION.md` - Enhanced 'which' command documentation

**Updated**:
- `docs/README.md` - Added reference to new documentation

### 3. Build System Updates

**Makefile Changes**:
- Updated test file patterns to handle subdirectories
- Modified compilation rules for organized test structure
- Updated test runner paths
- Fixed secure editor test path references

**Test Runner Updates** (`tests/run_all_tests.sh`):
- Organized test execution by category
- Added section headers for different test types
- Updated paths to reflect new structure
- Improved test reporting and organization

### 4. Code Quality Improvements

**Fixed Compiler Warnings**:
- `src/main.c:67` - Changed `char *effective_user` to `const char *effective_user`
- `src/editor_detection.c:100` - Added `(void)pid;` to suppress unused parameter warning

**Build Verification**:
- `make clean && make` now completes without warnings
- All object files compile cleanly

## Benefits of Reorganization

### 1. Improved Maintainability
- Clear separation of test types
- Easier to locate specific tests
- Better organization for new contributors

### 2. Enhanced Testing Workflow
- Targeted test execution by category
- Clearer test dependencies and relationships
- Improved CI/CD integration potential

### 3. Better Documentation Structure
- Centralized documentation in `docs/`
- Consistent naming and organization
- Easier cross-referencing

### 4. Cleaner Root Directory
- Removed clutter from root directory
- Professional project appearance
- Easier navigation

## Usage After Reorganization

### Running Tests
```bash
# Run all tests
make test

# Run specific test categories
make unit-test
make integration-test
make security-test

# Run comprehensive test suite
./tests/run_all_tests.sh
```

### Building
```bash
# Clean build (no warnings)
make clean && make

# Build with specific targets
make tests          # Build all test executables
make security-tests # Build security test suite
```

### Accessing Documentation
```bash
# All documentation now in docs/
ls docs/

# Enhanced which command documentation
cat docs/ENHANCED_WHICH_IMPLEMENTATION.md
```

### Running Demos
```bash
# Demo scripts now organized
./tests/demos/demo_enhanced_which.sh
./tests/demos/demo_file_locking.sh
```

## Migration Notes

### For Developers
- Update any scripts that reference old test paths
- Use new organized structure for new tests
- Follow established patterns for test categorization

### For CI/CD
- Update build scripts to use new test paths
- Consider running test categories separately
- Use `tests/run_all_tests.sh` for comprehensive testing

### For Documentation
- All new documentation should go in `docs/`
- Follow established naming conventions
- Update cross-references as needed

## Verification

The reorganization has been verified to:
- ✅ Build without warnings (`make clean && make`)
- ✅ Maintain all existing functionality
- ✅ Preserve test coverage
- ✅ Improve project organization
- ✅ Update all relevant paths and references

## Future Improvements

Potential future enhancements:
- Add test coverage reporting by category
- Implement parallel test execution
- Add automated test discovery
- Create test templates for new categories
- Integrate with CI/CD systems
