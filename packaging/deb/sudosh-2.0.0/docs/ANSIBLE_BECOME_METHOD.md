# Sudosh Ansible Become Method Plugin

This document describes how to use sudosh as an Ansible become method, providing enhanced logging, session recording, and automatic Ansible detection for your automation workflows.

## Overview

The sudosh become method plugin allows Ansible to use sudosh instead of standard sudo for privilege escalation. This provides several advantages:

- **Enhanced Logging**: All commands are logged with full Ansible context
- **Automatic Detection**: Sudosh automatically detects Ansible sessions and adjusts behavior
- **Session Recording**: Optional session recording for compliance requirements
- **Audit Trails**: Detailed audit trails linking commands to specific Ansible tasks
- **Security**: All the security benefits of sudosh with Ansible integration

## Installation

### 1. Install Sudosh

First, ensure sudosh is installed and configured on your target hosts:

```bash
# Build and install sudosh
make && sudo make install

# Verify installation
which sudosh
sudosh --version
```

### 2. Install the Become Plugin

Copy the sudosh become plugin to your Ansible plugins directory:

```bash
# Option 1: Project-specific installation
mkdir -p ansible_plugins/become
cp ansible_plugins/become/sudosh.py ansible_plugins/become/

# Option 2: System-wide installation (requires admin privileges)
sudo cp ansible_plugins/become/sudosh.py /usr/share/ansible/plugins/become/
```

### 3. Configure Ansible

Update your `ansible.cfg` to use the sudosh become method:

```ini
[defaults]
# Enable sudosh become plugin
become_plugins = ./ansible_plugins/become

# Default become method
become = True
become_method = sudosh
become_user = root

[sudosh_become_plugin]
executable = sudosh
flags = --ansible-detect --ansible-verbose
session_log = True
detection_verbose = True
detection_force = False

[privilege_escalation]
become = True
become_method = sudosh
become_user = root
become_ask_pass = False
```

## Configuration Options

### Plugin-Specific Options

| Option | Default | Description |
|--------|---------|-------------|
| `become_exe` | `sudosh` | Path to sudosh executable |
| `become_flags` | `--ansible-detect --ansible-verbose` | Default flags passed to sudosh |
| `sudosh_session_log` | `True` | Enable session logging |
| `sudosh_detection_force` | `False` | Force Ansible detection mode |
| `sudosh_detection_verbose` | `True` | Enable verbose detection output |

### Environment Variables

The plugin automatically sets these environment variables for detection:

- `ANSIBLE_BECOME_METHOD=sudosh`
- `ANSIBLE_BECOME_USER=<target_user>`
- `ANSIBLE_MODULE_NAME=<module_name>`
- `ANSIBLE_TASK_UUID=<task_id>`
- `ANSIBLE_PLAYBOOK_DIR=<playbook_directory>`

## Usage Examples

### Basic Usage

```yaml
---
- name: Example playbook using sudosh
  hosts: all
  become: yes
  become_method: sudosh
  tasks:
    - name: Install package
      package:
        name: curl
        state: present
        
    - name: Create file
      file:
        path: /etc/myapp.conf
        state: touch
        owner: root
        group: root
        mode: '0644'
```

### Per-Task Configuration

```yaml
---
- name: Mixed become methods
  hosts: all
  tasks:
    - name: Standard sudo task
      command: whoami
      become: yes
      become_method: sudo
      
    - name: Sudosh task with enhanced logging
      command: whoami
      become: yes
      become_method: sudosh
      vars:
        ansible_become_plugins: ./ansible_plugins/become
        ansible_sudosh_detection_verbose: true
```

### Advanced Configuration

```yaml
---
- name: Advanced sudosh configuration
  hosts: all
  vars:
    ansible_become_plugins: ./ansible_plugins/become
    ansible_sudosh_session_log: true
    ansible_sudosh_detection_force: false
  tasks:
    - name: File operations with full logging
      file:
        path: /var/log/myapp
        state: directory
        owner: myapp
        group: myapp
        mode: '0755'
      become: yes
      become_method: sudosh
      environment:
        ANSIBLE_TASK_DESCRIPTION: "Create application log directory"
```

## Detection Features

### Automatic Detection

Sudosh automatically detects when it's being called from Ansible and:

- Suppresses the sudo lecture to avoid cluttering automation logs
- Adds Ansible context to all log entries
- Records the automation session with metadata
- Provides enhanced audit trails

### Detection Methods

1. **Environment Variables**: Detects `ANSIBLE_*` environment variables
2. **Become Method**: Recognizes `ANSIBLE_BECOME_METHOD=sudosh`
3. **Parent Process**: Analyzes the process tree for Ansible processes
4. **Execution Context**: Examines SSH connections and terminal types

### Logging Output

When using sudosh as a become method, you'll see enhanced log entries:

```
Dec 15 10:30:15 hostname sudosh: user : ANSIBLE_SESSION_START: become_method=sudosh module=command task_uuid=abc123
Dec 15 10:30:16 hostname sudosh: user : COMMAND=/bin/whoami ; ANSIBLE_CONTEXT: method=become_method confidence=95%
```

## Testing

### Unit Tests

Run the detection unit tests:

```bash
make tests
./bin/test_ansible_detection
```

### Integration Tests

Test the become method with a real playbook:

```bash
# Test basic functionality
ansible-playbook tests/ansible_sudosh_become_test.yml -i tests/inventory.ini

# Test with verbose output
ansible-playbook tests/ansible_sudosh_become_test.yml -i tests/inventory.ini -vvv
```

### Manual Testing

Test the detection manually:

```bash
# Test become method detection
ANSIBLE_BECOME_METHOD=sudosh ANSIBLE_MODULE_NAME=command ./bin/sudosh --ansible-verbose -l

# Test with full Ansible environment
env ANSIBLE_BECOME_METHOD=sudosh \
    ANSIBLE_BECOME_USER=root \
    ANSIBLE_MODULE_NAME=command \
    ANSIBLE_TASK_UUID=test-123 \
    ./bin/sudosh --ansible-verbose -l
```

## Troubleshooting

### Common Issues

1. **Plugin Not Found**
   ```
   ERROR! Invalid become method specified: sudosh
   ```
   - Ensure the plugin is in the correct directory
   - Check `become_plugins` path in `ansible.cfg`

2. **Permission Denied**
   ```
   sudosh: authentication failed
   ```
   - Verify user has sudoers permissions for sudosh
   - Check sudosh configuration and permissions

3. **Detection Not Working**
   ```
   No Ansible session detected
   ```
   - Verify environment variables are set correctly
   - Check sudosh logs for detection details
   - Use `--ansible-force` to force detection

### Debug Mode

Enable debug mode for troubleshooting:

```bash
# Run with maximum verbosity
ansible-playbook playbook.yml -vvvv

# Check sudosh logs
journalctl | grep sudosh

# Test detection manually
ANSIBLE_BECOME_METHOD=sudosh ./bin/sudosh --ansible-verbose --help
```

## Security Considerations

- Sudosh maintains all security features when used as a become method
- All commands are logged and audited
- Session recording provides additional compliance capabilities
- Detection doesn't affect the security model
- Environment variables are validated and sanitized

## Performance Impact

- Minimal overhead compared to standard sudo
- Detection adds ~1-2ms to command execution
- Logging is asynchronous and doesn't block execution
- Session recording has configurable impact based on settings

## Compatibility

- **Ansible Versions**: 2.8+
- **Python Versions**: 3.6+
- **Operating Systems**: Linux, macOS, BSD
- **Sudosh Versions**: 1.0+

## Support

For issues and questions:

- Check the troubleshooting section above
- Review sudosh logs: `journalctl | grep sudosh`
- Run unit tests: `./bin/test_ansible_detection`
- Test manually with verbose output: `--ansible-verbose`

## Contributing

To contribute improvements to the become plugin:

1. Test your changes with the provided test suite
2. Ensure compatibility with multiple Ansible versions
3. Update documentation as needed
4. Submit pull requests with detailed descriptions
