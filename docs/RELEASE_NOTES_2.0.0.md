# Sudosh 2.0.0 Release Notes

## Major Release - Enhanced Security and Sudo Replacement

**Release Date:** December 2024  
**Version:** 2.0.0  
**Breaking Changes:** Yes - Major version upgrade with significant enhancements

---

## 🚀 **Major New Features**

### 1. **Enhanced Shell Access Restriction**
- **Universal Shell Blocking**: All users without explicit shell permissions are restricted from accessing shells
- **Unified Behavior**: Both command-line and interactive modes show warning and continue in secure sudosh environment
- **Extended Coverage**: Now blocks `su`, `bash`, `sh`, `zsh`, `python`, `perl`, `ruby`, and other shell-like commands
- **Simple Messaging**: Clean, non-intrusive warning messages for better user experience

### 2. **Improved Sudo Replacement**
- **System Integration**: Replaces `/usr/bin/sudo` instead of `/usr/local/bin/sudo` for better system compatibility
- **Manpage Integration**: Automatically installs sudosh manpage as `sudo.8` and backs up original to `osudo.8`
- **Safe Backup**: Original sudo binary backed up to `osudo` for easy restoration
- **Complete Compatibility**: Full sudo command-line compatibility with enhanced security

### 3. **Secure File Editing (sudoedit)**
- **Protected Editor Environment**: Secure file editing with `-e` option that prevents shell escapes
- **Editor Preference Support**: Respects `SUDO_EDITOR`, `VISUAL`, `EDITOR` environment variables or defaults to `vi`
- **Multiple File Support**: Edit multiple files in a single session: `sudosh -e file1 file2 file3`
- **Security Hardening**: Disables shell access, external commands, and dangerous editor features

### 4. **Consolidated Documentation**
- **Centralized Docs**: All documentation moved to `docs/` directory
- **Comprehensive Guides**: Updated installation, deployment, and security guides
- **Release History**: Complete version history and upgrade paths
- **Testing Documentation**: Comprehensive testing procedures and validation

---

## 🔧 **Technical Improvements**

### Security Enhancements
- **Shell Restriction Logic**: Simplified and unified shell access control
- **Permission Validation**: Enhanced user permission checking with group-based access
- **Secure File Editing**: Protected sudoedit environment preventing shell escapes
- **Audit Logging**: Improved security violation logging and tracking
- **Command Validation**: Strengthened command validation and sanitization

### Installation & Deployment
- **System Paths**: Updated to use standard system paths (`/usr/bin`, `/usr/share/man/man8`)
- **Backup Strategy**: Improved backup and restoration procedures
- **Package Management**: Enhanced RPM and DEB package generation
- **Configuration**: Streamlined configuration and setup processes

### Testing & Quality
- **Comprehensive Test Suite**: 16+ shell restriction tests, mode behavior validation
- **Integration Testing**: Enhanced CI/CD pipeline with regression testing
- **Security Testing**: CVE-specific security validation and penetration testing
- **Performance Testing**: Load testing and performance optimization validation

---

## 📋 **Detailed Changes**

### Shell Access Restriction
```bash
# Before (1.9.x): Different behavior for different modes
sudosh bash  # Hard rejection
# In interactive: Hard rejection

# After (2.0.0): Unified behavior
sudosh bash  # Warning + continue in interactive mode
# In interactive: Warning + continue in interactive mode
```

### Sudo Replacement Installation
```bash
# Before (1.9.x)
make install-sudo-replacement  # Installs to /usr/local/bin/sudo

# After (2.0.0)
make install-sudo-replacement  # Installs to /usr/bin/sudo
                              # Backs up original to osudo
                              # Installs manpage as sudo.8
```

### Secure File Editing
```bash
# New in 2.0.0: Secure sudoedit functionality
sudosh -e /etc/hosts                    # Edit single file
sudosh -e /etc/hosts /etc/resolv.conf   # Edit multiple files
EDITOR=nano sudosh -e /etc/config.conf  # Use preferred editor
sudosh -u www-data -e /var/www/config   # Edit as specific user

# Security features:
# - Prevents shell escapes from editors
# - Respects SUDO_EDITOR > VISUAL > EDITOR > vi
# - Comprehensive logging of all edit operations
# - Protected environment blocks external commands
```

### Documentation Structure
```
# Before (1.9.x): Scattered documentation
README.md
CHANGELOG.md
docs/SOME_GUIDE.md
FEATURE_NOTES.md

# After (2.0.0): Consolidated structure
README.md
docs/
├── CHANGELOG.md
├── RELEASE_NOTES_2.0.0.md
├── COMPREHENSIVE_GUIDE.md
├── DEPLOYMENT_GUIDE.md
├── SECURITY_TESTING_SUMMARY.md
└── [all other documentation]
```

---

## 🔄 **Migration Guide**

### From 1.9.x to 2.0.0

#### 1. **Backup Current Installation**
```bash
# Backup current sudosh installation
sudo cp /usr/local/bin/sudosh /usr/local/bin/sudosh.1.9.x.backup
sudo cp /usr/local/share/man/man1/sudosh.1 /usr/local/share/man/man1/sudosh.1.1.9.x.backup
```

#### 2. **Uninstall Previous Version**
```bash
# If using sudo replacement
sudo make uninstall-sudo-replacement

# If using standard installation
sudo make uninstall
```

#### 3. **Install 2.0.0**
```bash
# Build new version
make clean && make

# Install as sudo replacement (recommended)
sudo make install-sudo-replacement

# Or install alongside existing sudo
sudo make install
```

#### 4. **Update Configuration**
- Review shell access permissions for users
- Update sudoers rules if needed for shell access
- Test shell restriction behavior with your user base

---

## ⚠️ **Breaking Changes**

### 1. **Shell Access Behavior**
- **Impact**: Users without shell permissions will see different behavior
- **Change**: Instead of hard rejection, users are kept in secure sudosh environment
- **Action Required**: Review user permissions and update documentation

### 2. **Installation Paths**
- **Impact**: Sudo replacement now uses `/usr/bin/sudo` instead of `/usr/local/bin/sudo`
- **Change**: Better system integration but different path
- **Action Required**: Update any scripts or documentation referencing old paths

### 3. **Manpage Location**
- **Impact**: When installed as sudo replacement, manpage appears as `sudo.8`
- **Change**: Original sudo manpage backed up to `osudo.8`
- **Action Required**: Update any references to sudo manpage location

---

## 🧪 **Testing & Validation**

### Test Coverage
- ✅ **16/16 shell restriction tests passing**
- ✅ **4/4 mode behavior tests passing**
- ✅ **2/2 warning message tests passing**
- ✅ **Complete integration test suite**
- ✅ **Security regression testing**
- ✅ **CVE-specific security validation**

### Validation Checklist
- [ ] Shell restriction works for all blocked commands
- [ ] Sudo replacement installation successful
- [ ] Original sudo backed up correctly
- [ ] Manpage installation and backup successful
- [ ] All existing functionality preserved
- [ ] Performance impact minimal
- [ ] Security enhancements active

---

## 📚 **Documentation Updates**

### New Documentation
- `docs/RELEASE_NOTES_2.0.0.md` - This release notes document
- `docs/SHELL_RESTRICTION_IMPLEMENTATION_SUMMARY.md` - Detailed shell restriction guide
- Updated `docs/SUDO_REPLACEMENT_GUIDE.md` - Enhanced sudo replacement procedures
- Updated `docs/DEPLOYMENT_GUIDE.md` - 2.0-specific deployment instructions

### Updated Documentation
- `README.md` - Updated for 2.0 features and installation
- `docs/COMPREHENSIVE_GUIDE.md` - Complete feature documentation
- `docs/TESTING_GUIDE.md` - Enhanced testing procedures
- `docs/SECURITY_TESTING_SUMMARY.md` - Updated security validation

---

## 🔮 **Future Roadmap**

### Planned for 2.1.x
- Enhanced LDAP/Active Directory integration
- Advanced shell access time-based restrictions
- Improved audit reporting and analytics
- Extended sudo option compatibility

### Long-term Goals
- Cloud-native deployment options
- Advanced threat detection integration
- Machine learning-based anomaly detection
- Enterprise management console

---

## 🤝 **Contributing**

Sudosh 2.0 represents a significant step forward in secure command execution. We welcome contributions, bug reports, and feature requests.

- **Issues**: Report bugs and request features on GitHub
- **Documentation**: Help improve documentation and guides
- **Testing**: Contribute test cases and validation scenarios
- **Security**: Report security issues responsibly

---

## 📞 **Support**

For support with Sudosh 2.0:
- Review documentation in `docs/` directory
- Check existing GitHub issues
- Consult the comprehensive guide: `docs/COMPREHENSIVE_GUIDE.md`
- Review deployment guide: `docs/DEPLOYMENT_GUIDE.md`

---

**Thank you for using Sudosh 2.0! This release represents months of development focused on security, usability, and system integration.**
