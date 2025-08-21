#!/bin/bash

# Simple validation script to test that our enhancement test framework works
# This script compiles and runs a basic test to verify the framework

set -e

echo "Validating Enhancement Test Framework"
echo "===================================="

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Check if sudosh is built
if [ ! -f "bin/sudosh" ]; then
    echo -e "${YELLOW}Building sudosh first...${NC}"
    make clean && make
fi

# Test compilation of each test file
echo ""
echo "Testing compilation of enhancement tests..."

# Compilation flags
CFLAGS="-Wall -Wextra -std=c99 -O2 -I./src -I./tests"
LDFLAGS="-lpam"

# Get object files (excluding main.o)
OBJ_FILES=""
for obj in obj/*.o; do
    if [[ "$(basename "$obj")" != "main.o" ]]; then
        OBJ_FILES="$OBJ_FILES $obj"
    fi
done

# Test files to validate
TEST_FILES=(
    "tests/unit/test_nss_enhancements.c"
    "tests/unit/test_pipeline_security.c"
    "tests/unit/test_text_processing_redirection.c"
    "tests/security/test_security_enhancements.c"
    "tests/integration/test_enhancement_integration.c"
)

COMPILE_SUCCESS=0
COMPILE_TOTAL=0

for test_file in "${TEST_FILES[@]}"; do
    if [ -f "$test_file" ]; then
        echo -n "Compiling $(basename "$test_file")... "
        COMPILE_TOTAL=$((COMPILE_TOTAL + 1))
        
        test_binary="test_validation_$(basename "$test_file" .c)"
        
        if gcc $CFLAGS "$test_file" tests/support/test_globals.c $OBJ_FILES -o "$test_binary" $LDFLAGS 2>/dev/null; then
            echo -e "${GREEN}OK${NC}"
            COMPILE_SUCCESS=$((COMPILE_SUCCESS + 1))
            
            # Clean up
            rm -f "$test_binary"
        else
            echo -e "${RED}FAILED${NC}"
        fi
    else
        echo -e "${YELLOW}Warning: $test_file not found${NC}"
    fi
done

echo ""
echo "Compilation Results:"
echo "  Total: $COMPILE_TOTAL"
echo "  Success: $COMPILE_SUCCESS"
echo "  Failed: $((COMPILE_TOTAL - COMPILE_SUCCESS))"

# Test the test runner script
echo ""
echo "Testing test runner script..."

if [ -f "tests/run_enhancement_tests.sh" ]; then
    if [ -x "tests/run_enhancement_tests.sh" ]; then
        echo -e "${GREEN}Test runner script is executable${NC}"
    else
        echo -e "${YELLOW}Making test runner script executable...${NC}"
        chmod +x tests/run_enhancement_tests.sh
    fi
    
    # Test help option
    echo -n "Testing help option... "
    if tests/run_enhancement_tests.sh --help >/dev/null 2>&1; then
        echo -e "${GREEN}OK${NC}"
    else
        echo -e "${RED}FAILED${NC}"
    fi
else
    echo -e "${RED}Test runner script not found${NC}"
fi

# Test Makefile targets
echo ""
echo "Testing Makefile targets..."

echo -n "Checking test-enhancements target... "
if grep -q "test-enhancements:" Makefile; then
    echo -e "${GREEN}OK${NC}"
else
    echo -e "${RED}MISSING${NC}"
fi

echo -n "Checking test-all target... "
if grep -q "test-all:" Makefile; then
    echo -e "${GREEN}OK${NC}"
else
    echo -e "${RED}MISSING${NC}"
fi

# Test basic function availability
echo ""
echo "Testing basic function availability..."

# Create a simple test to check if our new functions are available
cat > test_function_check.c << 'EOF'
#include "sudosh.h"

int main() {
    // Test NSS functions
    struct user_info *user = get_user_info_files("root");
    if (user) free_user_info(user);
    
    // Test pipeline functions
    int result = validate_secure_pipeline("ls | grep test");
    (void)result;
    
    // Test text processing functions
    int is_text = is_text_processing_command("grep");
    (void)is_text;
    
    // Test redirection functions
    int is_safe = is_safe_redirection_target("/tmp/test.txt");
    (void)is_safe;
    
    return 0;
}
EOF

echo -n "Testing function availability... "
if gcc $CFLAGS test_function_check.c tests/support/test_globals.c $OBJ_FILES -o test_function_check $LDFLAGS 2>/dev/null; then
    echo -e "${GREEN}OK${NC}"
    rm -f test_function_check
else
    echo -e "${RED}FAILED${NC}"
fi

rm -f test_function_check.c

# Summary
echo ""
echo "Validation Summary:"
echo "=================="

if [ $COMPILE_SUCCESS -eq $COMPILE_TOTAL ]; then
    echo -e "${GREEN}✓ All test files compile successfully${NC}"
else
    echo -e "${RED}✗ Some test files failed to compile${NC}"
fi

if [ -x "tests/run_enhancement_tests.sh" ]; then
    echo -e "${GREEN}✓ Test runner script is ready${NC}"
else
    echo -e "${RED}✗ Test runner script has issues${NC}"
fi

if grep -q "test-enhancements:" Makefile && grep -q "test-all:" Makefile; then
    echo -e "${GREEN}✓ Makefile targets are available${NC}"
else
    echo -e "${RED}✗ Makefile targets are missing${NC}"
fi

echo ""
if [ $COMPILE_SUCCESS -eq $COMPILE_TOTAL ]; then
    echo -e "${GREEN}Enhancement test framework is ready!${NC}"
    echo ""
    echo "To run the tests:"
    echo "  make test-enhancements    # Run enhancement tests only"
    echo "  make test-all            # Run all tests"
    echo "  cd tests && ./run_enhancement_tests.sh  # Detailed test run"
    exit 0
else
    echo -e "${RED}Enhancement test framework has issues. Please check the compilation errors.${NC}"
    exit 1
fi
