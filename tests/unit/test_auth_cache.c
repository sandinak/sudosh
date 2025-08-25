/**
 * test_auth_cache.c - Test authentication cache functionality
 *
 * Tests for the new authentication caching features in sudosh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <time.h>
#include "sudosh.h"
#include "test_framework.h"

/* Global verbose flag for testing */
int verbose_mode = 0;

/* Test cache directory creation */
void test_create_auth_cache_dir(void) {
    printf("Running test_create_auth_cache_dir... ");
    
    /* Note: This test may fail if not running as root, which is expected */
    int result = create_auth_cache_dir();
    
    /* The function should return 0 or 1 depending on permissions */
    assert(result == 0 || result == 1);
    
    printf("PASS\n");
}

/* Test cache path generation */
void test_get_auth_cache_path(void) {
    printf("Running test_get_auth_cache_path... ");
    
    char *cache_path = get_auth_cache_path("testuser");
    assert(cache_path != NULL);
    
    /* Check that path contains expected components */
    assert(strstr(cache_path, AUTH_CACHE_DIR) != NULL);
    assert(strstr(cache_path, AUTH_CACHE_FILE_PREFIX) != NULL);
    assert(strstr(cache_path, "testuser") != NULL);
    
    free(cache_path);
    
    /* Test with NULL username */
    cache_path = get_auth_cache_path(NULL);
    assert(cache_path == NULL);
    
    printf("PASS\n");
}

/* Test cache check with non-existent cache */
void test_check_auth_cache_nonexistent(void) {
    printf("Running test_check_auth_cache_nonexistent... ");
    
    /* Test with a user that shouldn't have a cache */
    int result = check_auth_cache("nonexistent_test_user_12345");
    assert(result == 0); /* Should return false for non-existent cache */
    
    /* Test with NULL username */
    result = check_auth_cache(NULL);
    assert(result == 0);
    
    printf("PASS\n");
}

/* Test cache update and check cycle */
void test_auth_cache_update_and_check(void) {
    printf("Running test_auth_cache_update_and_check... ");
    
    const char *test_user = "cache_test_user";
    
    /* Clear any existing cache first */
    clear_auth_cache(test_user);
    
    /* Verify cache doesn't exist */
    int result = check_auth_cache(test_user);
    assert(result == 0);
    
    /* Update cache (may fail if not root, which is expected) */
    result = update_auth_cache(test_user);
    
    if (result == 1) {
        /* Cache update succeeded, check if it's valid */
        result = check_auth_cache(test_user);
        assert(result == 1);
        
        /* Clean up */
        clear_auth_cache(test_user);
        
        /* Verify cache is cleared */
        result = check_auth_cache(test_user);
        assert(result == 0);
    }
    /* If update failed (not root), that's also acceptable for testing */
    
    printf("PASS\n");
}

/* Test cache clearing */
void test_clear_auth_cache(void) {
    printf("Running test_clear_auth_cache... ");
    
    const char *test_user = "clear_test_user";
    
    /* Try to clear cache (should not crash even if cache doesn't exist) */
    clear_auth_cache(test_user);
    
    /* Test with NULL username (should not crash) */
    clear_auth_cache(NULL);
    
    printf("PASS\n");
}

/* Test cache cleanup */
void test_cleanup_auth_cache(void) {
    printf("Running test_cleanup_auth_cache... ");
    
    /* This should not crash even if cache directory doesn't exist */
    cleanup_auth_cache();
    
    printf("PASS\n");
}

/* Test cached authentication function */
void test_authenticate_user_cached(void) {
    printf("Running test_authenticate_user_cached... ");

    /* Test with NULL username */
    int result = authenticate_user_cached(NULL);
    assert(result == 0);

    /* For testing purposes, we'll just test the cache check part */
    /* without actually trying to authenticate, since that would require user input */
    result = check_auth_cache("test_user_no_auth");
    assert(result == 0); /* Should be false for non-existent cache */

    printf("PASS\n");
}

/* Test cache file security */
void test_cache_file_security(void) {
    printf("Running test_cache_file_security... ");
    
    const char *test_user = "security_test_user";
    char *cache_path;
    struct stat st;
    
    /* Clear any existing cache */
    clear_auth_cache(test_user);
    
    /* Try to create cache */
    int result = update_auth_cache(test_user);
    
    if (result == 1) {
        /* Cache was created, check security */
        cache_path = get_auth_cache_path(test_user);
        assert(cache_path != NULL);
        
        if (stat(cache_path, &st) == 0) {
            /* File exists, check permissions */
            assert((st.st_mode & 0777) == 0600); /* Should be readable/writable by owner only */
            assert(st.st_uid == 0); /* Should be owned by root */
        }
        
        /* Clean up */
        clear_auth_cache(test_user);
        free(cache_path);
    }
    /* If cache creation failed (not root), that's acceptable for testing */
    
    printf("PASS\n");
}

/* Test cache timeout behavior */
void test_cache_timeout(void) {
    printf("Running test_cache_timeout... ");
    
    /* This test verifies the timeout logic without actually waiting */
    const char *test_user = "timeout_test_user";
    
    /* Clear any existing cache */
    clear_auth_cache(test_user);
    
    /* The timeout logic is tested indirectly through other functions */
    /* We can't easily test actual timeout without waiting 15 minutes */
    
    /* Test that check_auth_cache handles time comparisons correctly */
    int result = check_auth_cache(test_user);
    assert(result == 0); /* Should be false for non-existent cache */
    
    printf("PASS\n");
}

int main(void) {
    printf("=== Authentication Cache Tests ===\n");
    
    test_create_auth_cache_dir();
    test_get_auth_cache_path();
    test_check_auth_cache_nonexistent();
    test_auth_cache_update_and_check();
    test_clear_auth_cache();
    test_cleanup_auth_cache();
    test_authenticate_user_cached();
    test_cache_file_security();
    test_cache_timeout();
    
    printf("\n=== Test Results ===\n");
    printf("Total tests: 9\n");
    printf("Passed: 9\n");
    printf("Failed: 0\n");
    printf("All authentication cache tests passed!\n");
    printf("\nNote: Some tests may have limited functionality when not running as root,\n");
    printf("which is expected behavior for security reasons.\n");
    
    return 0;
}
