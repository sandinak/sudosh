# Sudosh Alias Validation Enhancement

## Overview

This enhancement adds comprehensive security validation for aliases in sudosh, ensuring that aliases are checked for valid commands before being expanded into the workspace. This prevents alias-based command injection, privilege escalation, and other security vulnerabilities.

## Problem Statement

Previously, sudosh would expand aliases without thorough security validation of the expanded commands. This created potential security vulnerabilities where:

1. **Alias-based command injection**: Malicious aliases could inject dangerous commands
2. **Privilege escalation**: Aliases could override critical system commands
3. **Environment manipulation**: Aliases could modify environment variables
4. **Recursive aliases**: Self-referential aliases could cause infinite loops
5. **Audit trail gaps**: Alias expansions weren't comprehensively logged

## Solution

The enhancement implements multi-layered security validation for aliases:

### 1. Alias Creation Validation

When aliases are created (via `add_alias()` or loaded from shell RC files), they undergo:

- **Dangerous pattern detection** (`check_dangerous_alias_patterns()`)
- **Expansion safety validation** (`validate_alias_expansion_safety()`)
- **Standard command validation** (`validate_command()`)
- **Dangerous command detection** (`is_dangerous_command()`)

### 2. Alias Expansion Validation

When aliases are expanded (via `expand_aliases()`), the expanded commands undergo:

- **Expanded command validation** (`validate_expanded_alias_command()`)
- **Recursion detection**
- **Shell metacharacter detection**
- **Redirection safety validation**
- **Pipeline security validation**
- **Comprehensive audit logging**

## Implementation Details

### Files Modified

- `src/shell_enhancements.c` - Enhanced alias system with security validation
- `src/main.c` - Updated main loop to handle failed alias expansion
- `src/sudosh.h` - Added function declarations
- `tests/unit/test_alias_validation.c` - Comprehensive test suite

### Key Functions Added

#### `check_dangerous_alias_patterns()`
Detects dangerous alias patterns including:
- Critical command override attempts (`sudo`, `passwd`, `su`, etc.)
- Aliases containing privileged commands
- Environment manipulation (`PATH=`, `LD_PRELOAD=`, etc.)
- Execution from dangerous locations (`/tmp/`, `~/.`, `/dev/shm/`)

#### `validate_alias_expansion_safety()`
Tests alias expansion safety by:
- Checking for self-referential aliases
- Detecting recursive references
- Testing expansion with dummy arguments
- Validating the expanded result

#### `validate_expanded_alias_command()`
Validates expanded commands for:
- Standard security compliance
- Shell metacharacter injection
- Command substitution attempts
- Unsafe redirection
- Dangerous pipeline operations

#### `expand_aliases_internal()`
Internal expansion function without security validation (used for testing during alias creation)

### Security Controls

#### Critical Command Protection
Prevents aliasing of critical system commands:
```
sudo, su, passwd, chown, chmod, chgrp, mount, umount, systemctl, 
service, init, reboot, shutdown, halt, poweroff, iptables, 
firewall-cmd, ufw, crontab, at, batch, ssh, scp, rsync, wget, curl
```

#### Environment Protection
Blocks environment manipulation attempts:
```
PATH=, LD_PRELOAD=, LD_LIBRARY_PATH=, SHELL=, HOME=, USER=, 
LOGNAME=, SUDO_, export PATH, export LD_PRELOAD, etc.
```

#### Location Protection
Prevents execution from dangerous locations:
```
/tmp/, /var/tmp/, /dev/shm/, ~/.
```

#### Shell Metacharacter Protection
Blocks shell injection patterns:
```
;, &, &&, ||, `, $(, ), redirection operators
```

## Usage Examples

### Safe Aliases (Accepted)
```bash
alias ll='ls -la'                    # Safe file listing
alias grep='grep --color=auto'       # Safe grep enhancement
alias df='df -h'                     # Safe disk usage
alias myps='ps aux | head -20'       # Safe process listing with pipeline
```

### Dangerous Aliases (Rejected)
```bash
alias sudo='ls'                      # Critical command override
alias backup='sudo rsync'            # Contains privileged command
alias setpath='PATH=/tmp:$PATH'       # Environment manipulation
alias tmpexec='/tmp/malicious'        # Dangerous location
alias chain='ls; rm file'            # Command injection
alias ls='ls -la'                    # Self-referential
```

## Audit Trail

### Successful Alias Expansion
```
ALIAS_EXPANSION: user=username alias expanded: ll -> ls -la
```

### Blocked Dangerous Expansion
```
SECURITY_VIOLATION: user=username dangerous alias expansion blocked: sudo -> malicious_command
```

### Failed Alias Creation
```
SECURITY_VIOLATION: user=username dangerous alias creation blocked: sudo -> ls
```

## Integration with Existing Security

The alias validation integrates seamlessly with existing sudosh security features:

- **NSS-based permission checking**: Expanded commands are validated against user permissions
- **Pipeline security validation**: Aliased pipelines undergo the same security checks
- **Text processing validation**: Aliased text commands are validated for shell escapes
- **Safe redirection validation**: Aliased redirections are checked for safe targets
- **Command injection prevention**: All existing injection prevention applies to aliases

## Testing

### Test Suite
- `tests/unit/test_alias_validation.c` - 7 comprehensive test functions
- Tests cover dangerous pattern detection, expansion safety, edge cases
- Integration with existing test framework

### Manual Testing
```bash
# Compile with enhancements
make clean && make

# Run comprehensive tests
make test-enhancements

# Test in sudosh session
bin/sudosh
> alias sudo='ls'          # Should be rejected
> alias ll='ls -la'        # Should be accepted
> ll                       # Should expand safely
```

## Performance Impact

- **Minimal overhead**: Validation only occurs during alias creation and expansion
- **Efficient pattern matching**: Uses optimized string operations
- **Cached results**: Alias validation results are cached after creation
- **No impact on non-alias commands**: Zero overhead for direct command execution

## Security Benefits

1. **Prevents privilege escalation** through alias manipulation
2. **Blocks command injection** via malicious alias expansion
3. **Stops environment manipulation** through alias-based attacks
4. **Detects recursive aliases** that could cause denial of service
5. **Maintains audit trail** of all alias operations
6. **Integrates with existing controls** for comprehensive protection
7. **Validates both creation and expansion** for defense in depth

## Backward Compatibility

- **Existing safe aliases continue to work** without modification
- **Dangerous aliases are blocked** with clear error messages
- **Shell RC file loading** applies the same validation
- **Fallback mechanisms** preserve functionality when validation fails
- **No breaking changes** to existing sudosh functionality

## Configuration

No additional configuration required. The enhancement:
- Uses existing sudosh security settings
- Integrates with current logging configuration
- Respects existing alias system settings
- Works with all supported shell environments

## Future Enhancements

Potential future improvements:
- **Whitelist-based alias approval** for specific environments
- **User-specific alias policies** based on privilege levels
- **Dynamic alias validation** based on runtime context
- **Machine learning-based** suspicious alias detection
- **Integration with external** security policy engines

## Summary

This enhancement significantly improves sudosh security by:

- **Validating aliases before expansion** to prevent security bypasses
- **Implementing comprehensive pattern detection** for dangerous aliases
- **Maintaining detailed audit trails** for all alias operations
- **Integrating with existing security controls** for unified protection
- **Providing clear feedback** when dangerous aliases are blocked

The implementation follows sudosh's security-first approach while maintaining usability and performance, ensuring that aliases enhance productivity without compromising security.
