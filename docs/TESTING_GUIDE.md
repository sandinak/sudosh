# Sudosh Testing Guide

This comprehensive guide covers all aspects of testing sudosh, from basic functionality tests to advanced security validation.

## Quick Start

### Run All Tests
```bash
# Build and run complete test suite
make test

# Run comprehensive test suite (recommended)
./tests/run_all_tests.sh

# Run with test mode for unattended execution
export SUDOSH_TEST_MODE=1
./tests/run_all_tests.sh

# Run only security tests
make security-tests

# Clean build and test
make clean && make test
```

## Test Categories

### 1. Command-Line Execution Tests ⭐ NEW
**Purpose**: Validate the new sudo-like command execution functionality.

```bash
# Comprehensive command-line execution tests
./tests/test_command_line_execution.sh

# Test basic command execution
SUDOSH_TEST_MODE=1 ./bin/sudosh echo "Hello World"

# Test user specification
SUDOSH_TEST_MODE=1 ./bin/sudosh -u $(whoami) whoami

# Test command mode
SUDOSH_TEST_MODE=1 ./bin/sudosh -c "echo 'test'"

# Test AI blocking in command mode
AUGMENT_SESSION_ID=test ./bin/sudosh echo "blocked"
```

**Coverage**: 18 individual tests including:
- Basic command execution
- User specification with `-u` option
- Command mode with `-c` option
- AI detection in command mode
- Error condition handling
- File operations

### 2. Security Tests
**Purpose**: Validate protection against security vulnerabilities and attack vectors.

```bash
# Comprehensive security assessment
./bin/test_security_comprehensive

# Individual security test categories
./bin/test_security_command_injection      # Command injection protection
./bin/test_security_privilege_escalation   # Privilege escalation protection
./bin/test_security_auth_bypass           # Authentication bypass protection
./bin/test_security_logging_evasion       # Logging evasion protection
./bin/test_security_race_conditions       # Race condition protection
./bin/test_security_enhanced_fixes        # Enhanced security features
```

### 2. Unit Tests
**Purpose**: Test individual components and functions in isolation.

```bash
./bin/test_unit_auth        # Authentication functions
./bin/test_unit_security    # Security functions
./bin/test_unit_utils       # Utility functions
```

### 3. Integration Tests
**Purpose**: Test component interactions and end-to-end functionality.

```bash
./bin/test_integration_basic    # Basic integration scenarios
```

### 4. Feature Tests
**Purpose**: Test specific features and functionality.

```bash
./bin/test_color_functionality      # Color support and PS1 parsing
./bin/test_shell_enhancements      # Shell enhancement features
./bin/test_logging_comprehensive   # Comprehensive logging features
./bin/test_auth_cache             # Authentication caching
```

## Test Environment Setup

### Prerequisites
```bash
# Ensure build dependencies are available
sudo apt-get install build-essential libpam0g-dev  # Ubuntu/Debian
sudo yum install gcc pam-devel                     # RHEL/CentOS
```

### Build Tests
```bash
# Build all tests
make test

# Build only security tests
make security-tests

# Build specific test category
make bin/test_security_command_injection
```

## Test Mode Configuration ⭐ NEW

### Unattended Testing
Sudosh includes a comprehensive test mode for CI/CD and automated testing:

```bash
# Enable test mode (bypasses authentication and privileges)
export SUDOSH_TEST_MODE=1

# Run tests without user interaction
./tests/run_all_tests.sh

# Test individual components
SUDOSH_TEST_MODE=1 bin/test_unit_security
SUDOSH_TEST_MODE=1 bin/test_ai_detection
SUDOSH_TEST_MODE=1 bin/test_ansible_detection
```

### Test Mode Features
When `SUDOSH_TEST_MODE=1` is set:

1. **Authentication bypass** - No password prompts
2. **Privilege bypass** - No setuid requirements
3. **File system isolation** - Uses temporary directories
4. **Non-interactive execution** - Perfect for CI/CD
5. **Enhanced logging** - Detailed test output

### CI/CD Integration
```bash
# GitHub Actions example
- name: Test sudosh
  run: |
    export SUDOSH_TEST_MODE=1
    make test
    ./tests/run_all_tests.sh

# Jenkins example
environment {
    SUDOSH_TEST_MODE = '1'
}
stages {
    stage('Test') {
        steps {
            sh './tests/run_all_tests.sh'
        }
    }
}
```

## Understanding Test Results

### Security Test Results
- **SECURE (attack blocked)**: ✅ Security protection is working
- **VULNERABLE (attack succeeded)**: ❌ Security issue detected

### Functionality Test Results
- **PASSED**: ✅ Feature is working correctly
- **FAILED**: ❌ Feature issue detected

### Example Security Test Output
```
=== Security Tests - Command Injection ===
Testing: Shell metacharacter injection... SECURE (attack blocked)
Testing: Command substitution injection... SECURE (attack blocked)
Testing: I/O redirection injection... SECURE (attack blocked)
Testing: Path traversal injection... SECURE (attack blocked)
Testing: Environment variable injection... SECURE (attack blocked)
Testing: Null byte injection... SECURE (attack blocked)
Testing: Unicode encoding injection... SECURE (attack blocked)
Testing: Format string injection... SECURE (attack blocked)

=== Command Injection Test Results ===
Total tests: 8
Secure (attacks blocked): 8
Vulnerable (attacks succeeded): 0
✅ All command injection attacks blocked!
```

## Test Development

### Adding New Tests


### 3. Sudoers Authorization Profiles Tests (Integration)
Purpose: Validate sudosh authorization behavior across common sudoers profiles, verify NOPASSWD behavior, and ensure `-l`/`-ll` listings show correct sources and summaries.

Run only these tests:

    make test-sudoers-authz

What it does:
- Creates a temp sudoers file and sudoers.d directory
- Overrides sudosh parsing paths using environment variables:
  - `SUDOSH_SUDOERS_PATH` (sudoers file)
  - `SUDOSH_SUDOERS_DIR` (sudoers.d dir)
- Exercises 5 profiles:
  1. Unrestricted Admin (ALL=(ALL) NOPASSWD: ALL)
  2. Standard Admin (ALL=(ALL) ALL)
  3. Docker User (NOPASSWD: /usr/bin/docker, /usr/bin/docker-compose)
  4. System Service User (NOPASSWD: /usr/bin/systemctl restart/start/stop *)
  5. Dangerous Commands User (NOPASSWD: /usr/sbin/iptables, /usr/bin/rm -rf *, /usr/sbin/fdisk)
- Validates: allowed vs blocked commands, password prompt expectations, and `-l`/`-ll` output including rule sources and summaries.

File:
- `tests/integration/test_sudoers_authz_profiles.sh`

1. **Create test file** in `tests/` directory
2. **Include test framework** headers
3. **Implement test functions** following naming conventions
4. **Add to Makefile** build targets
5. **Update documentation**

### Test Framework Usage
```c
#include "test_framework.h"
#include "test_security_framework.h"

// Security test assertion macros
SECURITY_TEST_ASSERT_BLOCKED(test_function, "Test description");
SECURITY_TEST_ASSERT_ALLOWED(test_function, "Test description");

// Standard test assertion macros
TEST_ASSERT(condition, "Test description");
TEST_ASSERT_EQUAL(expected, actual, "Test description");
```

### Test Naming Conventions
- Security tests: `test_security_*`
- Unit tests: `test_unit_*`
- Integration tests: `test_integration_*`
- Feature tests: `test_*_functionality`

## Continuous Integration

### Automated Testing
The test suite is designed for CI/CD integration:

```bash
# CI/CD pipeline example
make clean
make test
if [ $? -eq 0 ]; then
    echo "All tests passed"
else
    echo "Tests failed"
    exit 1
fi
```

### Test Coverage
- **Security Tests**: 95%+ vulnerability coverage
- **Unit Tests**: Core function coverage
- **Integration Tests**: End-to-end scenarios
- **Feature Tests**: User-facing functionality

## Troubleshooting

### Common Issues

**PAM Authentication Tests Failing**
```bash
# Check if PAM is available
pkg-config --exists pam
# If not available, tests will use mock authentication
```

**Permission Errors**
```bash
# Some tests may require specific permissions
sudo ./bin/test_security_privilege_escalation
```

**Build Failures**
```bash
# Clean and rebuild
make clean
make test
```

### Debug Mode
```bash
# Enable verbose output
export VERBOSE=1
./bin/test_security_comprehensive
```

## Test Maintenance

### Regular Testing
- Run security tests after any code changes
- Validate all tests pass before releases
- Update tests when adding new features
- Review test coverage periodically

### Security Test Updates
- Add tests for new attack vectors
- Update tests when security fixes are implemented
- Ensure regression prevention coverage
- Document new security test categories

## Performance Testing

### Basic Performance Validation
```bash
# Time test execution
time ./bin/test_security_comprehensive

# Memory usage monitoring
valgrind --tool=memcheck ./bin/test_unit_auth
```

## Test Documentation

### Test Documentation Files
- **[Security Testing Summary](SECURITY_TESTING_SUMMARY.md)**: Detailed security test documentation
- **[Testing Guide](TESTING_GUIDE.md)**: This comprehensive testing guide
- **Test source files**: Inline documentation in test source code

### Contributing Test Documentation
When adding new tests:
1. Document test purpose and scope
2. Explain test methodology
3. Describe expected results
4. Update relevant documentation files
5. Include usage examples

For more information about contributing, see [CONTRIBUTING.md](../CONTRIBUTING.md).
