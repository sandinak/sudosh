# Sudosh Packaging Guide

This document describes how to build packages for sudosh on various Linux distributions.

## Overview

Sudosh supports building packages for both RPM-based (DNF) and DEB-based (APT) package management systems. The packaging system is integrated into the Makefile and provides automated package generation with proper dependencies, permissions, and post-installation scripts.

## Supported Package Formats

### RPM Packages (DNF-based systems)
- **Red Hat Enterprise Linux (RHEL)**
- **CentOS / AlmaLinux / Rocky Linux**
- **Fedora**
- **openSUSE**

### DEB Packages (APT-based systems)
- **Ubuntu**
- **Debian**
- **Linux Mint**
- **Elementary OS**

## Prerequisites

### For RPM Package Building
```bash
# RHEL/CentOS/AlmaLinux/Rocky Linux
sudo dnf install rpm-build rpm-devel gcc make pam-devel

# Fedora
sudo dnf install rpm-build rpmdevtools gcc make pam-devel

# openSUSE
sudo zypper install rpm-build gcc make pam-devel
```

### For DEB Package Building
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential devscripts debhelper gcc make libpam0g-dev

# Additional tools for signing (optional)
sudo apt install gnupg2 dh-make
```

## Building Packages

### Build Both Package Types
```bash
make packages
```
This will create both RPM and DEB packages in the `dist/` directory.

### Build RPM Package Only
```bash
make rpm
```

### Build DEB Package Only
```bash
make deb
```

### Build Source Tarball Only
```bash
make packaging/sudosh-1.3.2.tar.gz
```

## Package Contents

### Files Installed
- **Binary**: `/usr/bin/sudosh` (with setuid root permissions)
- **Manual page**: `/usr/share/man/man1/sudosh.1`
- **Documentation**: README.md, CHANGELOG.md
- **License**: LICENSE file

### Post-Installation Actions
- Creates authentication cache directory: `/var/run/sudosh`
- Sets secure permissions (700, root:root) on cache directory

### Post-Removal Actions
- Removes authentication cache directory on package purge/removal

## Package Metadata

### RPM Package Information
- **Name**: sudosh
- **Version**: 1.3.2
- **License**: MIT
- **Dependencies**: pam
- **Build Dependencies**: gcc, make, pam-devel

### DEB Package Information
- **Package**: sudosh
- **Section**: admin
- **Priority**: optional
- **Dependencies**: libpam0g
- **Build Dependencies**: debhelper (>= 9), gcc, make, libpam0g-dev

## Package Features

### Security Features
- **Setuid root binary** for proper privilege escalation
- **Secure cache directory** with restricted permissions
- **PAM integration** for authentication
- **Comprehensive audit logging**

### Authentication Caching
- **15-minute default timeout** (configurable)
- **Session isolation** per user/TTY
- **Automatic cleanup** of expired cache files
- **Secure file permissions** (0600, root-owned)

### User Experience
- **Color support** inherited from calling shell
- **Command history** with arrow key navigation
- **Tab completion** for paths and commands
- **Comprehensive error messages**

## Testing Package Installation

### RPM Package Testing
```bash
# Install the package
sudo rpm -ivh dist/sudosh-1.3.2-1.*.rpm

# Test functionality
sudo sudosh
ls -la /var/run/sudosh  # Check cache directory

# Remove the package
sudo rpm -e sudosh
```

### DEB Package Testing
```bash
# Install the package
sudo dpkg -i dist/sudosh_1.3.2-1_*.deb

# Fix any dependency issues
sudo apt-get install -f

# Test functionality
sudo sudosh
ls -la /var/run/sudosh  # Check cache directory

# Remove the package
sudo apt remove sudosh
```

## Package Verification

### Verify Package Contents
```bash
# RPM package
rpm -qlp dist/sudosh-1.3.2-1.*.rpm

# DEB package
dpkg-deb -c dist/sudosh_1.3.2-1_*.deb
```

### Verify Package Dependencies
```bash
# RPM package
rpm -qRp dist/sudosh-1.3.2-1.*.rpm

# DEB package
dpkg-deb -I dist/sudosh_1.3.2-1_*.deb
```

## Customization

### Modifying Package Metadata
Edit the template files in `packaging/`:
- **RPM**: `packaging/sudosh.spec.in`
- **DEB**: `packaging/debian/control.in`

### Changing Package Version
The version is automatically extracted from `src/sudosh.h`. Update the `SUDOSH_VERSION` define to change the package version.

### Adding Custom Dependencies
Modify the appropriate template files:
- **RPM**: Add to `Requires:` line in `packaging/sudosh.spec.in`
- **DEB**: Add to `Depends:` line in `packaging/debian/control.in`

## Troubleshooting

### Common Issues

#### Missing Build Dependencies
```bash
# Error: "gcc: command not found"
# Solution: Install build tools
sudo apt install build-essential  # Debian/Ubuntu
sudo dnf groupinstall "Development Tools"  # RHEL/CentOS/Fedora
```

#### PAM Development Headers Missing
```bash
# Error: "pam headers not found"
# Solution: Install PAM development package
sudo apt install libpam0g-dev     # Debian/Ubuntu
sudo dnf install pam-devel        # RHEL/CentOS/Fedora
```

#### Permission Errors During Build
```bash
# Error: "Permission denied"
# Solution: Ensure proper permissions on packaging files
chmod +x packaging/debian/rules packaging/debian/postinst packaging/debian/postrm
```

### Cleaning Build Artifacts
```bash
# Clean all packaging files
make clean-packages

# Clean everything including build files
make clean
```

## Distribution-Specific Notes

### RHEL/CentOS/AlmaLinux
- May require EPEL repository for some build dependencies
- Use `dnf` on newer versions, `yum` on older versions

### Ubuntu/Debian
- LTS versions recommended for production deployments
- May need to enable universe repository for some dependencies

### Fedora
- Latest packaging tools and standards
- Frequent updates may require package rebuilds

## Contributing

When contributing packaging improvements:
1. Test on multiple distributions
2. Verify package installation and removal
3. Check that all dependencies are correctly specified
4. Ensure post-installation scripts work properly
5. Update this documentation for any changes

## Support

For packaging-related issues:
- Check the build logs for specific error messages
- Verify all prerequisites are installed
- Test on a clean system when possible
- Report issues with distribution and version information
