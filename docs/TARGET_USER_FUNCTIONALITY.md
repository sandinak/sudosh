# Sudosh Target User Functionality

## Overview

Sudosh now supports running commands as specific target users using the `-u {user}` command line option. This functionality provides secure, audited access to run commands with different user privileges while maintaining all existing security protections and sudoers permission validation.

## 🎯 **Usage**

### **Basic Syntax**
```bash
sudosh -u {targetuser}
```

### **Examples**
```bash
# Run commands as root
sudosh -u root

# Run commands as web server user
sudosh -u www-data

# Run commands as database user
sudosh -u postgres

# Run commands as application user
sudosh -u appuser
```

## 🔐 **Security Model**

### **Permission Validation Process**
1. **User Authentication** - Calling user must authenticate with sudosh
2. **Target User Validation** - Target user must exist in the system
3. **Sudoers Permission Check** - Calling user must have permission to run as target user
4. **Command Execution** - Commands run with target user privileges

### **Sudoers Integration**
The target user functionality fully integrates with sudoers configuration:

#### **Required Sudoers Entries**
```bash
# Allow user to run as any user
username ALL=(ALL) ALL

# Allow user to run as specific users
username ALL=(root,www-data,postgres) ALL

# Allow group members to run as any user
%admin ALL=(ALL) ALL

# Allow specific commands as specific user
username ALL=(www-data) /usr/bin/systemctl restart apache2
```

#### **Permission Checking**
- Parses `sudo -l` output to validate runas permissions
- Supports `(ALL)` and `(ALL : ALL)` specifications
- Handles comma-separated user lists: `(user1,user2,user3)`
- Validates specific user permissions: `(targetuser)`
- Maintains compatibility with existing sudo security model

## 🛡️ **Security Features**

### **Enhanced Security Protections**
All existing security features remain active with target user functionality:

#### **Shell Command Blocking**
```bash
sudosh(www-data):/var/www# bash
sudosh: shell commands are not permitted

sudosh(postgres):/var/lib# python -c 'import os; os.system("ls")'
sudosh: shell commands are not permitted
```

#### **Dangerous Command Detection**
```bash
sudosh(root):/# rm -rf /important/data
⚠️  WARNING: This command uses dangerous recursive or force flags
Command: rm -rf /important/data
This command could be dangerous. Are you sure you want to proceed? (yes/no):
```

#### **System Directory Protection**
```bash
sudosh(appuser):/# ls /etc
⚠️  WARNING: This command accesses critical system directories
Command: ls /etc
This command could be dangerous. Are you sure you want to proceed? (yes/no):
```

### **Target User Validation**
- **User Existence Check** - Validates target user exists in system
- **System Account Protection** - Warns about system accounts (UID < 100)
- **Allowed System Users** - Permits common service accounts (www-data, postgres, etc.)
- **User Confirmation** - Requires explicit approval for system accounts

## 🖥️ **User Interface**

### **Enhanced Prompt**
The prompt clearly indicates the target user context:

```bash
# Normal sudosh prompt
sudosh:/current/directory# 

# Target user prompt
sudosh(targetuser):/current/directory# 
```

### **Command Line Options**
```bash
Usage: sudosh [options]

Options:
  -h, --help              Show this help message
      --version           Show version information
  -v, --verbose           Enable verbose output
  -l, --log-session FILE  Log entire session to FILE
  -u, --user USER         Run commands as target USER
```

## 📝 **Logging and Auditing**

### **Enhanced Logging**
All commands executed with target user functionality are logged with context:

#### **Log Format**
```
# Normal command logging
username: command_executed

# Target user command logging  
username: command_executed (as targetuser)
```

#### **Security Event Logging**
```
# Permission validation
username: validated permission to run commands as user 'targetuser'

# Unauthorized access attempts
username: unauthorized target user access attempt

# Target user validation failures
username: invalid target user 'nonexistent_user'
```

### **Audit Trail**
- All target user access attempts logged
- Permission validation results recorded
- Command execution context preserved
- Security violations tracked with target user info

## ⚙️ **Technical Implementation**

### **Environment Setup**
When running commands as target user, the environment is properly configured:

#### **User Context Changes**
- **UID/GID** - Changed to target user's UID/GID
- **Supplementary Groups** - Initialized for target user
- **Environment Variables** - Set for target user context

#### **Environment Variables Set**
```bash
HOME=/target/user/home/directory
USER=targetuser
LOGNAME=targetuser
```

### **Process Execution**
```c
/* Target user execution flow */
1. Validate target user exists
2. Check sudoers permissions
3. Fork child process
4. Change to target user privileges
5. Set environment variables
6. Execute command
7. Log execution with context
```

## 🎯 **Use Cases**

### **1. Service Account Management**
```bash
# Manage web server files
sudosh -u www-data
sudosh(www-data):/var/www# chown www-data:www-data uploads/
sudosh(www-data):/var/www# chmod 755 public/
```

### **2. Database Administration**
```bash
# Database maintenance
sudosh -u postgres
sudosh(postgres):/var/lib/postgresql# pg_dump mydb > backup.sql
sudosh(postgres):/var/lib/postgresql# psql -c "VACUUM ANALYZE;"
```

### **3. Application Deployment**
```bash
# Deploy as application user
sudosh -u appuser
sudosh(appuser):/opt/app# git pull origin main
sudosh(appuser):/opt/app# ./deploy.sh
```

### **4. System Administration**
```bash
# Full system access
sudosh -u root
sudosh(root):/# systemctl restart nginx
sudosh(root):/# mount /dev/sdb1 /mnt/backup
```

## 🔧 **Configuration**

### **Sudoers Configuration**
Target user functionality requires appropriate sudoers configuration:

#### **Basic Configuration**
```bash
# /etc/sudoers or /etc/sudoers.d/sudosh

# Allow admin group to run as any user
%admin ALL=(ALL) ALL

# Allow specific user to run as web server
webadmin ALL=(www-data) ALL

# Allow DBA to run as database users
dba ALL=(postgres,mysql) ALL
```

#### **Advanced Configuration**
```bash
# Restrict to specific commands
webadmin ALL=(www-data) /usr/bin/systemctl restart apache2
webadmin ALL=(www-data) /bin/chown www-data\:www-data *

# Allow without password for specific users
appdeployer ALL=(appuser) NOPASSWD: ALL

# Time-based restrictions
Cmnd_Alias DEPLOY_CMDS = /opt/app/deploy.sh, /usr/bin/git
deployer ALL=(appuser) DEPLOY_CMDS
```

## ✅ **Testing**

### **Comprehensive Test Suite**
The target user functionality includes extensive testing:

#### **Test Categories**
- **Target User Validation** - User existence and validity checks
- **Permission Checking** - Sudoers rule validation
- **Command Execution** - Proper privilege changes
- **Security Integration** - Compatibility with security features
- **Environment Setup** - Correct environment configuration
- **Edge Cases** - Error handling and boundary conditions
- **Integration** - Compatibility with existing features

#### **Test Results**
```
=== Target User Test Results ===
Total tests: 8
Passed: 8
Failed: 0
✅ All target user tests passed!
```

## 🚀 **Benefits**

### **Security Benefits**
- **Principle of Least Privilege** - Run with minimum required permissions
- **Audit Compliance** - Complete audit trail for all actions
- **Permission Validation** - Enforced sudoers rule compliance
- **Environment Isolation** - Proper user context separation

### **Operational Benefits**
- **Simplified Administration** - Single tool for multi-user operations
- **Clear Context** - Visual indication of execution context
- **Consistent Interface** - Same sudosh features across all users
- **Enhanced Logging** - Detailed audit information

### **Administrative Benefits**
- **Centralized Control** - Sudoers-based permission management
- **Flexible Configuration** - Granular permission control
- **Security Monitoring** - Comprehensive logging and alerting
- **Compliance Support** - Audit trail for regulatory requirements

## 📋 **Requirements**

### **System Requirements**
- **Sudoers Configuration** - Appropriate runas permissions
- **Target User Accounts** - Valid system user accounts
- **PAM Integration** - For authentication (existing requirement)
- **Syslog Access** - For logging (existing requirement)

### **Permission Requirements**
- **Calling User** - Must have sudo privileges
- **Target User** - Must exist in system
- **Sudoers Rules** - Must allow runas access to target user
- **System Access** - Appropriate file system permissions

## 🔄 **Compatibility**

### **Backward Compatibility**
- **Existing Features** - All current functionality preserved
- **Command Line** - Existing options continue to work
- **Configuration** - No changes to existing configuration
- **Scripts** - Existing automation continues to function

### **Integration**
- **Security Features** - Full compatibility with enhanced security
- **Logging System** - Enhanced with target user context
- **Authentication** - Uses existing PAM integration
- **Permission System** - Extends existing sudoers integration

The target user functionality provides a powerful, secure way to run commands as different users while maintaining all of sudosh's security protections and audit capabilities.
