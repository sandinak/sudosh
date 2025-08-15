# Shell Access Restriction Feature

## Overview

The Shell Access Restriction feature is a security enhancement that prevents users with broad "ALL" commands permission from accessing shells (bash, sh, zsh, etc.) unless they have specific shell permissions or are members of the `sudo-shells` group.

## Problem Statement

Users with `ALL` commands permission in sudoers can potentially access any shell, which could bypass security controls and logging. This creates a security risk where users with broad privileges can escape into unrestricted shell environments.

## Solution

The feature implements a two-tier permission system:

1. **Check for ALL commands permission**: If a user has `ALL` commands in their sudoers configuration
2. **Verify shell-specific permissions**: Check if they have explicit shell permissions or are in the `sudo-shells` group
3. **Graceful fallback**: If they lack shell permissions, show a warning and drop them into sudosh instead

## Implementation Details

### Core Functions

- `user_has_all_commands(username)`: Checks if user has ALL commands permission in sudoers
- `user_has_shell_permissions(username)`: Checks for specific shell permissions or `sudo-shells` group membership
- Enhanced `validate_command()`: Implements the restriction logic

### Shell Commands Detected

The feature blocks access to these shell and scripting environments:
- **Shells**: bash, sh, zsh, csh, tcsh, ksh, fish, dash
- **Scripting languages**: python, python3, perl, ruby, node, nodejs
- **Interactive environments**: irb, pry, ipython, ipython3

### Permission Checks

1. **Specific shell permissions**: User has explicit shell commands in sudoers rules
2. **Group membership**: User is a member of the `sudo-shells` group
3. **Fallback behavior**: Drop into sudosh with warning message

## Configuration

### Adding Users to sudo-shells Group

```bash
# Create the sudo-shells group
sudo groupadd sudo-shells

# Add user to the group
sudo usermod -a -G sudo-shells username
```

### Sudoers Configuration Examples

#### User with ALL commands but no shell access (restricted):
```
username ALL=(ALL) ALL
```

#### User with ALL commands and specific shell access (allowed):
```
username ALL=(ALL) ALL, /bin/bash, /bin/sh
```

#### User with group-based shell access (allowed):
```
%sudo-shells ALL=(ALL) ALL
```

## Warning Message

When a user with ALL commands tries to access a shell without proper permissions, they see:

```
=== SHELL ACCESS RESTRICTION ===
WARNING: You have ALL commands permission but are not authorized for shell access.

Shell commands (bash, sh, etc.) are restricted for users with broad privileges
unless they have specific shell permissions or are members of the 'sudo-shells' group.

To gain shell access, ask your administrator to either:
1. Add you to the 'sudo-shells' group, or
2. Add specific shell commands to your sudoers rules

For now, you'll be dropped into sudosh for secure command execution.
=================================
```

## Testing

### Test Scripts

- `tests/test_shell_restriction.sh`: Comprehensive shell restriction tests
- `tests/test_shell_warning.sh`: Warning message verification
- `tests/test_cve_security.sh`: Includes shell restriction in CVE security tests

### Test Environment Variables

- `SUDOSH_TEST_MODE=1`: Enable test mode
- `SUDOSH_TEST_ALL_COMMANDS=1`: Simulate user with ALL commands for testing

### Running Tests

```bash
# Test shell restriction functionality
./tests/test_shell_restriction.sh

# Test warning message display
./tests/test_shell_warning.sh

# Full CVE security test suite (includes shell restriction)
./tests/test_cve_security.sh
```

## Security Benefits

1. **Privilege Separation**: Prevents users with broad privileges from escaping into unrestricted shells
2. **Audit Trail**: All commands remain logged through sudosh
3. **Graceful Degradation**: Users can still work securely through sudosh
4. **Flexible Configuration**: Administrators can grant shell access as needed

## Compatibility

- Works with existing sudoers configurations
- Backward compatible - existing shell permissions continue to work
- No impact on users without ALL commands permission
- Integrates with existing sudosh security features

## Administrative Guidelines

### Best Practices

1. **Review existing sudoers**: Audit users with ALL commands permission
2. **Create sudo-shells group**: For users who legitimately need shell access
3. **Document exceptions**: Maintain records of why users need shell access
4. **Regular audits**: Periodically review shell access permissions

### Migration Strategy

1. **Phase 1**: Deploy feature in monitoring mode
2. **Phase 2**: Identify users affected by restrictions
3. **Phase 3**: Configure appropriate permissions (groups or specific rules)
4. **Phase 4**: Enable enforcement

## Troubleshooting

### Common Issues

**User can't access bash despite being in sudo-shells group**
- Verify group membership: `groups username`
- Check sudoers syntax: `visudo -c`
- Ensure sudosh is using current group information

**Warning message not appearing**
- Verify user has ALL commands: `sudo -l -U username`
- Check sudoers file parsing
- Enable debug logging if needed

**Safe commands being blocked**
- Verify command is in safe commands list
- Check for command injection patterns
- Review security validation logic

## Future Enhancements

- Integration with LDAP/Active Directory groups
- Configurable shell command lists
- Time-based shell access restrictions
- Enhanced logging and reporting
- Integration with security monitoring systems
