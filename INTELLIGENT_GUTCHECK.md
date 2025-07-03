# Intelligent Gut Check System

## Overview

The sudosh intelligent gut check system has been significantly improved to reduce false positives while maintaining strong security. The new system intelligently analyzes commands to distinguish between safe read-only operations and potentially dangerous actions.

## Key Improvements

### Before (Old System)
- **Overly broad warnings**: ANY command accessing `/etc`, `/var/log`, or other system directories triggered warnings
- **High false positive rate**: Safe operations like `cat /etc/passwd` or `vi /etc/hosts` caused unnecessary interruptions
- **Poor user experience**: Users frequently had to confirm safe, routine operations

### After (New Intelligent System)
- **Context-aware analysis**: Distinguishes between read-only and write operations
- **Reduced false positives**: Safe commands on system files proceed without warnings
- **Maintained security**: Dangerous operations still trigger appropriate warnings
- **Better user experience**: Fewer interruptions for legitimate administrative tasks

## Command Classification

### Safe Read-Only Commands (No Warning)
These commands are allowed to access system directories without warnings:

#### File Viewing and Editing
- `cat`, `less`, `more`, `head`, `tail` - File content viewing
- `view`, `vi`, `vim`, `nano`, `emacs`, `pico` - Text editors
- `grep`, `egrep`, `fgrep` - Text searching
- `awk`, `sed`, `sort`, `uniq`, `cut`, `tr`, `wc` - Text processing

#### File System Operations
- `ls`, `ll`, `dir` - Directory listing
- `find`, `locate` - File searching
- `which`, `whereis` - Command location
- `file`, `stat` - File information
- `du`, `df` - Disk usage

#### System Information
- `ps`, `top`, `htop` - Process information
- `id`, `whoami`, `who`, `w` - User information
- `last`, `lastlog` - Login history
- `date`, `uptime`, `uname`, `hostname` - System status
- `dmesg` - Kernel messages
- `mount`, `lsblk`, `lscpu`, `lsmem` - Hardware information
- `netstat`, `ss`, `ip`, `ifconfig` - Network information

#### Utilities
- `diff`, `cmp` - File comparison
- `md5sum`, `sha1sum`, `sha256sum` - Checksums
- `strings`, `hexdump`, `od`, `xxd` - Binary analysis

### Dangerous Operations (Warning Required)

#### File Modification Commands
- `rm`, `rmdir`, `unlink`, `shred`, `wipe` - File deletion
- `mv`, `cp`, `dd`, `rsync` - File movement/copying
- `chmod`, `chown`, `chgrp`, `chattr`, `setfacl` - Permission changes
- `ln`, `link`, `symlink` - Link creation
- `mkdir`, `touch`, `truncate` - File/directory creation

#### System Administration
- `systemctl`, `service`, `chkconfig` - Service management
- `crontab`, `at`, `batch` - Job scheduling
- `useradd`, `userdel`, `usermod` - User management
- `passwd`, `chpasswd` - Password management
- `mount`, `umount`, `swapon`, `swapoff` - Filesystem mounting

#### Package Management
- `dpkg`, `apt`, `apt-get`, `yum`, `dnf`, `rpm`, `zypper` - Package operations

#### Network Security
- `iptables`, `ip6tables`, `ufw`, `firewall-cmd` - Firewall management

### Special Cases (Always Warning)

#### Output Redirection
Any command with output redirection to system directories triggers warnings:
- `echo "text" > /etc/passwd`
- `cat file >> /etc/hosts`
- `ls /etc > /etc/test`
- `command 2> /etc/error.log`

#### Pipes to Dangerous Commands
Commands piped to dangerous operations trigger warnings:
- `cat /etc/passwd | rm /etc/hosts`
- `ls /etc | chmod 777`
- `find /etc | chown user`

## Implementation Details

### Function Architecture

#### `is_safe_readonly_command(const char *command)`
- Checks if a command is in the safe read-only list
- Extracts command name from full command line
- Handles both basename and absolute path commands
- Returns 1 if safe, 0 if not

#### `is_dangerous_system_operation(const char *command)`
- Checks if a command can modify system state
- Identifies potentially harmful operations
- Returns 1 if dangerous, 0 if not

#### `check_system_directory_access(const char *command)`
- Main intelligent analysis function
- Combines multiple checks for comprehensive analysis
- Priority order:
  1. Check if command accesses system directories
  2. Check for output redirection (always dangerous)
  3. Check for pipes to dangerous commands
  4. Check if command is dangerous operation
  5. Check if command is safe read-only
  6. Default to warning for unknown operations

### Security Considerations

#### Maintained Security
- All genuinely dangerous operations still trigger warnings
- Output redirection to system files is always flagged
- Pipes to dangerous commands are detected
- Unknown commands default to requiring confirmation

#### Reduced False Positives
- Common administrative tasks proceed smoothly
- File viewing and editing operations are uninterrupted
- System information gathering is streamlined
- Legitimate troubleshooting activities are facilitated

## Examples

### Allowed Without Warning
```bash
# File viewing
cat /etc/passwd
less /etc/hosts
vi /etc/fstab
view /etc/sudoers

# System information
ls /etc
find /etc -name "*.conf"
grep error /var/log/syslog
ps aux | grep apache

# Safe editing
nano /etc/hosts
vim /etc/ssh/sshd_config
```

### Requires Warning/Confirmation
```bash
# File modification
rm /etc/passwd
chmod 777 /etc/shadow
chown user /etc/sudoers

# Output redirection
echo "test" > /etc/passwd
cat backup >> /etc/hosts

# Dangerous pipes
cat /etc/passwd | rm /etc/hosts
ls /etc | chmod 777

# System changes
systemctl stop sshd
userdel username
```

## Testing

The intelligent gut check system includes comprehensive tests:

- **Unit tests** for individual function validation
- **Integration tests** for end-to-end behavior
- **Edge case tests** for boundary conditions
- **Regression tests** to ensure existing functionality

Run tests with:
```bash
make tests
./test_intelligent_gutcheck
./demo_intelligent_gutcheck.sh
```

## Benefits

1. **Improved User Experience**: Fewer interruptions for legitimate tasks
2. **Maintained Security**: All dangerous operations still protected
3. **Better Productivity**: Administrative tasks flow more smoothly
4. **Intelligent Analysis**: Context-aware command evaluation
5. **Backward Compatibility**: Existing security policies unchanged
