# Sudosh 2.0 Deployment and Integration Guide

This guide provides comprehensive instructions for deploying sudosh 2.0 in production environments, integrating with CI/CD systems, and implementing Ansible plugins.

## 🚀 **Sudosh 2.0 Overview**

### Major Changes in 2.0
- **System Integration**: Replaces `/usr/bin/sudo` for better compatibility
- **Enhanced Shell Restriction**: Universal shell blocking with graceful fallback
- **Manpage Integration**: Installs as `sudo.8` with backup to `osudo.8`
- **Unified Behavior**: Consistent experience across all usage modes
- **Improved Security**: Extended shell coverage including `su` command

### Migration from 1.9.x
Before deploying 2.0, ensure you:
1. **Backup existing installation**
2. **Review shell access policies** for your users
3. **Test shell restriction behavior** in your environment
4. **Update documentation** referencing old paths

## 🚀 Production Deployment

### Prerequisites

#### System Requirements
- Linux/Unix system with PAM support
- GCC compiler with C99 support
- PAM development libraries
- Sudo installed and configured
- Root privileges for installation

#### Package Dependencies

**RHEL/CentOS/Fedora:**
```bash
sudo yum install gcc pam-devel make
# or
sudo dnf install gcc pam-devel make
```

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install gcc libpam0g-dev make
```

**SUSE/openSUSE:**
```bash
sudo zypper install gcc pam-devel make
```

### Installation Methods

#### Method 1: Package Installation (Recommended)

**RPM-based Systems:**
```bash
# Download and build package
git clone https://github.com/sandinak/sudosh.git
cd sudosh
make rpm

# Install package
sudo rpm -ivh dist/sudosh-*.rpm

# Verify installation
sudosh --version
```

**DEB-based Systems:**
```bash
# Download and build package
git clone https://github.com/sandinak/sudosh.git
cd sudosh
make deb

# Install package
sudo dpkg -i dist/sudosh_*.deb
sudo apt-get install -f  # Fix dependencies if needed

# Verify installation
sudosh --version
```

#### Method 2: Source Installation

```bash
# Clone repository
git clone https://github.com/sandinak/sudosh.git
cd sudosh

# Build and install
make
sudo make install

# Verify installation
sudosh --version
which sudosh
```

### Post-Installation Configuration

#### 1. Set Proper Permissions
```bash
# Ensure sudosh has setuid bit (done by make install)
sudo chmod 4755 /usr/local/bin/sudosh

# Verify permissions
ls -la /usr/local/bin/sudosh
# Should show: -rwsr-xr-x 1 root root
```

#### 2. Configure Sudoers
Add sudosh to your sudoers configuration:

```bash
sudo visudo
```

Add lines like:
```
# Allow users to run sudosh
%wheel ALL=(ALL) NOPASSWD: /usr/local/bin/sudosh
%sudo ALL=(ALL) NOPASSWD: /usr/local/bin/sudosh

# Or for specific users
username ALL=(ALL) NOPASSWD: /usr/local/bin/sudosh
```

#### 3. Configure Logging
```bash
# Create log directory
sudo mkdir -p /var/log/sudosh
sudo chmod 755 /var/log/sudosh

# Configure logrotate
sudo tee /etc/logrotate.d/sudosh << EOF
/var/log/sudosh/*.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
    create 644 root root
}
EOF
```

#### 4. System Integration
```bash
# Add to system PATH (optional)
echo 'export PATH="/usr/local/bin:$PATH"' >> /etc/profile.d/sudosh.sh

# Create alias for easy access (optional)
echo 'alias ss="sudosh"' >> /etc/profile.d/sudosh.sh
```

## 🔧 CI/CD Integration

### Test Mode Configuration

Sudosh includes a comprehensive test mode for unattended execution in CI/CD environments:

```bash
# Enable test mode
export SUDOSH_TEST_MODE=1

# Run tests
make test

# Run comprehensive test suite
./tests/run_all_tests.sh
```

### GitHub Actions Integration

Create `.github/workflows/sudosh-test.yml`:

```yaml
name: Sudosh Test Suite

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y gcc libpam0g-dev make
    
    - name: Build sudosh
      run: make
    
    - name: Run test suite
      run: |
        export SUDOSH_TEST_MODE=1
        ./tests/run_all_tests.sh
    
    - name: Test command-line execution
      run: |
        export SUDOSH_TEST_MODE=1
        ./bin/sudosh echo "CI/CD test successful"
        ./bin/sudosh -c "echo 'Command mode test successful'"
```

### Jenkins Integration

Create `Jenkinsfile`:

```groovy
pipeline {
    agent any
    
    environment {
        SUDOSH_TEST_MODE = '1'
    }
    
    stages {
        stage('Build') {
            steps {
                sh 'make clean'
                sh 'make'
            }
        }
        
        stage('Test') {
            steps {
                sh './tests/run_all_tests.sh'
            }
        }
        
        stage('Package') {
            steps {
                sh 'make packages'
                archiveArtifacts artifacts: 'dist/*', fingerprint: true
            }
        }
    }
    
    post {
        always {
            publishTestResults testResultsPattern: 'test-results.xml'
        }
    }
}
```

### GitLab CI Integration

Create `.gitlab-ci.yml`:

```yaml
stages:
  - build
  - test
  - package

variables:
  SUDOSH_TEST_MODE: "1"

before_script:
  - apt-get update -qq && apt-get install -y -qq gcc libpam0g-dev make

build:
  stage: build
  script:
    - make
  artifacts:
    paths:
      - bin/
    expire_in: 1 hour

test:
  stage: test
  dependencies:
    - build
  script:
    - ./tests/run_all_tests.sh
  artifacts:
    reports:
      junit: test-results.xml

package:
  stage: package
  dependencies:
    - build
  script:
    - make packages
  artifacts:
    paths:
      - dist/
    expire_in: 1 week
  only:
    - main
    - tags
```

## 📦 Package Distribution

### Creating Distribution Packages

```bash
# Build all package types
make packages

# Build specific package types
make rpm     # RPM packages
make deb     # DEB packages

# Packages will be created in dist/ directory
ls -la dist/
```

### Repository Setup

#### RPM Repository
```bash
# Create repository structure
mkdir -p /var/www/html/repo/rpm
cp dist/*.rpm /var/www/html/repo/rpm/

# Create repository metadata
createrepo /var/www/html/repo/rpm/
```

#### DEB Repository
```bash
# Create repository structure
mkdir -p /var/www/html/repo/deb
cp dist/*.deb /var/www/html/repo/deb/

# Create Packages file
cd /var/www/html/repo/deb/
dpkg-scanpackages . /dev/null | gzip -9c > Packages.gz
```

### Automated Distribution

Create distribution script `scripts/distribute.sh`:

```bash
#!/bin/bash
# Automated package distribution

set -e

VERSION=$(grep "VERSION" src/sudosh.h | cut -d'"' -f2)
DIST_DIR="dist"

echo "Building sudosh version $VERSION packages..."

# Build packages
make packages

# Upload to repository
rsync -av $DIST_DIR/ user@repo-server:/var/www/html/repo/

# Update repository metadata
ssh user@repo-server "createrepo /var/www/html/repo/rpm/"
ssh user@repo-server "cd /var/www/html/repo/deb/ && dpkg-scanpackages . /dev/null | gzip -9c > Packages.gz"

echo "Distribution complete!"
```

## 🔍 Monitoring and Maintenance

### Log Monitoring

```bash
# Monitor sudosh logs
tail -f /var/log/syslog | grep sudosh

# Monitor session logs
tail -f /var/log/sudosh/*.log

# Set up log alerts
grep -i "AI_BLOCKED\|SECURITY_VIOLATION" /var/log/syslog
```

### Health Checks

Create health check script `scripts/health-check.sh`:

```bash
#!/bin/bash
# Sudosh health check

export SUDOSH_TEST_MODE=1

# Test basic functionality
if ! ./bin/sudosh echo "Health check" > /dev/null 2>&1; then
    echo "ERROR: Basic functionality test failed"
    exit 1
fi

# Test AI detection
if ! AUGMENT_SESSION_ID=test ./bin/sudosh echo "test" 2>&1 | grep -q "AI session detected"; then
    echo "ERROR: AI detection test failed"
    exit 1
fi

echo "Health check passed"
exit 0
```

### Performance Monitoring

```bash
# Monitor sudosh performance
time sudosh -c "echo 'Performance test'"

# Monitor memory usage
ps aux | grep sudosh

# Monitor file descriptor usage
lsof | grep sudosh
```

This deployment guide provides a comprehensive foundation for production deployment and CI/CD integration. For specific environment requirements, consult the additional documentation in the `docs/` directory.
