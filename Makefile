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
    # Other Unix systems
    CFLAGS += -D_GNU_SOURCE
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
SOURCES = main.c auth.c command.c logging.c security.c utils.c nss.c sudoers.c sssd.c
OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)

# Test files
TEST_SOURCES = $(wildcard $(TESTDIR)/test_*.c)
TEST_OBJECTS = $(TEST_SOURCES:$(TESTDIR)/%.c=$(OBJDIR)/$(TESTDIR)/%.o)
TEST_TARGETS = $(TEST_SOURCES:$(TESTDIR)/test_%.c=$(BINDIR)/test_%)

# Security test files
SECURITY_TEST_SOURCES = $(wildcard $(TESTDIR)/test_security_*.c)
SECURITY_TEST_BINARIES = $(SECURITY_TEST_SOURCES:$(TESTDIR)/%.c=$(BINDIR)/%)

# Library objects (excluding main.c for testing)
LIB_SOURCES = auth.c command.c logging.c security.c utils.c nss.c sudoers.c sssd.c
LIB_OBJECTS = $(LIB_SOURCES:%.c=$(OBJDIR)/%.o)

# Target executable
TARGET = $(BINDIR)/sudosh

# Default target
all: $(TARGET)

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

# Compile test files
$(OBJDIR)/$(TESTDIR)/%.o: $(TESTDIR)/%.c | $(OBJDIR)
	@mkdir -p $(OBJDIR)/$(TESTDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -c $< -o $@

# Ensure test directory exists
$(OBJDIR)/$(TESTDIR):
	@mkdir -p $(OBJDIR)/$(TESTDIR)

# Build individual test executables
$(BINDIR)/test_%: $(OBJDIR)/$(TESTDIR)/test_%.o $(LIB_OBJECTS) | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

# Build all tests
tests: $(LIB_OBJECTS) | $(OBJDIR)/$(TESTDIR)
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

# Install target (requires root privileges)
install: $(TARGET) sudosh.1
	@echo "Installing sudosh..."
	@if [ "$(shell id -u)" != "0" ]; then \
		echo "Error: Installation requires root privileges. Run with sudo."; \
		exit 1; \
	fi
	install -d $(BINDIR_INSTALL)
	install -d $(MANDIR)
	install -m 4755 $(TARGET) $(BINDIR_INSTALL)/sudosh
	install -m 644 sudosh.1 $(MANDIR)/sudosh.1
	@echo "sudosh installed to $(BINDIR_INSTALL)/sudosh"
	@echo "Manual page installed to $(MANDIR)/sudosh.1"
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
	@echo "sudosh removed from $(BINDIR_INSTALL)"
	@echo "Manual page removed from $(MANDIR)"

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

# Show help
help:
	@echo "Available targets:"
	@echo "  all              - Build sudosh (default)"
	@echo "  tests            - Build all tests"
	@echo "  test             - Run all tests"
	@echo "  unit-test        - Run unit tests only"
	@echo "  integration-test - Run integration tests only"
	@echo "  test-suid        - Set suid root for testing (requires sudo)"
	@echo "  clean-suid       - Remove suid privileges (requires sudo)"
	@echo "  install          - Install sudosh and manpage (requires root)"
	@echo "  uninstall        - Remove sudosh and manpage (requires root)"
	@echo "  clean            - Remove build files"
	@echo "  rebuild          - Clean and rebuild"
	@echo "  debug            - Build with debug symbols"
	@echo "  coverage         - Build with coverage support"
	@echo "  coverage-report  - Generate coverage report"
	@echo "  static-analysis  - Run static code analysis"
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

.PHONY: all tests test unit-test integration-test test-suid clean-suid install uninstall clean rebuild debug coverage coverage-report static-analysis help
