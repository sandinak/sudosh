/**
 * test_awk_sed_support.c - Test awk/sed support functionality
 *
 * Tests the enhanced awk/sed support with field references, quotes, and redirection
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../test_framework.h"
#include "../../src/sudosh.h"

/* Test framework globals */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Simple test to verify awk/sed functionality */
int test_basic_functionality(void) {
    printf("Testing basic awk/sed functionality...\n");
    
    /* Test that awk and sed are recognized as text processing commands */
    ASSERT_TRUE(is_text_processing_command("awk"));
    ASSERT_TRUE(is_text_processing_command("sed"));
    ASSERT_TRUE(is_text_processing_command("grep"));
    
    /* Test basic text processing validation */
    ASSERT_TRUE(validate_text_processing_command("awk '{print $1}'"));
    ASSERT_TRUE(validate_text_processing_command("sed 's/old/new/'"));
    ASSERT_TRUE(validate_text_processing_command("grep pattern"));
    
    /* Test that dangerous operations are blocked */
    ASSERT_FALSE(validate_text_processing_command("awk 'BEGIN{system(\"ls\")}'"));
    ASSERT_FALSE(validate_text_processing_command("sed 'e ls'"));
    
    printf("✓ Basic awk/sed functionality tests passed\n");
    return 1;
}

int main(void) {
    printf("=== Awk/Sed Support Tests ===\n");
    
    RUN_TEST(test_basic_functionality);
    
    printf("\n=== Awk/Sed Support Test Summary ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passes);
    printf("Failed: %d\n", test_failures);
    
    if (test_failures == 0) {
        printf("✅ All awk/sed support tests passed!\n");
    } else {
        printf("❌ Some awk/sed support tests failed\n");
    }
    
    return (test_failures == 0) ? 0 : 1;
}
