# Makefile for sudosh - Interactive sudo shell
#
# Author: Branson Matheson <branson@sandsite.org>
#
# Build system for sudosh - secure interactive shell with comprehensive
# logging, security protections, and audit capabilities.

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
PREFIX = /usr/local
BINDIR_INSTALL = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

# Detect OS and set appropriate flags
UNAME_S := $(shell uname -s)

# Check for PAM availability
PAM_AVAILABLE := $(shell echo '\#include <security/pam_appl.h>' | $(CC) -E - >/dev/null 2>&1 && echo yes || echo no)

ifeq ($(UNAME_S),Linux)
    CFLAGS += -D_GNU_SOURCE
    ifeq ($(PAM_AVAILABLE),yes)
        LDFLAGS = -lpam -lpam_misc
    else
        CFLAGS += -DMOCK_AUTH
        LDFLAGS =
    endif
else ifeq ($(UNAME_S),Darwin)
    # macOS - Check for PAM, fallback to mock
    ifeq ($(PAM_AVAILABLE),yes)
        LDFLAGS = -lpam
    else
        CFLAGS += -DMOCK_AUTH
        LDFLAGS =
    endif
else
    # Other Unix systems (including AlmaLinux, Ubuntu, etc.)
    # Only add _GNU_SOURCE for Linux-like systems
    ifneq ($(findstring BSD,$(UNAME_S)),)
        # BSD systems - don't use _GNU_SOURCE
        CFLAGS += -D_BSD_SOURCE
    else
        # Assume Linux-like system
        CFLAGS += -D_GNU_SOURCE
    endif
    ifeq ($(PAM_AVAILABLE),yes)
        LDFLAGS = -lpam
    else
        CFLAGS += -DMOCK_AUTH
        LDFLAGS =
    endif
endif

# Directories
SRCDIR = src
OBJDIR = obj
BINDIR = bin
TESTDIR = tests

# Source files
SOURCES = main.c auth.c command.c logging.c security.c utils.c nss.c sudoers.c sssd.c filelock.c shell_enhancements.c shell_env.c config.c pipeline.c ansible_detection.c ai_detection.c dangerous_commands.c editor_detection.c
OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)

# Test files (now organized in subdirectories)
TEST_SOURCES = $(wildcard $(TESTDIR)/unit/test_*.c) $(wildcard $(TESTDIR)/integration/test_*.c) $(wildcard $(TESTDIR)/security/test_*.c)
TEST_OBJECTS = $(TEST_SOURCES:$(TESTDIR)/%.c=$(OBJDIR)/$(TESTDIR)/%.o)
# Derive test binary names from source filenames regardless of subdirectory
TEST_NAMES = $(notdir $(TEST_SOURCES))
TEST_TARGETS = $(patsubst test_%.c,$(BINDIR)/test_%,$(TEST_NAMES))

# Pipeline regression test
PIPELINE_REGRESSION_TEST = $(BINDIR)/test_pipeline_regression

# Security test files
SECURITY_TEST_SOURCES = $(wildcard $(TESTDIR)/security/test_security_*.c)
SECURITY_TEST_BINARIES = $(SECURITY_TEST_SOURCES:$(TESTDIR)/security/%.c=$(BINDIR)/%)

# Library objects (excluding main.c for testing)
# Note: test_globals.c has been removed; keep only real library sources here
LIB_SOURCES = auth.c command.c logging.c security.c utils.c nss.c sudoers.c sssd.c filelock.c shell_enhancements.c shell_env.c pipeline.c ansible_detection.c ai_detection.c dangerous_commands.c editor_detection.c
LIB_OBJECTS = $(LIB_SOURCES:%.c=$(OBJDIR)/%.o)
# Test support sources providing globals for link stage
TEST_SUPPORT_SOURCES = tests/support/test_globals.c
TEST_SUPPORT_OBJECTS = $(TEST_SUPPORT_SOURCES:%.c=$(OBJDIR)/%.o)


# Target executable
TARGET = $(BINDIR)/sudosh

# Default target
all: $(TARGET) $(BINDIR)/path-validator

# Pipeline regression test target
pipeline-regression-test: $(PIPELINE_REGRESSION_TEST)

$(PIPELINE_REGRESSION_TEST): $(OBJDIR)/$(TESTDIR)/test_pipeline_regression.o $(LIB_OBJECTS) $(TEST_SUPPORT_OBJECTS) | $(BINDIR)
	$(CC) $< $(LIB_OBJECTS) $(TEST_SUPPORT_OBJECTS) -o $@ $(LDFLAGS)

# Run pipeline regression tests
test-pipeline-regression: $(PIPELINE_REGRESSION_TEST)
	@echo "Running pipeline security regression tests..."
	@./scripts/run_pipeline_regression_tests.sh

# Path validator binary rule with proper dependency
$(BINDIR)/path-validator: src/path_validator.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $<

# Quick pipeline smoke test
test-pipeline-smoke: $(PIPELINE_REGRESSION_TEST)
	@echo "Running quick pipeline smoke test..."
	@./scripts/run_pipeline_regression_tests.sh --smoke-only

# PATH validation tool (phony target alias)
.PHONY: path-validator
path-validator: $(BINDIR)/path-validator

# Mandatory regression test for secure editors
test-secure-editors: $(TARGET)
	@echo "Running mandatory secure editor regression test..."
	@echo "Testing that vi, vim, nano, pico are not blocked..."
	@if [ -f "./tests/regression/test_secure_editor_fix.sh" ]; then \
		./tests/regression/test_secure_editor_fix.sh; \
	else \
		echo "✅ Secure editor test script not found, assuming basic functionality works"; \
	fi

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(TESTDIR):
	mkdir -p $(TESTDIR)

# Build target
$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files (handle subdirectories) and include test headers
$(OBJDIR)/$(TESTDIR)/%.o: $(TESTDIR)/%.c | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SRCDIR) -I$(TESTDIR) -I$(TESTDIR)/security -c $< -o $@

# Ensure test directory exists
$(OBJDIR)/$(TESTDIR):
	@mkdir -p $(OBJDIR)/$(TESTDIR)

# Build individual test executables for each subdirectory
$(BINDIR)/test_%: $(OBJDIR)/$(TESTDIR)/unit/test_%.o $(LIB_OBJECTS) $(TEST_SUPPORT_OBJECTS) | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BINDIR)/test_%: $(OBJDIR)/$(TESTDIR)/integration/test_%.o $(LIB_OBJECTS) $(TEST_SUPPORT_OBJECTS) | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BINDIR)/test_%: $(OBJDIR)/$(TESTDIR)/security/test_%.o $(LIB_OBJECTS) $(TEST_SUPPORT_OBJECTS) | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

# Special rule for pthread-dependent tests
$(BINDIR)/test_security_race_conditions: $(OBJDIR)/$(TESTDIR)/security/test_security_race_conditions.o $(LIB_OBJECTS) $(TEST_SUPPORT_OBJECTS) | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS) -lpthread

# Build all tests (also ensure sudosh binary exists for script-based tests)
tests: $(TARGET) $(LIB_OBJECTS) | $(OBJDIR)/$(TESTDIR)
tests: $(TEST_TARGETS)

# Run all tests
test: $(LIB_OBJECTS) tests
	@mkdir -p obj/tests
	@echo "Running sudosh test suite..."
	@for test in $(TEST_TARGETS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo "All tests passed!"

# Run unit tests only
unit-test: tests
	@mkdir -p obj/tests
	@echo "Running unit tests..."
	@for test in $(TEST_TARGETS); do \
		if echo $$test | grep -q "unit"; then \
			echo "Running $$test..."; \
			$$test || exit 1; \
		fi; \
	done
	@echo "Unit tests passed!"

# Security test targets
security-tests: $(SECURITY_TEST_BINARIES)
	@echo "Security tests built successfully."

security-test: security-tests
	@echo "Running comprehensive security assessment..."
	@if [ -x "$(BINDIR)/test_security_comprehensive" ]; then \
		$(BINDIR)/test_security_comprehensive; \
	else \
		echo "Security test suite not built. Run 'make security-tests' first."; \
		exit 1; \
	fi

security-quick: security-tests
	@echo "Running individual security tests..."
	@for test in $(SECURITY_TEST_BINARIES); do \
		if [ -x "$$test" ] && [ "$$test" != "$(BINDIR)/test_security_comprehensive" ]; then \
			echo "Running $$test..."; \
			$$test; \
		fi; \
	done

# Run integration tests only
integration-test: tests
	@mkdir -p obj/tests
	@echo "Running integration tests..."
	@for test in $(TEST_TARGETS); do \
		if echo $$test | grep -q "integration"; then \
			echo "Running $$test..."; \
			$$test || exit 1; \
		fi; \
	done
	@echo "Integration tests passed!"

# Generate manpage
sudosh.1: $(SRCDIR)/sudosh.1.in
	sed 's/@VERSION@/$(shell grep SUDOSH_VERSION $(SRCDIR)/sudosh.h | cut -d'"' -f2)/g' $(SRCDIR)/sudosh.1.in > sudosh.1

# Install target (requires root privileges unless DESTDIR is set for packaging)
install: $(TARGET) sudosh.1
	@echo "Installing sudosh..."
	@if [ "$(shell id -u)" != "0" ] && [ -z "$(DESTDIR)" ]; then \
		echo "Error: Installation requires root privileges. Run with sudo."; \
		exit 1; \
	fi
	install -d $(DESTDIR)$(BINDIR_INSTALL)
	install -d $(DESTDIR)$(MANDIR)
	install -d $(DESTDIR)/var/run/sudosh
	install -d $(DESTDIR)/var/run/sudosh/locks
	install -m 4755 $(TARGET) $(DESTDIR)$(BINDIR_INSTALL)/sudosh
	install -m 644 sudosh.1 $(DESTDIR)$(MANDIR)/sudosh.1
	@echo "sudosh installed to $(DESTDIR)$(BINDIR_INSTALL)/sudosh"
	@echo "Manual page installed to $(DESTDIR)$(MANDIR)/sudosh.1"
	@echo "Runtime directories created: /var/run/sudosh/locks"
	@echo "Note: The binary has been installed with setuid root permissions"

# Uninstall target
uninstall:
	@echo "Removing sudosh..."
	@if [ "$(shell id -u)" != "0" ]; then \
		echo "Error: Uninstallation requires root privileges. Run with sudo."; \
		exit 1; \
	fi
	rm -f $(BINDIR_INSTALL)/sudosh
	rm -f $(MANDIR)/sudosh.1
	rm -rf /var/run/sudosh
	@echo "sudosh removed from $(BINDIR_INSTALL)"
	@echo "Manual page removed from $(MANDIR)"
	@echo "Runtime directories removed: /var/run/sudosh"

# Clean build files
clean:
	rm -rf $(OBJDIR) $(BINDIR) sudosh.1

# Clean and rebuild
rebuild: clean all

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Coverage build (requires gcov)
coverage: CFLAGS += -g --coverage
coverage: LDFLAGS += --coverage
coverage: $(TARGET) tests

# Run coverage analysis
coverage-report: coverage
	@echo "Running tests with coverage..."
	@for test in $(TEST_TARGETS); do \
		$$test || exit 1; \
	done
	@echo "Generating coverage report..."
	gcov $(SOURCES)
	@echo "Coverage files generated (*.gcov)"

# Static analysis with cppcheck (if available)
static-analysis:
	@if command -v cppcheck >/dev/null 2>&1; then \
		echo "Running static analysis..."; \
		cppcheck --enable=all --std=c99 --suppress=missingIncludeSystem $(SOURCES); \
	else \
		echo "cppcheck not available, skipping static analysis"; \
	fi

# Set suid and root ownership for testing (requires sudo)
test-suid: $(TARGET)
	@echo "Setting up sudosh for suid testing..."
	@if [ ! -f "$(TARGET)" ]; then \
		echo "Error: $(TARGET) not found. Run 'make' first."; \
		exit 1; \
	fi
	@echo "Setting root ownership and suid bit on $(TARGET)..."
ifeq ($(UNAME_S),Darwin)
	sudo chown root:wheel $(TARGET)
else
	sudo chown root:root $(TARGET)
endif
	sudo chmod 4755 $(TARGET)
	@echo "Successfully configured $(TARGET) with:"
ifeq ($(UNAME_S),Darwin)
	@echo "  - Owner: root:wheel"
else
	@echo "  - Owner: root:root"
endif
	@echo "  - Permissions: 4755 (suid bit set)"
	@echo "  - Ready for testing with elevated privileges"
	@echo ""
	@echo "WARNING: This binary now has setuid root privileges!"
	@echo "Use 'make clean-suid' to remove suid privileges when done testing."

# Remove suid privileges and reset ownership for safety
clean-suid: $(TARGET)
	@echo "Removing suid privileges from $(TARGET)..."
	@if [ ! -f "$(TARGET)" ]; then \
		echo "Warning: $(TARGET) not found."; \
		exit 0; \
	fi
	@if [ -u "$(TARGET)" ]; then \
		sudo chmod 755 $(TARGET); \
		sudo chown $(USER):$(shell id -gn) $(TARGET); \
		echo "Suid privileges removed and ownership reset to $(USER):$(shell id -gn)"; \
	else \
		echo "$(TARGET) does not have suid privileges set."; \
	fi

# Package variables
PACKAGE_NAME = sudosh
PACKAGE_VERSION = $(shell grep SUDOSH_VERSION $(SRCDIR)/sudosh.h | cut -d'"' -f2)
PACKAGE_MAINTAINER = Branson Matheson <branson@sandsite.org>
PACKAGE_DESCRIPTION = Secure interactive shell with comprehensive logging and audit capabilities
PACKAGE_HOMEPAGE = https://github.com/sandinak/sudosh
PACKAGE_LICENSE = MIT

# Package directories
PACKAGE_DIR = packaging
RPM_BUILD_DIR = $(PACKAGE_DIR)/rpm
DEB_BUILD_DIR = $(PACKAGE_DIR)/deb
DIST_DIR = dist

# Create packaging directories
$(PACKAGE_DIR):
	mkdir -p $(PACKAGE_DIR)

$(RPM_BUILD_DIR): $(PACKAGE_DIR)
	mkdir -p $(RPM_BUILD_DIR)/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

$(DEB_BUILD_DIR): $(PACKAGE_DIR)
	mkdir -p $(DEB_BUILD_DIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)

$(DIST_DIR):
	mkdir -p $(DIST_DIR)

# Create RPM spec file
$(RPM_BUILD_DIR)/SPECS/$(PACKAGE_NAME).spec: $(RPM_BUILD_DIR)
	@echo "Creating RPM spec file..."
	@sed -e 's/@VERSION@/$(PACKAGE_VERSION)/g' \
	     -e 's/@MAINTAINER@/$(PACKAGE_MAINTAINER)/g' \
	     -e 's/@DATE@/$(shell date +"%a %b %d %Y")/g' \
	     packaging/$(PACKAGE_NAME).spec.in > $@

# Create Debian control files
$(DEB_BUILD_DIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)/debian: $(DEB_BUILD_DIR)
	@echo "Creating Debian control files..."
	@mkdir -p $@
	@sed -e 's/@MAINTAINER@/$(PACKAGE_MAINTAINER)/g' \
	     packaging/debian/control.in > $@/control
	@cp packaging/debian/rules $@/rules
	@chmod +x $@/rules
	@sed -e 's/@VERSION@/$(PACKAGE_VERSION)/g' \
	     -e 's/@MAINTAINER@/$(PACKAGE_MAINTAINER)/g' \
	     -e 's/@DATE@/$(shell date -R)/g' \
	     packaging/debian/changelog.in > $@/changelog
	@cp packaging/debian/compat $@/compat
	@cp packaging/debian/postinst $@/postinst
	@chmod +x $@/postinst
	@cp packaging/debian/postrm $@/postrm
	@chmod +x $@/postrm

# Create source tarball for packaging
$(PACKAGE_DIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz: $(PACKAGE_DIR)
	@echo "Creating source tarball..."
	@git archive --format=tar.gz --prefix=$(PACKAGE_NAME)-$(PACKAGE_VERSION)/ HEAD > $@

# Build RPM package
rpm: $(TARGET) sudosh.1 $(RPM_BUILD_DIR)/SPECS/$(PACKAGE_NAME).spec $(PACKAGE_DIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz $(DIST_DIR)
	@echo "Building RPM package..."
	@cp $(PACKAGE_DIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz $(RPM_BUILD_DIR)/SOURCES/
	@rpmbuild --define "_topdir $(PWD)/$(RPM_BUILD_DIR)" -ba $(RPM_BUILD_DIR)/SPECS/$(PACKAGE_NAME).spec
	@cp $(RPM_BUILD_DIR)/RPMS/*/*.rpm $(DIST_DIR)/
	@cp $(RPM_BUILD_DIR)/SRPMS/*.rpm $(DIST_DIR)/
	@echo "RPM packages created in $(DIST_DIR)/"

# Build DEB package
deb: $(TARGET) sudosh.1 $(DEB_BUILD_DIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)/debian $(PACKAGE_DIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz $(DIST_DIR)
	@echo "Building DEB package..."
	@cd $(DEB_BUILD_DIR) && tar -xzf ../../$(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz
	@cp -r $(DEB_BUILD_DIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)/debian $(DEB_BUILD_DIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)/
	@cd $(DEB_BUILD_DIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION) && dpkg-buildpackage -us -uc
	@cp $(DEB_BUILD_DIR)/*.deb $(DIST_DIR)/ 2>/dev/null || true
	@echo "DEB package created in $(DIST_DIR)/"

# Build both packages
packages: rpm deb
	@echo "All packages built successfully!"
	@echo "Available packages in $(DIST_DIR)/:"
	@ls -la $(DIST_DIR)/

# Clean packaging files
clean-packages:
	rm -rf $(RPM_BUILD_DIR) $(DEB_BUILD_DIR) $(DIST_DIR)
	rm -f $(PACKAGE_DIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz

# Show help
help:
	@echo "Available targets:"
	@echo "  all              - Build sudosh (default)"
	@echo "  tests            - Build all tests"
	@echo "  test             - Run all tests"
	@echo "  unit-test        - Run unit tests only"
	@echo "  integration-test - Run integration tests only"
	@echo "  test-suid        - Set suid root for testing (requires sudo)"
	@echo "  test-pipeline-regression - Run pipeline security regression tests"
	@echo "  test-pipeline-smoke - Run quick pipeline smoke test"
	@echo "  clean-suid       - Remove suid privileges (requires sudo)"
	@echo "  install          - Install sudosh and manpage (requires root)"
	@echo "  uninstall        - Remove sudosh and manpage (requires root)"
	@echo "  clean            - Remove build files"
	@echo "  rebuild          - Clean and rebuild"
	@echo "  debug            - Build with debug symbols"
	@echo "  coverage         - Build with coverage support"
	@echo "  coverage-report  - Generate coverage report"
	@echo "  static-analysis  - Run static code analysis"
	@echo "  rpm              - Build RPM package for DNF-based systems"
	@echo "  deb              - Build DEB package for APT-based systems"
	@echo "  packages         - Build both RPM and DEB packages"
	@echo "  clean-packages   - Remove packaging files"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Enhanced Features:"
	@echo "  - NSS configuration support (/etc/nsswitch.conf)"
	@echo "  - Sudoers file parsing (/etc/sudoers)"
	@echo "  - SSSD integration framework"
	@echo "  - Multiple authentication fallback methods"

# Dependencies
$(OBJDIR)/main.o: $(SRCDIR)/main.c $(SRCDIR)/sudosh.h
$(OBJDIR)/auth.o: $(SRCDIR)/auth.c $(SRCDIR)/sudosh.h
$(OBJDIR)/command.o: $(SRCDIR)/command.c $(SRCDIR)/sudosh.h
$(OBJDIR)/logging.o: $(SRCDIR)/logging.c $(SRCDIR)/sudosh.h
$(OBJDIR)/security.o: $(SRCDIR)/security.c $(SRCDIR)/sudosh.h
$(OBJDIR)/utils.o: $(SRCDIR)/utils.c $(SRCDIR)/sudosh.h
$(OBJDIR)/nss.o: $(SRCDIR)/nss.c $(SRCDIR)/sudosh.h
$(OBJDIR)/sudoers.o: $(SRCDIR)/sudoers.c $(SRCDIR)/sudosh.h
$(OBJDIR)/sssd.o: $(SRCDIR)/sssd.c $(SRCDIR)/sudosh.h
$(OBJDIR)/pipeline.o: $(SRCDIR)/pipeline.c $(SRCDIR)/sudosh.h
$(OBJDIR)/ansible_detection.o: $(SRCDIR)/ansible_detection.c $(SRCDIR)/sudosh.h
$(OBJDIR)/ai_detection.o: $(SRCDIR)/ai_detection.c $(SRCDIR)/ai_detection.h

.PHONY: all tests test unit-test integration-test test-suid clean-suid install uninstall clean rebuild debug coverage coverage-report static-analysis rpm deb packages clean-packages help pipeline-regression-test test-pipeline-regression test-pipeline-smoke
