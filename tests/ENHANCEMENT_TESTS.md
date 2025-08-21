# Sudosh Security Enhancement Tests

This document describes the comprehensive test suite for the three security enhancements implemented in sudosh.

## Overview

The test suite covers:
1. **NSS-based user/group information retrieval** - Tests for removing sudo dependency
2. **Secure pipeline support** - Tests for safe command chaining with auditing
3. **Safe text processing and controlled redirection** - Tests for secure text commands and file operations
4. **Security vulnerability regression** - Tests to prevent security regressions
5. **Integration and compatibility** - Tests to ensure existing functionality remains intact

## Test Structure

```
tests/
├── unit/                           # Unit tests for individual functions
│   ├── test_nss_enhancements.c     # NSS functionality tests
│   ├── test_pipeline_security.c    # Pipeline security tests
│   └── test_text_processing_redirection.c  # Text processing tests
├── security/                       # Security-focused regression tests
│   └── test_security_enhancements.c
├── integration/                    # Integration and compatibility tests
│   └── test_enhancement_integration.c
├── run_enhancement_tests.sh        # Main test runner script
└── ENHANCEMENT_TESTS.md           # This documentation
```

## Running Tests

### Quick Test Run
```bash
make test-enhancements
```

### Detailed Test Run with Reports
```bash
cd tests
./run_enhancement_tests.sh
```

### Clean Run (Remove Previous Reports)
```bash
cd tests
./run_enhancement_tests.sh --clean
```

### All Tests (Original + Enhancements)
```bash
make test-all
```

## Test Categories

### 1. NSS Enhancement Tests (`test_nss_enhancements.c`)

**Positive Tests:**
- `test_get_user_info_files_valid()` - Valid username lookup
- `test_check_admin_groups_files_root()` - Admin group membership
- `test_check_sudo_privileges_nss()` - NSS-based privilege checking
- `test_nss_config_reading()` - NSS configuration parsing
- `test_get_user_info_nss()` - Integrated NSS user lookup

**Negative Tests:**
- `test_get_user_info_files_invalid()` - Invalid username handling
- `test_get_user_info_files_null()` - NULL input handling
- `test_check_admin_groups_files_null()` - NULL input validation
- `test_check_sudo_privileges_nss_null()` - NULL parameter handling

**Security Tests:**
- `test_nss_no_sudo_dependency()` - Verify no sudo dependency
- `test_missing_passwd_file()` - Error handling for missing files

### 2. Pipeline Security Tests (`test_pipeline_security.c`)

**Positive Tests:**
- `test_parse_pipeline_valid()` - Valid pipeline parsing
- `test_validate_pipeline_security_valid()` - Whitelisted command validation
- `test_is_whitelisted_pipe_command()` - Command whitelist verification
- `test_complex_valid_pipeline()` - Complex but safe pipeline validation
- `test_pipeline_audit_logging()` - Audit logging functionality

**Negative Tests:**
- `test_parse_pipeline_invalid()` - Invalid pipeline rejection
- `test_validate_pipeline_security_invalid()` - Dangerous command blocking
- `test_pipeline_dangerous_find()` - Dangerous find command blocking
- `test_pipeline_command_injection()` - Command injection prevention

**Edge Cases:**
- `test_single_command_pipeline()` - Single command handling
- `test_pipeline_memory_management()` - Memory leak prevention

### 3. Text Processing and Redirection Tests (`test_text_processing_redirection.c`)

**Positive Tests:**
- `test_is_text_processing_command()` - Text command identification
- `test_validate_text_processing_safe()` - Safe text command validation
- `test_is_safe_redirection_target()` - Safe redirection target validation
- `test_validate_safe_redirection()` - Safe redirection validation
- `test_text_processing_in_safe_commands()` - Text commands in safe list

**Negative Tests:**
- `test_validate_text_processing_dangerous_sed()` - Dangerous sed blocking
- `test_validate_text_processing_dangerous_awk()` - Dangerous awk blocking
- `test_validate_text_processing_dangerous_grep()` - Dangerous grep blocking
- `test_validate_text_processing_command_injection()` - Command injection prevention

**Security Tests:**
- `test_text_processing_dangerous_blocked()` - Dangerous pattern blocking
- `test_text_processing_with_redirection()` - Combined validation

### 4. Security Enhancement Regression Tests (`test_security_enhancements.c`)

**Command Injection Prevention:**
- `test_nss_command_injection_prevention()` - NSS function injection prevention
- `test_shell_escape_prevention()` - Text processing escape prevention
- `test_privilege_escalation_prevention()` - Pipeline privilege escalation prevention

**Path Security:**
- `test_path_traversal_prevention()` - Path traversal attack prevention
- `test_symlink_attack_prevention()` - Symlink attack prevention

**Buffer Security:**
- `test_buffer_overflow_protection()` - Buffer overflow prevention
- `test_null_byte_injection_prevention()` - Null byte injection prevention

**System Security:**
- `test_race_condition_prevention()` - Race condition prevention
- `test_memory_leak_prevention()` - Memory leak prevention
- `test_backward_compatibility()` - Existing security preservation

### 5. Integration Tests (`test_enhancement_integration.c`)

**Compatibility Tests:**
- `test_existing_functionality_unchanged()` - Existing feature preservation
- `test_nss_fallback_mechanisms()` - Fallback mechanism validation
- `test_different_sudoers_configurations()` - Various sudoers support
- `test_sssd_integration()` - SSSD integration testing

**Performance Tests:**
- `test_performance_impact()` - Performance impact measurement
- `test_memory_usage()` - Memory usage validation
- `test_concurrent_access()` - Concurrent access safety

**Integration Tests:**
- `test_integrated_functionality()` - All enhancements working together
- `test_comprehensive_error_handling()` - Error handling across features

## Test Results and Reports

### Test Output
Tests generate detailed reports in `tests/reports/`:
- `test_summary.txt` - Overall test summary
- `*_output.log` - Individual test output
- `*_result.log` - Test exit codes
- `*_compile.log` - Compilation logs

### Pass/Fail Criteria

**Pass Criteria:**
- All security controls function correctly
- No memory leaks detected
- Performance within acceptable limits
- Backward compatibility maintained
- Error handling robust

**Fail Criteria:**
- Security bypass possible
- Memory leaks detected
- Significant performance degradation
- Existing functionality broken
- Crashes or undefined behavior

## Security Test Scenarios

### Attack Vectors Tested

1. **Command Injection:**
   - Shell metacharacters in usernames
   - Command substitution in parameters
   - Pipeline command chaining attacks

2. **Path Traversal:**
   - `../` sequences in redirection targets
   - Symlink-based path traversal
   - Home directory escape attempts

3. **Shell Escapes:**
   - sed `e`, `w`, `r` commands
   - awk `system()` calls
   - grep execution flags

4. **Privilege Escalation:**
   - Pipeline privilege chaining
   - sudo command injection
   - Group membership manipulation

5. **Buffer Attacks:**
   - Long input strings
   - Null byte injection
   - Format string attacks

## Maintenance

### Adding New Tests
1. Create test function following naming convention: `test_feature_scenario()`
2. Use `TEST_ASSERT_*` macros for validation
3. Add to appropriate test suite using `RUN_TEST()`
4. Update this documentation

### Test Framework Functions
- `TEST_ASSERT_EQ(expected, actual, message)` - Equality assertion
- `TEST_ASSERT_STR_EQ(expected, actual, message)` - String equality
- `TEST_ASSERT_NOT_NULL(ptr, message)` - Non-null assertion
- `TEST_ASSERT_NULL(ptr, message)` - Null assertion
- `TEST_ASSERT(condition, message)` - Boolean assertion

### Debugging Failed Tests
1. Check `tests/reports/test_summary.txt` for overview
2. Review individual `*_output.log` files for details
3. Check `*_compile.log` for compilation issues
4. Run tests with `--verbose` flag for detailed output

## Continuous Integration

The test suite is designed to be run in CI/CD pipelines:
- Exit code 0 on success, non-zero on failure
- Detailed logging for debugging
- Performance benchmarks for regression detection
- Memory usage monitoring

## Security Compliance

These tests help ensure compliance with:
- Principle of least privilege
- Defense in depth
- Input validation requirements
- Audit trail requirements
- Secure coding standards
