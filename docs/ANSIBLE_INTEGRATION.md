# Sudosh Ansible Integration Guide

This guide provides comprehensive instructions for integrating sudosh with Ansible, including plugin development, configuration, and best practices.

## 🎯 Overview

Sudosh provides intelligent Ansible session detection and specialized handling for automation workflows. This integration ensures that:

- Ansible playbooks can execute privileged commands through sudosh
- All Ansible operations are properly logged with session context
- AI detection is appropriately handled for automation scenarios
- Security policies are maintained during automated operations

## 🔧 Basic Ansible Integration

### Using Sudosh as Become Method

#### Method 1: Direct Command Execution

```yaml
---
- name: Example playbook using sudosh
  hosts: all
  tasks:
    - name: Execute command via sudosh
      command: sudosh systemctl status nginx
      register: nginx_status
    
    - name: Execute command as specific user
      command: sudosh -u www-data ls /var/www
      register: web_files
    
    - name: Execute complex command
      command: sudosh -c "systemctl restart nginx && systemctl status nginx"
      register: nginx_restart
```

#### Method 2: Shell Module Integration

```yaml
---
- name: Advanced sudosh integration
  hosts: all
  tasks:
    - name: Run maintenance script
      shell: |
        sudosh -u postgres pg_dump mydb > /backup/mydb.sql
        sudosh chown postgres:postgres /backup/mydb.sql
      register: backup_result
    
    - name: Conditional execution
      shell: sudosh systemctl restart {{ service_name }}
      when: service_restart_required | default(false)
      vars:
        service_name: nginx
```

### Environment Configuration

Configure Ansible to work optimally with sudosh:

```yaml
# ansible.cfg
[defaults]
host_key_checking = False
timeout = 30
gathering = smart
fact_caching = memory

[ssh_connection]
ssh_args = -o ControlMaster=auto -o ControlPersist=60s
pipelining = True
```

## 🔌 Custom Ansible Plugin Development

### Sudosh Become Plugin

Create `plugins/become/sudosh.py`:

```python
# -*- coding: utf-8 -*-
# Copyright: (c) 2024, Branson Matheson <branson@sandsite.org>
# GNU General Public License v3.0+ (see COPYING or https://www.gnu.org/licenses/gpl-3.0.txt)

from __future__ import (absolute_import, division, print_function)
__metaclass__ = type

DOCUMENTATION = '''
    name: sudosh
    short_description: Run commands via sudosh
    description:
        - This become plugin allows using sudosh as a privilege escalation method
        - Provides enhanced security and logging compared to standard sudo
        - Includes AI detection and comprehensive audit capabilities
    author: Branson Matheson (@sandinak)
    version_added: "1.9.3"
    options:
        become_user:
            description: User to become
            default: root
            ini:
              - section: privilege_escalation
                key: become_user
              - section: sudosh_become_plugin
                key: user
            vars:
              - name: ansible_become_user
              - name: ansible_sudosh_user
        become_flags:
            description: Options to pass to sudosh
            default: ''
            ini:
              - section: privilege_escalation
                key: become_flags
              - section: sudosh_become_plugin
                key: flags
            vars:
              - name: ansible_become_flags
              - name: ansible_sudosh_flags
        become_exe:
            description: Sudosh executable
            default: sudosh
            ini:
              - section: privilege_escalation
                key: become_exe
              - section: sudosh_become_plugin
                key: executable
            vars:
              - name: ansible_become_exe
              - name: ansible_sudosh_exe
    notes:
        - Requires sudosh to be installed on target systems
        - Sudosh must be properly configured with setuid permissions
        - All operations are logged by sudosh for audit purposes
'''

from ansible.plugins.become import BecomeBase
from ansible.module_utils.six.moves import shlex_quote


class BecomeModule(BecomeBase):

    name = 'sudosh'

    def build_become_command(self, cmd, shell):
        super(BecomeModule, self).build_become_command(cmd, shell)

        if not cmd:
            return cmd

        become_exe = self.get_option('become_exe')
        become_flags = self.get_option('become_flags') or ''
        become_user = self.get_option('become_user') or 'root'

        # Build sudosh command
        sudosh_cmd = [become_exe]
        
        # Add flags if specified
        if become_flags:
            sudosh_cmd.extend(become_flags.split())
        
        # Add user specification if not root
        if become_user != 'root':
            sudosh_cmd.extend(['-u', become_user])
        
        # Add the actual command
        if shell:
            sudosh_cmd.extend(['-c', cmd])
        else:
            sudosh_cmd.append(cmd)

        return ' '.join(shlex_quote(arg) for arg in sudosh_cmd)
```

### Sudosh Action Plugin

Create `plugins/action/sudosh.py`:

```python
# -*- coding: utf-8 -*-
# Copyright: (c) 2024, Branson Matheson <branson@sandsite.org>
# GNU General Public License v3.0+ (see COPYING or https://www.gnu.org/licenses/gpl-3.0.txt)

from __future__ import (absolute_import, division, print_function)
__metaclass__ = type

from ansible.plugins.action import ActionBase
from ansible.utils.display import Display

display = Display()


class ActionModule(ActionBase):
    """
    Action plugin for sudosh command execution
    """

    def run(self, tmp=None, task_vars=None):
        if task_vars is None:
            task_vars = dict()

        result = super(ActionModule, self).run(tmp, task_vars)
        del tmp  # tmp no longer has any effect

        # Get module arguments
        module_args = self._task.args.copy()
        
        # Extract sudosh-specific options
        command = module_args.get('command', '')
        user = module_args.get('user', 'root')
        flags = module_args.get('flags', '')
        log_session = module_args.get('log_session', False)
        
        if not command:
            result['failed'] = True
            result['msg'] = 'command parameter is required'
            return result

        # Build sudosh command
        sudosh_cmd = ['sudosh']
        
        if flags:
            sudosh_cmd.extend(flags.split())
        
        if user != 'root':
            sudosh_cmd.extend(['-u', user])
        
        if log_session:
            log_file = f"/var/log/sudosh/ansible-{task_vars.get('inventory_hostname', 'unknown')}.log"
            sudosh_cmd.extend(['-L', log_file])
        
        # Add command
        sudosh_cmd.extend(['-c', command])
        
        # Execute via command module
        command_args = {
            '_raw_params': ' '.join(sudosh_cmd),
            '_uses_shell': True
        }
        
        result.update(self._execute_module(
            module_name='command',
            module_args=command_args,
            task_vars=task_vars
        ))
        
        return result
```

### Plugin Installation

```bash
# Create plugin directory structure
mkdir -p ~/.ansible/plugins/become
mkdir -p ~/.ansible/plugins/action

# Copy plugins
cp plugins/become/sudosh.py ~/.ansible/plugins/become/
cp plugins/action/sudosh.py ~/.ansible/plugins/action/

# Or install system-wide
sudo mkdir -p /usr/share/ansible/plugins/become
sudo mkdir -p /usr/share/ansible/plugins/action
sudo cp plugins/become/sudosh.py /usr/share/ansible/plugins/become/
sudo cp plugins/action/sudosh.py /usr/share/ansible/plugins/action/
```

## 📋 Playbook Examples

### Basic Usage Examples

```yaml
---
- name: Sudosh integration examples
  hosts: all
  become: yes
  become_method: sudosh
  
  tasks:
    - name: Install package using sudosh
      package:
        name: nginx
        state: present
    
    - name: Start service using sudosh
      service:
        name: nginx
        state: started
        enabled: yes
    
    - name: Execute custom command
      sudosh:
        command: "systemctl status nginx"
        user: root
        log_session: true
      register: nginx_status
    
    - name: Display result
      debug:
        var: nginx_status.stdout_lines
```

### Advanced Configuration Management

```yaml
---
- name: Advanced sudosh configuration
  hosts: webservers
  vars:
    sudosh_log_dir: "/var/log/sudosh"
    
  tasks:
    - name: Create sudosh log directory
      file:
        path: "{{ sudosh_log_dir }}"
        state: directory
        mode: '0755'
        owner: root
        group: root
    
    - name: Configure web server as www-data user
      sudosh:
        command: |
          mkdir -p /var/www/html/app
          chown -R www-data:www-data /var/www/html/app
          chmod -R 755 /var/www/html/app
        user: root
        flags: "-v"
        log_session: true
      register: web_config
    
    - name: Deploy application files
      sudosh:
        command: "rsync -av /tmp/app/ /var/www/html/app/"
        user: www-data
        log_session: true
      register: app_deploy
    
    - name: Restart web services
      sudosh:
        command: "systemctl restart nginx php-fpm"
        user: root
        flags: "-v -L {{ sudosh_log_dir }}/deployment.log"
      when: app_deploy.changed
```

### Security-Focused Playbooks

```yaml
---
- name: Security hardening with sudosh
  hosts: all
  vars:
    security_audit_log: "/var/log/sudosh/security-audit.log"
    
  tasks:
    - name: Audit system permissions
      sudosh:
        command: |
          find /etc -type f -perm /o+w -exec ls -la {} \;
          find /var -type f -perm /o+w -exec ls -la {} \;
        user: root
        flags: "-L {{ security_audit_log }}"
      register: permission_audit
    
    - name: Check for SUID files
      sudosh:
        command: "find / -type f -perm /4000 -exec ls -la {} \;"
        user: root
        log_session: true
      register: suid_files
    
    - name: Generate security report
      template:
        src: security_report.j2
        dest: "/tmp/security_report_{{ ansible_date_time.epoch }}.txt"
      vars:
        permissions: "{{ permission_audit.stdout_lines }}"
        suid_files: "{{ suid_files.stdout_lines }}"
```

## 🔍 Monitoring and Logging

### Ansible-Specific Logging

Configure enhanced logging for Ansible operations:

```yaml
---
- name: Configure sudosh for Ansible
  hosts: all
  tasks:
    - name: Create Ansible-specific log directory
      file:
        path: /var/log/sudosh/ansible
        state: directory
        mode: '0755'
        owner: root
        group: root
    
    - name: Configure logrotate for Ansible logs
      copy:
        content: |
          /var/log/sudosh/ansible/*.log {
              daily
              rotate 30
              compress
              delaycompress
              missingok
              notifempty
              create 644 root root
          }
        dest: /etc/logrotate.d/sudosh-ansible
        mode: '0644'
```

### Log Analysis

Create log analysis script `scripts/analyze-ansible-logs.sh`:

```bash
#!/bin/bash
# Analyze sudosh Ansible logs

LOG_DIR="/var/log/sudosh/ansible"
REPORT_FILE="/tmp/ansible-sudosh-report.txt"

echo "Sudosh Ansible Integration Report" > $REPORT_FILE
echo "Generated: $(date)" >> $REPORT_FILE
echo "=================================" >> $REPORT_FILE

# Count operations by host
echo -e "\nOperations by Host:" >> $REPORT_FILE
grep "ANSIBLE_SESSION" /var/log/syslog | \
    awk '{print $4}' | sort | uniq -c | sort -nr >> $REPORT_FILE

# Count operations by user
echo -e "\nOperations by User:" >> $REPORT_FILE
grep "ANSIBLE_SESSION" /var/log/syslog | \
    grep -o "user=[^ ]*" | sort | uniq -c | sort -nr >> $REPORT_FILE

# Recent errors
echo -e "\nRecent Errors:" >> $REPORT_FILE
grep -i "error\|failed" $LOG_DIR/*.log | tail -10 >> $REPORT_FILE

echo "Report generated: $REPORT_FILE"
```

## 🧪 Testing Ansible Integration

### Test Playbook

Create `tests/ansible-integration-test.yml`:

```yaml
---
- name: Sudosh Ansible Integration Test
  hosts: localhost
  connection: local
  gather_facts: no

  tasks:
    - name: Test basic sudosh functionality
      command: sudosh echo "Ansible integration test"
      environment:
        SUDOSH_TEST_MODE: "1"
      register: basic_test

    - name: Test user specification
      command: sudosh -u {{ ansible_user }} whoami
      environment:
        SUDOSH_TEST_MODE: "1"
      register: user_test

    - name: Test command mode
      command: sudosh -c "echo 'Command mode test'"
      environment:
        SUDOSH_TEST_MODE: "1"
      register: command_test

    - name: Verify all tests passed
      assert:
        that:
          - basic_test.rc == 0
          - user_test.rc == 0
          - command_test.rc == 0
        fail_msg: "Sudosh Ansible integration tests failed"
        success_msg: "All Sudosh Ansible integration tests passed"
```

### Running Integration Tests

```bash
# Set test mode
export SUDOSH_TEST_MODE=1

# Run test playbook
ansible-playbook tests/ansible-integration-test.yml

# Test with inventory
ansible-playbook -i inventory tests/ansible-integration-test.yml
```

## 📚 Best Practices

### Security Considerations

1. **Always use test mode for CI/CD**: Set `SUDOSH_TEST_MODE=1` in automated environments
2. **Monitor logs**: Regularly review sudosh logs for Ansible operations
3. **Limit privileges**: Use specific users with `-u` option when possible
4. **Session logging**: Enable session logging for critical operations

### Performance Optimization

1. **Connection reuse**: Configure SSH connection multiplexing
2. **Pipelining**: Enable SSH pipelining for better performance
3. **Fact caching**: Use fact caching to reduce overhead
4. **Parallel execution**: Use appropriate parallelism settings

### Troubleshooting

Common issues and solutions:

1. **Permission denied**: Ensure sudosh has setuid bit set
2. **Command not found**: Verify sudosh is in PATH on target systems
3. **Authentication failures**: Check sudoers configuration
4. **Log file issues**: Ensure log directories exist and are writable

This Ansible integration guide provides a comprehensive foundation for using sudosh in automated environments while maintaining security and audit capabilities.
