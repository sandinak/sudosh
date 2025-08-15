# Security Testing Framework

This document provides comprehensive information about sudosh's security testing framework, which validates protection against various attack vectors and ensures the system maintains its security posture.

## Overview

Sudosh includes an extensive security test suite that validates protection against:
- Command injection attacks
- Privilege escalation attempts  
- Authentication bypass techniques
- Logging evasion methods
- Race condition exploits
- Enhanced security feature validation

## Test Categories

### 1. Command Injection Protection Tests
**File**: `test_security_command_injection.c`
**Binary**: `./bin/test_security_command_injection`

Tests protection against various command injection techniques:

- **Shell Metacharacter Injection**: `;`, `|`, `&&`, `||`, `&`
- **Command Substitution**: `` ` ``, `$()`
- **I/O Redirection**: `>`, `>>`, `<`, `2>`
- **Path Traversal**: `../`, `..\`
- **Environment Variable Injection**: `$HOME`, `$PATH`, `$USER`
- **Null Byte Injection**: Embedded `\x00` characters
- **Unicode/Encoding Attacks**: High-bit characters, URL encoding
- **Format String Injection**: `%s`, `%d`, `%x`, `%n`

### 2. Privilege Escalation Protection Tests
**File**: `test_security_privilege_escalation.c`
**Binary**: `./bin/test_security_privilege_escalation`

Tests protection against privilege escalation techniques:

- **PATH Hijacking**: Malicious PATH manipulation
- **File Descriptor Manipulation**: Inherited FD exploitation
- **Environment Variable Exploitation**: LD_PRELOAD, etc.
- **Sudoers File Manipulation**: Direct sudoers modification attempts

### 3. Authentication Bypass Protection Tests
**File**: `test_security_auth_bypass.c`
**Binary**: `./bin/test_security_auth_bypass`

Tests protection against authentication bypass techniques:

- **NOPASSWD Privilege Bypass**: Unauthorized privilege escalation
- **Sudo Group Membership Bypass**: Group-based access violations
- **PAM Authentication Bypass**: PAM system exploitation
- **Session Hijacking**: Session token manipulation
- **Credential Stuffing**: Common credential attacks
- **Authentication State Manipulation**: Auth state corruption
- **Timing Attacks**: Authentication timing exploitation

### 4. Logging Evasion Protection Tests
**File**: `test_security_logging_evasion.c`
**Binary**: `./bin/test_security_logging_evasion`

Tests protection against logging evasion techniques:

- **Log File Manipulation**: Direct log file modification
- **Syslog Evasion**: Syslog bypass attempts
- **Log Injection**: Malicious log entry injection
- **History Evasion**: Command history manipulation
- **Session Log Bypass**: Session logging circumvention

### 5. Race Condition Protection Tests
**File**: `test_security_race_conditions.c`
**Binary**: `./bin/test_security_race_conditions`

Tests protection against race condition exploits:

- **Authentication Cache Race**: Concurrent cache access
- **File Creation Race**: TOCTOU vulnerabilities
- **Signal Handler Race**: Signal handling exploitation
- **Process Creation Race**: Fork/exec race conditions

### 6. Enhanced Security Features Tests
**File**: `test_security_enhanced_fixes.c`
**Binary**: `./bin/test_security_enhanced_fixes`

Tests enhanced security features implemented during security audit:

- **Enhanced Command Validation**: Comprehensive input validation
- **Enhanced Authentication**: Username validation improvements
- **Cache Race Protection**: File locking mechanisms
- **PATH Security**: Hardcoded secure PATH enforcement
- **File Descriptor Security**: FD cleanup and isolation
- **Environment Sanitization**: Dangerous variable removal

## Running Security Tests

### Individual Test Execution
```bash
# Run specific security test category
./bin/test_security_command_injection
./bin/test_security_privilege_escalation
./bin/test_security_auth_bypass
./bin/test_security_logging_evasion
./bin/test_security_race_conditions
./bin/test_security_enhanced_fixes
```

### Comprehensive Security Assessment
```bash
# Run all security tests
make security-tests

# Run comprehensive security assessment
./bin/test_security_comprehensive
```

### Build Security Tests
```bash
# Build all security tests
make security-tests

# Clean and rebuild
make clean && make security-tests
```

## Test Result Interpretation

### Security Test Results
- **SECURE (attack blocked)**: ✅ Protection is working correctly
- **VULNERABLE (attack succeeded)**: ❌ Security issue detected

### Functionality Test Results  
- **PASSED**: ✅ Feature is working correctly
- **FAILED**: ❌ Feature issue detected

### Example Output
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

## Security Test Framework Architecture

### Test Framework Components
- **test_security_framework.h**: Common security testing utilities
- **Security test macros**: Standardized test assertion macros
- **Vulnerability logging**: Centralized security violation tracking
- **Test result aggregation**: Comprehensive reporting system

### Test Categories Integration
Each test category follows a consistent pattern:
1. **Setup**: Initialize test environment
2. **Attack Simulation**: Execute specific attack vectors
3. **Result Validation**: Verify protection mechanisms
4. **Cleanup**: Reset environment state
5. **Reporting**: Log results and generate reports

## Continuous Security Validation

### Regression Prevention
The security test suite serves as regression prevention by:
- Validating all known attack vectors remain blocked
- Ensuring new code changes don't introduce vulnerabilities
- Providing comprehensive coverage of security mechanisms
- Enabling automated security validation in CI/CD pipelines

### Security Audit Trail
All security test results are logged and can be used for:
- Security compliance reporting
- Vulnerability assessment documentation
- Security posture validation
- Audit trail maintenance

## Contributing to Security Tests

When adding new security tests:
1. Follow the existing test framework patterns
2. Use standardized test macros and utilities
3. Document new attack vectors and protections
4. Ensure comprehensive test coverage
5. Update this documentation with new test categories

For more information about contributing, see [CONTRIBUTING.md](../CONTRIBUTING.md).
