# Sudosh Security Enhancements

This document describes the three key security and functionality improvements implemented in sudosh.

## Enhancement 1: Remove sudo dependency for user/group information retrieval

### Problem
Previously, sudosh relied on `sudo -l -U username` commands to retrieve user permissions and group information, creating a circular dependency on sudo itself.

### Solution
Implemented direct parsing of NSS (Name Service Switch) data sources:

#### Files Modified
- `src/nss.c` - Enhanced NSS integration
- `src/auth.c` - Updated authentication functions
- `src/sudoers.c` - Added direct sudoers parsing
- `src/sudosh.h` - Added function declarations

#### Key Functions Added
- `get_user_info_files()` - Direct /etc/passwd parsing
- `get_user_info_sssd_direct()` - SSSD integration without sudo
- `check_admin_groups_files()` - Direct /etc/group parsing
- `check_sudo_privileges_nss()` - NSS-based privilege checking
- `check_command_permission_nss()` - Direct sudoers validation
- `check_sudoers_command_permission()` - Sudoers file parsing

#### Benefits
- Eliminates circular dependency on sudo
- Improves performance by avoiding subprocess calls
- Provides more reliable permission checking
- Maintains compatibility with existing systems

## Enhancement 2: Add secure pipe support with command auditing

### Problem
Previously, all pipe operations were blocked for security reasons, limiting functionality.

### Solution
Implemented secure pipeline support with comprehensive validation and auditing:

#### Files Modified
- `src/security.c` - Updated pipeline validation
- `src/pipeline.c` - Enhanced pipeline execution and logging
- `src/sudosh.h` - Added function declarations

#### Key Functions Added
- `validate_secure_pipeline()` - Pipeline security validation
- `validate_command_for_pipeline()` - Individual command validation
- `log_pipeline_start()` - Pipeline execution logging
- `log_pipeline_completion()` - Pipeline completion logging
- `validate_pipeline_with_permissions()` - Permission-based validation

#### Security Features
- Individual command validation in pipelines
- Whitelist-based command filtering
- Comprehensive audit logging for all pipeline operations
- Permission checking for each command in the pipeline
- Prevention of unauthorized command chaining

#### Supported Pipeline Commands
Text processing: `awk`, `sed`, `grep`, `cut`, `sort`, `uniq`, `head`, `tail`, `wc`, `cat`
System info: `ps`, `ls`, `df`, `du`, `who`, `w`, `id`, `whoami`, `date`, `uptime`
File utilities: `file`, `stat`, `find`, `locate`, `which`, `type`

## Enhancement 3: Add support for safe text processing and controlled file redirection

### Problem
Text processing commands and file redirection were completely blocked, limiting legitimate use cases.

### Solution
Implemented safe text processing with security controls and controlled redirection:

#### Files Modified
- `src/security.c` - Added text processing validation and redirection controls
- `src/sudosh.h` - Added function declarations

#### Key Functions Added
- `validate_safe_redirection()` - Redirection target validation
- `is_safe_redirection_target()` - Directory safety checking
- `is_text_processing_command()` - Text command identification
- `validate_text_processing_command()` - Security validation for text commands

#### Safe Text Processing Commands
Added with security controls:
- `grep`, `egrep`, `fgrep` - Pattern matching with escape prevention
- `sed` - Stream editing with shell escape blocking
- `awk`/`gawk` - Text processing with system() call prevention
- `cut`, `sort`, `uniq` - Basic text utilities
- `head`, `tail`, `wc`, `cat` - File reading utilities

#### Redirection Security
**Allowed redirection targets:**
- `/tmp/` - Temporary files
- `/var/tmp/` - System temporary files
- `/home/` and `/Users/` - User home directories
- `~/` - Current user's home directory
- Relative paths in current directory

**Blocked redirection targets:**
- `/etc/` - System configuration
- `/usr/` - System binaries and libraries
- `/var/log/` - System logs
- `/var/lib/`, `/var/run/` - System data
- `/sys/`, `/proc/`, `/dev/` - System interfaces
- `/boot/`, `/root/` - System and root directories

#### Security Controls for Text Processing
- **sed**: Blocks `e`, `w`, `r`, `W`, `R` commands that can execute shell commands
- **awk**: Prevents `system()` calls and unsafe file operations
- **grep**: Blocks execution flags and dangerous include patterns
- **All commands**: Prevents command substitution, shell metacharacters, and injection

## Implementation Details

### Compilation
All enhancements compile successfully with existing build system:
```bash
make clean && make
```

### Backward Compatibility
- Fallback mechanisms preserve existing functionality
- Original sudo-based methods available as last resort
- Existing security audit capabilities maintained
- No breaking changes to existing configurations

### Security Model
- Maintains principle of least privilege
- Comprehensive audit logging for all operations
- Individual permission validation for each command
- Prevention of privilege escalation through command chaining
- Protection against shell escapes and command injection

### Testing
- All enhancements tested for compilation
- Security validation functions implemented
- Comprehensive error handling and user feedback
- Audit logging for security events

## Summary

These three enhancements significantly improve sudosh's functionality while maintaining its strong security posture:

1. **Independence**: Removes dependency on sudo for core operations
2. **Functionality**: Enables secure pipeline operations with full auditing
3. **Usability**: Allows safe text processing and controlled file operations

All changes follow the established sudosh security model and maintain comprehensive audit capabilities.
