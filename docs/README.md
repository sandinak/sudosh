# sudosh - Interactive Sudo Shell

A secure, interactive sudo shell written in C that provides sudo-like functionality with comprehensive command logging to syslog.

## Features

- **Interactive Shell**: Provides a command prompt for executing privileged commands
- **PAM Authentication**: Uses PAM (Pluggable Authentication Modules) for secure user authentication
- **Comprehensive Logging**: All commands and authentication attempts are logged to syslog
- **Security Features**: 
  - Environment sanitization
  - Command validation
  - Signal handling
  - Privilege management
- **Sudo Compatibility**: Checks user permissions via wheel/sudo group membership
- **Built-in Commands**: Help, exit, and quit commands

## Requirements

- Linux/Unix system
- GCC compiler
- PAM development libraries
- Root privileges for installation (setuid bit)

### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential libpam0g-dev
```

**CentOS/RHEL/Fedora:**
```bash
sudo yum install gcc pam-devel
# or for newer versions:
sudo dnf install gcc pam-devel
```

## Building

```bash
make
```

For debug build:
```bash
make debug
```

## Installation

```bash
sudo make install
```

This installs sudosh to `/usr/local/bin/sudosh` with setuid root permissions.

## Usage

```bash
sudosh
```

Once started, sudosh will:
1. Check if the user has sudo privileges using enhanced authentication:
   - Parse `/etc/nsswitch.conf` to determine authentication sources
   - Parse `/etc/sudoers` file for direct privilege rules
   - Check SSSD integration if available
   - Fall back to group membership (wheel/sudo/admin groups)
2. Prompt for password authentication via PAM
3. Provide an interactive shell prompt: `sudosh# `

### Available Commands

- `help` - Show help message
- `exit` or `quit` - Exit sudosh
- Any other command - Execute as root with logging

### Examples

```bash
sudosh# ls -la /root
sudosh# systemctl restart nginx
sudosh# apt update && apt upgrade
sudosh# help
sudosh# exit
```

## Enhanced Authentication

sudosh uses the same heuristics as sudo for discovering users and permissions, providing enterprise-grade authentication support:

### Real UID Detection

sudosh correctly identifies the real user who invoked the command, even when running with suid root privileges:

- **Cross-platform UID detection**: Uses `getresuid()` on Linux, fallback methods on macOS
- **Real vs Effective UID**: Distinguishes between the invoking user and the effective root user
- **Same as sudo**: Uses identical user identification logic as the sudo command

### Direct Sudoers Reading

sudosh reads sudoers files directly instead of calling external commands:

- **Privilege escalation**: Temporarily escalates to root to read `/etc/sudoers`
- **Direct file parsing**: No dependency on `sudo -l` command execution
- **Efficient processing**: Faster than spawning external processes
- **Complete access**: Reads main sudoers file and all included directories

### NSS Integration

sudosh reads `/etc/nsswitch.conf` to determine the order of authentication sources:

```bash
# Example /etc/nsswitch.conf
passwd: files sssd
sudoers: sssd files
```

This allows sudosh to work with:
- **files**: Traditional `/etc/passwd` and `/etc/sudoers`
- **sssd**: System Security Services Daemon for LDAP/AD integration
- **ldap**: Direct LDAP authentication

### Complete Sudoers Parser

sudosh includes a full sudoers parser that understands:
- User and group specifications (`user`, `%group`)
- Host restrictions (`ALL`, `hostname`)
- Command specifications (`ALL`, `/path/to/command`)
- Run-as user specifications (`(ALL)`, `(root)`)
- NOPASSWD directives with proper precedence
- `#includedir` directives for additional sudoers files
- Comments and Defaults lines (properly ignored)

Example sudoers rules supported:
```bash
root ALL = (ALL) ALL
%admin ALL = (ALL) ALL
%wheel ALL = (ALL) NOPASSWD: ALL
user1 localhost = /bin/ls, /bin/cat
#includedir /etc/sudoers.d
```

### Include Directory Support

sudosh fully supports `#includedir` directives:
- **Automatic directory scanning**: Processes all valid files in included directories
- **File filtering**: Skips backup files (`.bak`, `~`) and invalid files
- **Rule merging**: Combines rules from main file and included files
- **NOPASSWD precedence**: Any matching rule with NOPASSWD grants password-free access

### SSSD Integration

When SSSD is available, sudosh can:
- Query SSSD for user information
- Check SSSD sudo rules via `getent sudoers`
- Fall back to LDAP searches for sudo rules
- Integrate with Active Directory and other enterprise directories

### Multi-layer Authentication

sudosh provides multiple authentication layers with intelligent fallbacks:
1. **Primary**: Direct sudoers file parsing (with privilege escalation)
2. **Secondary**: NSS-configured sources (SSSD, files, etc.)
3. **Tertiary**: External `sudo -l` command execution
4. **Final**: Group membership checking (wheel, sudo, admin)

This ensures maximum compatibility across different system configurations while maintaining efficiency and security.

## Logging

All sudosh activity is logged to syslog with facility `LOG_AUTHPRIV`. Logs include:

- Authentication attempts (success/failure)
- Session start/end
- Command executions
- Security violations
- Errors

### Viewing Logs

```bash
# View sudosh logs
sudo journalctl -t sudosh

# View auth logs (includes sudosh)
sudo tail -f /var/log/auth.log

# On systems with rsyslog
sudo tail -f /var/log/secure
```

## Security Features

### Environment Sanitization
- Removes dangerous environment variables (LD_PRELOAD, etc.)
- Sets secure PATH
- Sets appropriate umask

### Command Validation
- Checks for null bytes (injection attempts)
- Validates command length
- Detects path traversal attempts

### Privilege Management
- Requires setuid root or running as root
- Proper privilege escalation for command execution
- Signal handling for clean shutdown

### Authentication
- PAM-based authentication
- Group membership validation (wheel/sudo)
- Failed authentication logging

## File Structure

```
sudosh/
├── main.c          # Main program and command loop
├── auth.c          # PAM authentication functions
├── command.c       # Command parsing and execution
├── logging.c       # Syslog integration
├── security.c      # Security measures and validation
├── utils.c         # Utility functions
├── sudosh.h        # Header file with declarations
├── Makefile        # Build configuration
└── README.md       # This file
```

## Configuration

sudosh uses the system's PAM configuration. It attempts to use the "sudo" PAM service, which typically inherits from the system authentication configuration.

## Troubleshooting

### Permission Denied
Ensure sudosh is installed with setuid root:
```bash
ls -l /usr/local/bin/sudosh
# Should show: -rwsr-xr-x ... root root ... sudosh
```

### Authentication Issues
Check PAM configuration:
```bash
# Verify PAM sudo service exists
ls /etc/pam.d/sudo
```

### User Not in Sudoers
Add user to wheel or sudo group:
```bash
# Add user to sudo group (Debian/Ubuntu)
sudo usermod -aG sudo username

# Add user to wheel group (CentOS/RHEL)
sudo usermod -aG wheel username
```

## Uninstallation

```bash
sudo make uninstall
```

## Development

### Building for Development
```bash
make debug
```

### Cleaning Build Files
```bash
make clean
```

## License

This project is provided as-is for educational and security purposes. Use responsibly and in accordance with your organization's security policies.

## Security Considerations

- sudosh should only be used in trusted environments
- Regular security audits are recommended
- Monitor logs for suspicious activity
- Keep the system and dependencies updated

## Contributing

When contributing to this project:
1. Follow secure coding practices
2. Test thoroughly in isolated environments
3. Document security implications of changes
4. Ensure proper error handling and logging
