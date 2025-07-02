#!/bin/bash

# Setup script for sudosh - Interactive sudo shell
set -e

echo "Setting up sudosh build environment..."

# Update package lists
sudo apt-get update

# Install essential build tools
sudo apt-get install -y \
    build-essential \
    gcc \
    make \
    libc6-dev

# Install PAM development libraries (correct package name)
sudo apt-get install -y \
    libpam0g-dev

# Install additional development tools that might be needed
sudo apt-get install -y \
    pkg-config \
    manpages-dev

# Verify we're in the correct directory
if [ ! -f "Makefile" ] || [ ! -f "sudosh.h" ]; then
    echo "Error: Not in sudosh project directory"
    exit 1
fi

# Clean any previous builds
make clean || true

# Create necessary directories
mkdir -p obj bin tests

# Build the project
echo "Building sudosh..."
make

# Verify the binary was created
if [ ! -f "bin/sudosh" ]; then
    echo "Error: sudosh binary was not created"
    exit 1
fi

echo "Build completed successfully!"
echo "sudosh binary created at: bin/sudosh"

# Show build information
echo "Build information:"
echo "- Binary size: $(ls -lh bin/sudosh | awk '{print $5}')"
echo "- Binary permissions: $(ls -l bin/sudosh | awk '{print $1}')"

# Check PAM linking
if ldd bin/sudosh 2>/dev/null | grep -q pam; then
    echo "- PAM libraries: LINKED"
else
    echo "- PAM libraries: NOT LINKED (using mock auth)"
fi

echo "Setup completed successfully!"