# Secure Pipeline Implementation in Sudosh

## Overview

Sudosh now supports secure pipeline execution that allows chaining whitelisted commands while maintaining strict security isolation. This feature enables common data processing workflows like `ps auxw | awk '{print $3}' | grep 9 | less` while preventing security exploits.

## Security Model

### Command Whitelist

Only pre-approved commands are allowed in pipelines:

**Text Processing Utilities:**
- `awk`, `sed`, `grep`, `egrep`, `fgrep`
- `cut`, `sort`, `uniq`, `head`, `tail`
- `tr`, `wc`, `nl`, `cat`, `tac`, `rev`

**Note**: `head` and `tail` commands support all standard options including:
- `head -n NUM` / `head -NUM`: Show first NUM lines
- `head -c NUM`: Show first NUM bytes
- `tail -n NUM` / `tail -NUM`: Show last NUM lines
- `tail -f`: Follow file changes (useful for log monitoring)
- `tail -c NUM`: Show last NUM bytes

**System Information Commands:**
- `ps`, `ls`, `df`, `du`, `who`, `w`
- `id`, `whoami`, `date`, `uptime`, `uname`
- `hostname`, `pwd`, `env`, `printenv`

**Pagers and Viewers:**
- `less`, `more`, `cat`

**Network Utilities (Read-only):**
- `ping`, `traceroute`, `nslookup`, `dig`, `host`

**File Utilities (Read-only):**
- `file`, `stat`, `find`, `locate`, `which`, `whereis`, `type`

### Security Restrictions

1. **Command Validation**: Each command in the pipeline must be on the whitelist
2. **Argument Filtering**: Dangerous options are blocked (e.g., `find -exec`, `find -delete`)
3. **Secure Pager Environment**: Pagers like `less` run with shell escapes disabled
4. **Process Isolation**: Each command runs in its own process with restricted privileges

## Implementation Details

### Pipeline Detection

The system detects pipelines by looking for the `|` character outside of quoted strings:

```c
int is_pipeline_command(const char *input);
```

### Pipeline Parsing

Commands are parsed into a structured format that maintains security boundaries:

```c
struct pipeline_info {
    struct pipeline_command *commands;
    int num_commands;
    int *pipe_fds;
    int num_pipes;
};
```

### Security Validation

Each pipeline undergoes comprehensive security validation:

```c
int validate_pipeline_security(struct pipeline_info *pipeline);
```

This function:
- Verifies all commands are whitelisted
- Checks for dangerous command options
- Validates argument safety

### Secure Execution

Pipeline execution maintains security through:

1. **Process Isolation**: Each command runs in a separate process
2. **Pipe Management**: Secure file descriptor handling between processes
3. **Environment Sanitization**: Dangerous environment variables are removed
4. **Privilege Dropping**: Commands run with minimal necessary privileges

## Secure Pager Implementation

Pager commands (`less`, `more`) receive special security treatment:

### Environment Variables Set:
- `LESSSECURE=1` - Disables shell escapes in less
- `LESSOPEN=""` - Prevents command execution
- `LESSCLOSE=""` - Prevents command execution
- `VISUAL="/bin/false"` - Disables editor spawning
- `EDITOR="/bin/false"` - Disables editor spawning
- `SHELL="/bin/false"` - Disables shell access

### Blocked Features:
- Shell escape sequences (e.g., `!command`)
- Editor invocation (e.g., `v` command in less)
- File execution through LESSOPEN/LESSCLOSE

## Usage Examples

### Safe Pipeline Commands

```bash
# System monitoring
ps auxw | awk '{print $3}' | grep 9 | less

# File analysis
ls -la | head -10 | tail -5

# Log processing and monitoring
cat /var/log/syslog | grep error | wc -l
tail -f /var/log/syslog | grep WARNING    # Real-time log monitoring
cat large_file.txt | head -100 | tail -20 # Lines 81-100

# Data sampling and analysis
ps aux | head -20                          # First 20 processes
ps aux | sort -k3 -nr | head -10          # Top 10 CPU users
df -h | tail -n +2 | head -5               # First 5 mounted filesystems

# Network diagnostics
ping -c 5 google.com | grep "time="
```

### Blocked Dangerous Commands

```bash
# Command injection attempts
ps aux | rm -rf /          # rm not whitelisted
ls | bash                  # bash not whitelisted
cat file | sh              # sh not whitelisted

# Dangerous find operations
find /tmp | find /tmp -exec rm {} \;  # -exec blocked
```

## Testing

Comprehensive test suite validates:

1. **Pipeline Detection**: Correct identification of pipe vs non-pipe commands
2. **Command Whitelist**: Proper allow/deny decisions
3. **Pipeline Parsing**: Accurate command separation and argument handling
4. **Security Validation**: Rejection of dangerous command combinations
5. **Environment Setup**: Proper security variable configuration

Run tests with:
```bash
make && ./bin/test_pipeline
```

## Integration with Existing Security

The pipeline feature integrates seamlessly with existing sudosh security:

1. **Sudoers Integration**: Pipeline commands still respect sudoers permissions
2. **Audit Logging**: All pipeline executions are logged with full command details
3. **User Validation**: Standard user authentication and authorization applies
4. **Resource Limits**: Existing resource constraints remain in effect

## Configuration

No additional configuration is required. The pipeline feature:

- Uses the same security model as single commands
- Respects existing sudoers rules
- Maintains all current logging and audit capabilities
- Preserves backward compatibility

## Security Considerations

1. **Principle of Least Privilege**: Only essential commands are whitelisted
2. **Defense in Depth**: Multiple validation layers prevent bypass attempts
3. **Secure by Default**: All potentially dangerous features are disabled
4. **Audit Trail**: Complete logging of all pipeline operations
5. **Process Isolation**: Each command runs independently with minimal privileges

This implementation provides powerful data processing capabilities while maintaining the strict security posture that sudosh is designed to enforce.
