# Contributing to Sudosh

Thank you for your interest in contributing to Sudosh! This document provides guidelines and processes for contributing to the project.

## Table of Contents

- [Getting Started](#getting-started)
- [Development Process](#development-process)
- [Code Standards](#code-standards)
- [Testing Requirements](#testing-requirements)
- [Pull Request Process](#pull-request-process)
- [Issue Reporting](#issue-reporting)
- [Security Considerations](#security-considerations)

## Getting Started

### Prerequisites

Before contributing, ensure you have:

- **C compiler** (GCC recommended)
- **Make** build system
- **PAM development headers** (`libpam0g-dev` on Debian/Ubuntu, `pam-devel` on RHEL/CentOS)
- **Git** for version control
- **Basic understanding** of Unix/Linux security concepts

### Fork and Clone

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/sudosh.git
   cd sudosh
   ```
3. **Add upstream remote**:
   ```bash
   git remote add upstream https://github.com/sandinak/sudosh.git
   ```

### Build and Test

1. **Build the project**:
   ```bash
   make clean && make
   ```
2. **Run tests**:
   ```bash
   make tests
   make test
   ```
3. **Verify functionality**:
   ```bash
   ./bin/sudosh --version
   ```

## Development Process

### Branch Strategy

- **main**: Stable, production-ready code
- **feature/**: New features (`feature/auth-improvements`)
- **bugfix/**: Bug fixes (`bugfix/memory-leak`)
- **security/**: Security-related changes (`security/privilege-escalation-fix`)

### Workflow

1. **Create a feature branch**:
   ```bash
   git checkout -b feature/your-feature-name
   ```
2. **Make your changes** following code standards
3. **Write/update tests** for your changes
4. **Test thoroughly** including edge cases
5. **Update documentation** as needed
6. **Commit with clear messages**
7. **Push and create PR**

### Keeping Your Fork Updated

```bash
git fetch upstream
git checkout main
git merge upstream/main
git push origin main
```

## Code Standards

### C Coding Style

- **Indentation**: 4 spaces (no tabs)
- **Line length**: Maximum 100 characters
- **Braces**: K&R style (opening brace on same line)
- **Naming**: 
  - Functions: `snake_case`
  - Variables: `snake_case`
  - Constants: `UPPER_CASE`
  - Structs: `snake_case` with `_t` suffix

### Example Code Style

```c
// Good
int authenticate_user(const char *username, const char *password) {
    if (!username || !password) {
        return AUTH_FAILURE;
    }
    
    // Implementation here
    return AUTH_SUCCESS;
}

// Avoid
int authenticateUser(const char* username,const char* password)
{
if(!username||!password)
return AUTH_FAILURE;
// Implementation here
return AUTH_SUCCESS;
}
```

### Security Guidelines

- **Input validation**: Always validate user input
- **Memory management**: Check for leaks and buffer overflows
- **Privilege handling**: Follow principle of least privilege
- **Error handling**: Fail securely, don't expose sensitive information
- **Logging**: Log security-relevant events appropriately

## Testing Requirements

### Test Categories

1. **Unit Tests**: Test individual functions
2. **Integration Tests**: Test component interactions
3. **Security Tests**: Test security boundaries and edge cases
4. **Packaging Tests**: Verify package generation

### Writing Tests

- **Location**: Place tests in `tests/` directory
- **Naming**: `test_[component]_[functionality].c`
- **Framework**: Use the existing test framework in `tests/test_framework.h`
- **Coverage**: Aim for comprehensive test coverage of new code

### Example Test

```c
#include "test_framework.h"
#include "../src/sudosh.h"

void test_your_function() {
    // Setup
    char *input = "test_input";
    
    // Execute
    int result = your_function(input);
    
    // Verify
    assert_equals(EXPECTED_VALUE, result, "Function should return expected value");
}

int main() {
    printf("=== Your Component Tests ===\n");
    
    run_test(test_your_function);
    
    print_test_results();
    return get_test_exit_code();
}
```

### Running Tests

```bash
# Build and run all tests
make test

# Run specific test categories
make unit-test
make integration-test

# Run individual tests
./bin/test_your_component
```

## Pull Request Process

### Before Submitting

1. **Ensure all tests pass**:
   ```bash
   make clean && make && make test
   ```
2. **Update documentation** if needed
3. **Add/update tests** for your changes
4. **Check for memory leaks** and security issues
5. **Verify packaging** still works:
   ```bash
   make packages  # If packaging tools available
   ```

### PR Requirements

- **Clear title** describing the change
- **Detailed description** explaining:
  - What the change does
  - Why it's needed
  - How it was tested
  - Any breaking changes
- **Reference issues** if applicable
- **Include test results** or screenshots if relevant

### PR Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Security improvement
- [ ] Documentation update
- [ ] Performance improvement

## Testing
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Security tests pass
- [ ] Manual testing completed

## Checklist
- [ ] Code follows project style guidelines
- [ ] Self-review completed
- [ ] Documentation updated
- [ ] Tests added/updated
- [ ] No new compiler warnings
```

### Review Process

1. **Automated checks** must pass
2. **Code review** by maintainers
3. **Security review** for security-related changes
4. **Testing verification** on multiple platforms if possible
5. **Documentation review** for user-facing changes

## Issue Reporting

### Bug Reports

Include:
- **Environment**: OS, version, compiler
- **Steps to reproduce**
- **Expected vs actual behavior**
- **Error messages** or logs
- **Minimal test case** if possible

### Feature Requests

Include:
- **Use case** description
- **Proposed solution** or approach
- **Alternatives considered**
- **Security implications**

### Security Issues

**Do not** report security vulnerabilities in public issues. Instead:
1. Email maintainers directly
2. Provide detailed description
3. Include proof of concept if safe
4. Allow time for fix before disclosure

## Security Considerations

### Code Review Focus

- **Input validation** and sanitization
- **Memory safety** (buffer overflows, leaks)
- **Privilege escalation** prevention
- **Authentication** and authorization logic
- **Logging** of security events

### Testing Security Changes

- Test with **malicious inputs**
- Verify **privilege boundaries**
- Check **error handling** paths
- Test **edge cases** and corner cases
- Validate **logging** and audit trails

## Documentation Standards

### Code Documentation

- **Function headers** with purpose, parameters, return values
- **Complex logic** explained with comments
- **Security considerations** noted where relevant
- **Examples** for public APIs

### User Documentation

- **Clear instructions** for installation and usage
- **Examples** of common use cases
- **Troubleshooting** guides
- **Security best practices**

## Getting Help

- **GitHub Issues**: For bugs and feature requests
- **GitHub Discussions**: For questions and general discussion
- **Code Review**: Ask questions in PR comments
- **Documentation**: Check existing docs first

## Recognition

Contributors will be recognized in:
- **CHANGELOG.md** for significant contributions
- **README.md** contributors section
- **Git history** with proper attribution

Thank you for contributing to Sudosh and helping make it more secure and useful for everyone!
