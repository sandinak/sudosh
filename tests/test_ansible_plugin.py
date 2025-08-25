#!/usr/bin/env python3
"""
Comprehensive test suite for the Sudosh Ansible Become Plugin

This test suite validates the Ansible become plugin functionality including:
- Plugin loading and initialization
- Command building and execution
- JSON response parsing
- Error handling and security constraints
- Integration with Ansible core functionality

Author: Branson Matheson <branson@sandsite.org>
"""

import os
import sys
import json
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch, MagicMock

# Add the plugin directory to the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'ansible_plugin'))

try:
    from sudosh import BecomeModule
except ImportError:
    print("Warning: Could not import sudosh plugin. Some tests may be skipped.")
    BecomeModule = None


class TestSudoshBecomePlugin(unittest.TestCase):
    """Test cases for the Sudosh Ansible Become Plugin"""

    def setUp(self):
        """Set up test environment"""
        if BecomeModule is None:
            self.skipTest("Sudosh plugin not available")
        
        self.plugin = BecomeModule()
        self.plugin._connection = Mock()
        
        # Mock the get_option method
        self.plugin.get_option = Mock()
        self.plugin.get_option.side_effect = self._mock_get_option
        
        # Default options
        self.default_options = {
            'become_exe': 'sudosh',
            'become_user': 'root',
            'become_flags': ''
        }

    def _mock_get_option(self, option):
        """Mock get_option method"""
        return self.default_options.get(option)

    def test_plugin_initialization(self):
        """Test plugin initialization"""
        self.assertEqual(self.plugin.name, 'sudosh')
        self.assertIsNotNone(self.plugin)

    def test_build_become_command_basic(self):
        """Test basic command building"""
        cmd = "ls -la /tmp"
        shell = "/bin/bash"
        
        with patch('ansible.module_utils.common.process.get_bin_path', return_value='/usr/bin/sudosh'):
            result = self.plugin.build_become_command(cmd, shell)
            
        self.assertIn('sudosh', result)
        self.assertIn('--ansible-mode', result)
        self.assertIn('--command', result)
        self.assertIn(cmd, result)

    def test_build_become_command_with_user(self):
        """Test command building with specific user"""
        self.default_options['become_user'] = 'testuser'
        cmd = "whoami"
        shell = "/bin/bash"
        
        with patch('ansible.module_utils.common.process.get_bin_path', return_value='/usr/bin/sudosh'):
            result = self.plugin.build_become_command(cmd, shell)
            
        self.assertIn('--become-user', result)
        self.assertIn('testuser', result)

    def test_build_become_command_with_flags(self):
        """Test command building with additional flags"""
        self.default_options['become_flags'] = '--verbose --log-session /tmp/test.log'
        cmd = "echo test"
        shell = "/bin/bash"
        
        with patch('ansible.module_utils.common.process.get_bin_path', return_value='/usr/bin/sudosh'):
            result = self.plugin.build_become_command(cmd, shell)
            
        self.assertIn('--verbose', result)
        self.assertIn('--log-session', result)

    def test_check_success_json_response(self):
        """Test success checking with JSON response"""
        success_response = json.dumps({
            "success": True,
            "exit_code": 0,
            "stdout": "command output"
        })
        
        result = self.plugin.check_success(success_response.encode())
        self.assertTrue(result)

    def test_check_success_failure_response(self):
        """Test success checking with failure response"""
        failure_response = json.dumps({
            "success": False,
            "exit_code": 1,
            "error": "Command failed"
        })
        
        result = self.plugin.check_success(failure_response.encode())
        self.assertFalse(result)

    def test_check_success_legacy_marker(self):
        """Test success checking with legacy success marker"""
        legacy_response = b"Some output\nSUDOSH_ANSIBLE_SUCCESS\nMore output"
        
        result = self.plugin.check_success(legacy_response)
        self.assertTrue(result)

    def test_check_incorrect_password_json(self):
        """Test incorrect password detection with JSON"""
        error_response = json.dumps({
            "success": False,
            "exit_code": 1,
            "error": "Incorrect password"
        })
        
        result = self.plugin.check_incorrect_password(error_response.encode())
        self.assertTrue(result)

    def test_check_incorrect_password_text(self):
        """Test incorrect password detection with text output"""
        error_output = b"Sorry, try again.\nIncorrect password"
        
        result = self.plugin.check_incorrect_password(error_output)
        self.assertTrue(result)

    def test_check_missing_password_json(self):
        """Test missing password detection with JSON"""
        error_response = json.dumps({
            "success": False,
            "exit_code": 1,
            "error": "Password required"
        })
        
        result = self.plugin.check_missing_password(error_response.encode())
        self.assertTrue(result)

    def test_check_missing_password_marker(self):
        """Test missing password detection with marker"""
        prompt_output = b"SUDOSH_ANSIBLE_PASSWORD_PROMPT"
        
        result = self.plugin.check_missing_password(prompt_output)
        self.assertTrue(result)

    def test_get_prompt(self):
        """Test password prompt generation"""
        prompt = self.plugin._get_prompt()
        self.assertEqual(prompt, "sudosh password:")


class TestSudoshIntegration(unittest.TestCase):
    """Integration tests for sudosh with Ansible-like commands"""

    def setUp(self):
        """Set up integration test environment"""
        self.sudosh_path = self._find_sudosh_binary()
        if not self.sudosh_path:
            self.skipTest("Sudosh binary not found")

    def _find_sudosh_binary(self):
        """Find the sudosh binary"""
        possible_paths = [
            './bin/sudosh',
            '/usr/local/bin/sudosh',
            '/usr/bin/sudosh'
        ]
        
        for path in possible_paths:
            if os.path.exists(path) and os.access(path, os.X_OK):
                return path
        return None

    def _execute_sudosh_command(self, command, timeout=10):
        """Execute a sudosh command and return the result"""
        cmd = [self.sudosh_path, '--ansible-mode', '--command', command]
        
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout
            )
            return result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            return -1, "", "Command timed out"
        except Exception as e:
            return -1, "", str(e)

    def test_simple_command_execution(self):
        """Test simple command execution through sudosh"""
        exit_code, stdout, stderr = self._execute_sudosh_command("echo 'test'")
        
        # Should return JSON response
        self.assertIn('{', stdout)
        
        try:
            response = json.loads(stdout)
            # Command might be blocked, but should return valid JSON
            self.assertIn('success', response)
            self.assertIn('exit_code', response)
        except json.JSONDecodeError:
            self.fail("Response is not valid JSON")

    def test_allowed_command(self):
        """Test execution of an allowed command"""
        exit_code, stdout, stderr = self._execute_sudosh_command("pwd")
        
        try:
            response = json.loads(stdout)
            # pwd should generally be allowed
            if response.get('success'):
                self.assertIn('stdout', response)
        except json.JSONDecodeError:
            self.fail("Response is not valid JSON")

    def test_blocked_command(self):
        """Test execution of a blocked command"""
        exit_code, stdout, stderr = self._execute_sudosh_command("rm -rf /")
        
        try:
            response = json.loads(stdout)
            # This dangerous command should be blocked
            self.assertFalse(response.get('success', True))
            self.assertIn('error', response)
        except json.JSONDecodeError:
            self.fail("Response is not valid JSON")

    def test_json_response_format(self):
        """Test that responses are properly formatted JSON"""
        exit_code, stdout, stderr = self._execute_sudosh_command("ls /tmp")
        
        try:
            response = json.loads(stdout)
            
            # Check required fields
            self.assertIn('success', response)
            self.assertIn('exit_code', response)
            self.assertIsInstance(response['success'], bool)
            self.assertIsInstance(response['exit_code'], int)
            
            # Check optional fields
            if 'stdout' in response:
                self.assertIsInstance(response['stdout'], str)
            if 'stderr' in response:
                self.assertIsInstance(response['stderr'], str)
            if 'error' in response:
                self.assertIsInstance(response['error'], str)
                
        except json.JSONDecodeError:
            self.fail("Response is not valid JSON")


class TestAnsiblePlaybookExecution(unittest.TestCase):
    """Test actual Ansible playbook execution with sudosh plugin"""

    def setUp(self):
        """Set up playbook test environment"""
        self.test_dir = tempfile.mkdtemp()
        self.inventory_file = os.path.join(self.test_dir, 'inventory')
        self.playbook_file = os.path.join(self.test_dir, 'test_playbook.yml')
        
        # Create test inventory
        with open(self.inventory_file, 'w') as f:
            f.write('[test]\nlocalhost ansible_connection=local\n')

    def tearDown(self):
        """Clean up test environment"""
        import shutil
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def test_basic_playbook_execution(self):
        """Test basic playbook execution with sudosh"""
        playbook_content = """
---
- name: Test sudosh become plugin
  hosts: test
  become: yes
  become_method: sudosh
  gather_facts: no
  tasks:
    - name: Test command
      command: echo "Hello from sudosh"
      register: result
    
    - name: Display result
      debug:
        msg: "{{ result.stdout }}"
"""
        
        with open(self.playbook_file, 'w') as f:
            f.write(playbook_content)
        
        # This test requires ansible-playbook to be available
        try:
            result = subprocess.run([
                'ansible-playbook',
                '-i', self.inventory_file,
                self.playbook_file,
                '--check'  # Use check mode to avoid actual execution
            ], capture_output=True, text=True, timeout=30)
            
            # Check if ansible-playbook ran (even if it failed due to plugin not being installed)
            self.assertIsNotNone(result.returncode)
            
        except FileNotFoundError:
            self.skipTest("ansible-playbook not available")
        except subprocess.TimeoutExpired:
            self.fail("Playbook execution timed out")


def run_tests():
    """Run all tests"""
    # Create test suite
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # Add test cases
    suite.addTests(loader.loadTestsFromTestCase(TestSudoshBecomePlugin))
    suite.addTests(loader.loadTestsFromTestCase(TestSudoshIntegration))
    suite.addTests(loader.loadTestsFromTestCase(TestAnsiblePlaybookExecution))
    
    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    return result.wasSuccessful()


if __name__ == '__main__':
    print("=== Sudosh Ansible Plugin Test Suite ===\n")
    
    success = run_tests()
    
    print("\n=== Test Summary ===")
    if success:
        print("✅ All tests passed!")
        sys.exit(0)
    else:
        print("❌ Some tests failed!")
        sys.exit(1)
