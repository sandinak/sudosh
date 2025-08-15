# Pipeline Security Regression Testing

## Overview

This document describes the comprehensive regression test suite designed to ensure that pipeline security features in sudosh do not regress as the codebase evolves. The test suite validates all critical security boundaries and functional requirements.

## Test Suite Components

### 1. Core Regression Test (`tests/test_pipeline_regression.c`)

A comprehensive C test program that validates:

- **Pipeline Detection**: Ensures pipe vs non-pipe command identification works correctly
- **Security Whitelist**: Validates that dangerous commands remain blocked and safe commands remain allowed
- **Dangerous Combinations**: Tests that malicious pipeline combinations are rejected
- **Pager Security**: Verifies secure pager environment configuration
- **Parsing Edge Cases**: Handles malformed input correctly
- **Injection Prevention**: Blocks command injection and path traversal attempts
- **Performance & Memory**: Tests with complex pipelines and memory cleanup
- **Backward Compatibility**: Ensures existing functionality isn't broken

### 2. Automated Test Runner (`scripts/run_pipeline_regression_tests.sh`)

A bash script that:

- Checks prerequisites and build environment
- Builds the test suite automatically
- Runs smoke tests before full regression testing
- Generates detailed reports
- Provides clear exit codes for CI/CD integration

### 3. CI/CD Integration (`.github/workflows/pipeline-security-regression.yml`)

GitHub Actions workflow that:

- Runs tests on multiple compilers (gcc, clang)
- Tests both debug and release builds
- Performs memory leak detection with Valgrind
- Runs static analysis with cppcheck
- Validates security boundaries with custom tests
- Sends notifications on critical failures

## Running the Tests

### Quick Smoke Test

```bash
make test-pipeline-smoke
```

This runs a quick validation to ensure basic pipeline functionality works.

### Full Regression Test Suite

```bash
make test-pipeline-regression
```

This runs the complete regression test suite with detailed reporting.

### Manual Test Execution

```bash
# Build and run manually
make clean && make
./scripts/run_pipeline_regression_tests.sh

# Run only smoke test
./scripts/run_pipeline_regression_tests.sh --smoke-only
```

## Test Results Interpretation

### Exit Codes

- **0**: All tests passed - safe to deploy
- **1**: Some tests failed - review required before deployment
- **2**: Critical security regression detected - DO NOT DEPLOY
- **3+**: Test execution error - fix environment issues

### Output Files

- `pipeline_regression_test.log`: Detailed test execution log
- `pipeline_regression_report.txt`: Summary report with recommendations

### Critical vs Non-Critical Failures

**Critical Failures** indicate security regressions:
- Dangerous commands becoming whitelisted
- Security validation bypasses
- Pager security environment compromised
- Command injection vulnerabilities

**Non-Critical Failures** indicate functional issues:
- Parsing edge cases
- Performance degradation
- Memory leaks (in debug builds)

## Security Test Categories

### 1. Command Whitelist Validation

Tests that these commands remain **BLOCKED**:
- File system: `rm`, `chmod`, `chown`, `dd`, `mount`, `umount`
- Shells: `bash`, `sh`, `zsh`, `csh`, `tcsh`, `ksh`, `fish`
- Network: `iptables`, `netfilter`, `tc`
- Process: `kill`, `killall`, `pkill`
- System: `fdisk`, `mkfs`, `passwd`, `sudo`, `su`

Tests that these commands remain **ALLOWED**:
- Text processing: `awk`, `sed`, `grep`, `cut`, `sort`, `head`, `tail`
- System info: `ps`, `ls`, `df`, `du`, `who`, `w`, `id`
- Pagers: `less`, `more`, `cat`

### 2. Injection Prevention

Tests blocking of:
- Command substitution: `$(command)`, `` `command` ``, `${VAR}`
- Path traversal: `../../../bin/bash`, `./malicious_script`
- Shell metacharacters in dangerous contexts

### 3. Pager Security

Validates that secure pagers have:
- `LESSSECURE=1` (disables shell escapes)
- `LESSOPEN=""` (prevents command execution)
- `LESSCLOSE=""` (prevents command execution)
- `VISUAL="/bin/false"` (disables editor spawning)
- `EDITOR="/bin/false"` (disables editor spawning)

## Integration with Development Workflow

### Pre-Commit Testing

```bash
# Add to pre-commit hook
make test-pipeline-smoke || {
    echo "Pipeline smoke test failed - commit aborted"
    exit 1
}
```

### CI/CD Pipeline Integration

The regression tests are automatically triggered on:
- Push to main/develop branches
- Pull requests affecting pipeline code
- Daily scheduled runs (2 AM UTC)
- Manual workflow dispatch

### Release Validation

Before any release:

1. Run full regression test suite
2. Verify all tests pass with exit code 0
3. Review generated security report
4. Confirm no critical failures detected

## Adding New Tests

### When to Add Tests

Add regression tests when:
- Adding new whitelisted commands
- Modifying security validation logic
- Changing pipeline parsing behavior
- Fixing security vulnerabilities
- Adding new pager support

### Test Structure

```c
void test_new_feature_regression(void) {
    printf("\n=== New Feature Regression Tests ===\n");
    
    /* Critical security tests */
    CRITICAL_TEST(security_condition, "Security boundary maintained");
    
    /* Functional regression tests */
    REGRESSION_TEST(functional_condition, "Feature works correctly");
}
```

### Test Categories

- **CRITICAL_TEST**: Security boundaries that must never be violated
- **REGRESSION_TEST**: Functional requirements that should be maintained

## Troubleshooting

### Common Issues

1. **Build Failures**: Ensure all dependencies installed (`libpam0g-dev`, `build-essential`)
2. **Permission Errors**: Some tests may require specific file permissions
3. **Environment Issues**: Tests assume standard Linux environment

### Debug Mode

```bash
# Build with debug symbols
make clean
make CFLAGS="-Wall -Wextra -std=c99 -g -O0 -D_GNU_SOURCE -DDEBUG"

# Run with Valgrind
valgrind --leak-check=full ./bin/test_pipeline_regression
```

### Manual Security Validation

```bash
# Test specific security boundaries
echo "Testing dangerous command blocking..."
./bin/test_pipeline_regression | grep "CRITICAL"

# Verify whitelist integrity
grep -n "whitelisted_pipe_commands" src/pipeline.c
```

## Maintenance

### Regular Tasks

1. **Weekly**: Review test results from scheduled runs
2. **Monthly**: Update test cases based on new threats
3. **Per Release**: Validate all security boundaries
4. **After Security Updates**: Run full regression suite

### Test Suite Updates

When updating the test suite:

1. Maintain backward compatibility
2. Add tests for new security features
3. Update documentation
4. Verify CI/CD integration still works

This regression test suite provides comprehensive protection against security regressions while ensuring the pipeline functionality continues to work correctly as the codebase evolves.
