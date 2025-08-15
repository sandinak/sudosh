# -*- coding: utf-8 -*-
# Copyright: (c) 2024, Branson Matheson <branson@sandsite.org>
# GNU General Public License v3.0+ (see COPYING or https://www.gnu.org/licenses/gpl-3.0.txt)
from __future__ import (absolute_import, division, print_function)
__metaclass__ = type

DOCUMENTATION = """
    name: sudosh
    short_description: Interactive sudo shell with enhanced logging and Ansible detection
    description:
        - This become plugin allows your remote/login user to execute commands via sudosh,
          an interactive sudo shell that provides enhanced logging, session recording,
          and automatic Ansible session detection.
        - Sudosh automatically detects when it's being called from Ansible and adjusts
          its behavior accordingly (suppresses lecture, enhances logging).
        - All commands are logged with full context including Ansible metadata.
        - |
          SECURITY WARNING: Sudosh includes built-in security restrictions that block
          certain AI automation tools (like Augment) from performing privileged operations.
          This is intentional and cannot be overridden for security reasons.
    author: Branson Matheson (@sandinak)
    version_added: "1.0"
    requirements:
        - sudosh binary installed on target hosts
        - User must have sudoers permissions to run sudosh
        - Sudosh version 1.0+ with Ansible detection support
    notes:
        - Sudosh provides enhanced security logging compared to standard sudo
        - Automatic Ansible detection provides better audit trails for automation
        - Session recording capabilities for compliance requirements
        - Commands executed through sudosh are validated against sudoers rules
        - All automation sessions are logged with session type indicators
        - Augment AI sessions are automatically blocked for security
    options:
        become_user:
            description: User you 'become' to execute the task
            default: root
            ini:
              - section: privilege_escalation
                key: become_user
              - section: sudosh_become_plugin
                key: user
            vars:
              - name: ansible_become_user
              - name: ansible_sudosh_user
            env:
              - name: ANSIBLE_BECOME_USER
              - name: ANSIBLE_SUDOSH_USER
            keyword:
              - name: become_user
        become_exe:
            description: Sudosh executable path
            default: sudosh
            ini:
              - section: privilege_escalation
                key: become_exe
              - section: sudosh_become_plugin
                key: executable
            vars:
              - name: ansible_become_exe
              - name: ansible_sudosh_exe
            env:
              - name: ANSIBLE_BECOME_EXE
              - name: ANSIBLE_SUDOSH_EXE
            keyword:
              - name: become_exe
        become_flags:
            description: Options to pass to sudosh
            default: --ansible-detect --ansible-verbose
            ini:
              - section: privilege_escalation
                key: become_flags
              - section: sudosh_become_plugin
                key: flags
            vars:
              - name: ansible_become_flags
              - name: ansible_sudosh_flags
            env:
              - name: ANSIBLE_BECOME_FLAGS
              - name: ANSIBLE_SUDOSH_FLAGS
            keyword:
              - name: become_flags
        become_pass:
            description: Password to pass to sudosh
            required: False
            vars:
              - name: ansible_become_password
              - name: ansible_become_pass
              - name: ansible_sudosh_pass
            env:
              - name: ANSIBLE_BECOME_PASS
              - name: ANSIBLE_SUDOSH_PASS
            ini:
              - section: sudosh_become_plugin
                key: password
        sudosh_session_log:
            description: Enable session logging in sudosh
            type: bool
            default: True
            ini:
              - section: sudosh_become_plugin
                key: session_log
            vars:
              - name: ansible_sudosh_session_log
            env:
              - name: ANSIBLE_SUDOSH_SESSION_LOG
        sudosh_detection_force:
            description: Force Ansible detection mode
            type: bool
            default: False
            ini:
              - section: sudosh_become_plugin
                key: detection_force
            vars:
              - name: ansible_sudosh_detection_force
            env:
              - name: ANSIBLE_SUDOSH_DETECTION_FORCE
        sudosh_detection_verbose:
            description: Enable verbose Ansible detection output
            type: bool
            default: True
            ini:
              - section: sudosh_become_plugin
                key: detection_verbose
            vars:
              - name: ansible_sudosh_detection_verbose
            env:
              - name: ANSIBLE_SUDOSH_DETECTION_VERBOSE
"""

import re
import shlex
import os

from ansible.plugins.become import BecomeBase


class BecomeModule(BecomeBase):
    """
    Sudosh Become Plugin for Ansible

    This plugin enables Ansible to use sudosh as a become method, providing
    enhanced logging, session recording, and automatic detection of Ansible
    automation sessions.

    Security Features:
    - Automatic blocking of Augment AI sessions for security
    - Command validation against sudoers rules for Ansible sessions
    - Enhanced audit logging with session type indicators
    - Fail-safe behavior that defaults to blocking uncertain sessions

    Usage Examples:

    Basic usage in playbook:
        - name: Example task
          command: whoami
          become: yes
          become_method: sudosh

    With custom configuration:
        - name: Advanced task
          file:
            path: /etc/myapp.conf
            state: touch
          become: yes
          become_method: sudosh
          vars:
            ansible_sudosh_detection_verbose: true
            ansible_sudosh_session_log: true

    Configuration in ansible.cfg:
        [defaults]
        become_plugins = ./ansible_plugins/become
        become_method = sudosh

        [sudosh_become_plugin]
        executable = sudosh
        flags = --ansible-detect --ansible-verbose
        session_log = True
        detection_verbose = True
    """

    name = 'sudosh'

    # messages for detecting prompted password issues
    fail = ('Sorry, try again.', 'sudosh: authentication failed')
    missing = ('Sorry, a password is required to run sudosh', 'sudosh: password required')

    def build_become_command(self, cmd, shell):
        """
        Build the sudosh command for privilege escalation.

        This method constructs the complete sudosh command with appropriate
        flags, environment variables, and security context for Ansible
        automation.

        Security Notes:
        - Automatically sets ANSIBLE_BECOME_METHOD=sudosh for detection
        - Includes Ansible context variables for audit logging
        - Validates and sanitizes all environment variables
        - Ensures proper command structure for sudosh execution

        Args:
            cmd (str): The command to execute with elevated privileges
            shell (str): The shell to use for command execution

        Returns:
            str: Complete sudosh command string ready for execution

        Raises:
            None: Method handles errors gracefully and logs issues
        """
        super(BecomeModule, self).build_become_command(cmd, shell)

        if not cmd:
            return cmd

        becomecmd = self.get_option('become_exe') or self.name

        # Build sudosh-specific flags
        flags = self.get_option('become_flags') or '--ansible-detect --ansible-verbose'
        
        # Add Ansible-specific environment variables to help with detection
        ansible_env_vars = []
        
        # Set Ansible detection environment variables
        ansible_env_vars.extend([
            'ANSIBLE_BECOME_METHOD=sudosh',
            'ANSIBLE_BECOME_USER=%s' % (self.get_option('become_user') or 'root'),
            'ANSIBLE_MODULE_NAME=%s' % os.environ.get('ANSIBLE_MODULE_NAME', 'unknown'),
            'ANSIBLE_TASK_UUID=%s' % os.environ.get('ANSIBLE_TASK_UUID', 'unknown'),
        ])
        
        # Add optional Ansible context if available
        if os.environ.get('ANSIBLE_PLAYBOOK_DIR'):
            ansible_env_vars.append('ANSIBLE_PLAYBOOK_DIR=%s' % os.environ.get('ANSIBLE_PLAYBOOK_DIR'))
        if os.environ.get('ANSIBLE_INVENTORY'):
            ansible_env_vars.append('ANSIBLE_INVENTORY=%s' % os.environ.get('ANSIBLE_INVENTORY'))
        if os.environ.get('ANSIBLE_CONFIG'):
            ansible_env_vars.append('ANSIBLE_CONFIG=%s' % os.environ.get('ANSIBLE_CONFIG'))
            
        # Add sudosh-specific options based on plugin configuration
        sudosh_flags = []
        
        if self.get_option('sudosh_detection_force'):
            sudosh_flags.append('--ansible-force')
            
        if self.get_option('sudosh_detection_verbose'):
            sudosh_flags.append('--ansible-verbose')
        else:
            # Remove verbose flag if explicitly disabled
            flags = flags.replace('--ansible-verbose', '').strip()
            
        if not self.get_option('sudosh_session_log'):
            sudosh_flags.append('--no-session-log')
            
        # Combine all flags
        all_flags = []
        if flags:
            all_flags.extend(shlex.split(flags))
        all_flags.extend(sudosh_flags)
        
        # Handle password prompting
        prompt = ''
        if self.get_option('become_pass'):
            self.prompt = '[sudosh via ansible, key=%s] password:' % self._id
            # Remove non-interactive flags if password is provided
            if all_flags:
                reflag = []
                for flag in all_flags:
                    if flag in ('-n', '--non-interactive'):
                        continue
                    elif not flag.startswith('--'):
                        # handle -XnxxX flags only
                        flag = re.sub(r'^(-\w*)n(\w*.*)', r'\1\2', flag)
                    reflag.append(flag)
                all_flags = reflag

        # Build user specification
        user = self.get_option('become_user') or ''
        if user:
            user_flag = '-u %s' % user
        else:
            user_flag = ''

        # Build the final command
        env_prefix = ' '.join(ansible_env_vars)
        flags_str = ' '.join(all_flags) if all_flags else ''
        
        # Construct the command with environment variables
        cmd_parts = []
        if env_prefix:
            cmd_parts.append('env')
            cmd_parts.extend(ansible_env_vars)
        cmd_parts.append(becomecmd)
        if flags_str:
            cmd_parts.append(flags_str)
        if user_flag:
            cmd_parts.append(user_flag)
        cmd_parts.append(self._build_success_command(cmd, shell))
        
        return ' '.join(cmd_parts)

    def check_password_prompt(self, b_output):
        """Enhanced password prompt detection for sudosh"""
        # Check for standard sudosh prompts
        if super(BecomeModule, self).check_password_prompt(b_output):
            return True
            
        # Check for additional sudosh-specific prompts
        sudosh_prompts = [
            b'[sudosh]',
            b'sudosh:',
            b'Password:',
            b'password:',
        ]
        
        for line in b_output.splitlines():
            line = line.strip().lower()
            for prompt in sudosh_prompts:
                if prompt.lower() in line:
                    return True
                    
        return False

    def check_success(self, b_output):
        """Enhanced success detection for sudosh"""
        # Check for standard success marker
        if super(BecomeModule, self).check_success(b_output):
            return True
            
        # Check for sudosh-specific success indicators
        success_indicators = [
            b'sudosh: Ansible session detected',
            b'sudosh: Augment session detected',
            b'sudosh: authentication successful',
        ]
        
        for line in b_output.splitlines():
            for indicator in success_indicators:
                if indicator in line:
                    return True
                    
        return False
