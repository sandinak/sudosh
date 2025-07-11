/**
 * test_color_functionality.c - Test color support functionality
 *
 * Tests for the new color support features in sudosh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "sudosh.h"
#include "test_framework.h"

/* Global verbose flag for testing */
int verbose_mode = 0;

/* Test color detection */
void test_detect_terminal_colors(void) {
    printf("Running test_detect_terminal_colors... ");
    
    /* Save original environment */
    char *orig_term = getenv("TERM");
    char *orig_colorterm = getenv("COLORTERM");
    
    /* Test with color-capable terminal */
    setenv("TERM", "xterm-256color", 1);
    setenv("COLORTERM", "truecolor", 1);
    
    int result = detect_terminal_colors();
    
    /* Should detect colors if we're in a TTY */
    if (isatty(STDOUT_FILENO)) {
        assert(result == 1);
    }
    
    /* Test with non-color terminal */
    setenv("TERM", "dumb", 1);
    unsetenv("COLORTERM");
    
    result = detect_terminal_colors();
    /* Should not detect colors for dumb terminal */
    assert(result == 0);
    
    /* Restore original environment */
    if (orig_term) {
        setenv("TERM", orig_term, 1);
    } else {
        unsetenv("TERM");
    }
    
    if (orig_colorterm) {
        setenv("COLORTERM", orig_colorterm, 1);
    } else {
        unsetenv("COLORTERM");
    }
    
    printf("PASS\n");
}

/* Test color configuration initialization */
void test_init_color_config(void) {
    printf("Running test_init_color_config... ");
    
    struct color_config *config = init_color_config();
    assert(config != NULL);
    
    /* Check that default colors are set */
    assert(strlen(config->username_color) > 0);
    assert(strlen(config->hostname_color) > 0);
    assert(strlen(config->path_color) > 0);
    assert(strlen(config->prompt_color) > 0);
    assert(strlen(config->reset_color) > 0);
    
    /* Check that colors_enabled is set based on terminal capability */
    /* This will be 0 or 1 depending on the terminal */
    assert(config->colors_enabled == 0 || config->colors_enabled == 1);
    
    free_color_config(config);
    
    printf("PASS\n");
}

/* Test PS1 color parsing */
void test_parse_ps1_colors(void) {
    printf("Running test_parse_ps1_colors... ");
    
    struct color_config *config = init_color_config();
    assert(config != NULL);
    
    /* Test with a standard colorful PS1 */
    const char *ps1 = "\\[\\033[01;32m\\]\\u\\[\\033[00m\\]@\\[\\033[01;34m\\]\\h\\[\\033[00m\\]:\\[\\033[01;36m\\]\\w\\[\\033[00m\\]\\$ ";
    
    int result = parse_ps1_colors(ps1, config);
    assert(result == 1); /* Should successfully parse colors */
    
    /* Check that colors were extracted */
    assert(strlen(config->username_color) > 0);
    assert(strlen(config->hostname_color) > 0);
    assert(strlen(config->path_color) > 0);
    
    /* Test with PS1 without colors */
    const char *plain_ps1 = "\\u@\\h:\\w\\$ ";
    result = parse_ps1_colors(plain_ps1, config);
    assert(result == 0); /* Should not find colors */
    
    /* Test with NULL inputs */
    result = parse_ps1_colors(NULL, config);
    assert(result == 0);
    
    result = parse_ps1_colors(ps1, NULL);
    assert(result == 0);
    
    free_color_config(config);
    
    printf("PASS\n");
}

/* Test environment variable preservation */
void test_preserve_color_environment(void) {
    printf("Running test_preserve_color_environment... ");
    
    /* Set up test environment variables */
    setenv("PS1", "\\[\\033[32m\\]\\u@\\h\\[\\033[0m\\]:\\w\\$ ", 1);
    setenv("TERM", "xterm-256color", 1);
    setenv("COLORTERM", "truecolor", 1);
    setenv("LS_COLORS", "di=01;34:ln=01;36", 1);
    
    /* Save the values */
    char *orig_ps1 = strdup(getenv("PS1"));
    char *orig_term = strdup(getenv("TERM"));
    char *orig_colorterm = strdup(getenv("COLORTERM"));
    char *orig_ls_colors = strdup(getenv("LS_COLORS"));
    
    /* Call preserve function */
    preserve_color_environment();
    
    /* Simulate environment sanitization by unsetting variables */
    unsetenv("PS1");
    unsetenv("TERM");
    unsetenv("COLORTERM");
    unsetenv("LS_COLORS");
    
    /* Call preserve function again to restore */
    preserve_color_environment();
    
    /* Check that variables are restored */
    assert(getenv("PS1") != NULL);
    assert(getenv("TERM") != NULL);
    assert(getenv("COLORTERM") != NULL);
    assert(getenv("LS_COLORS") != NULL);
    
    /* Verify values are correct */
    assert(strcmp(getenv("PS1"), orig_ps1) == 0);
    assert(strcmp(getenv("TERM"), orig_term) == 0);
    assert(strcmp(getenv("COLORTERM"), orig_colorterm) == 0);
    assert(strcmp(getenv("LS_COLORS"), orig_ls_colors) == 0);
    
    /* Cleanup */
    free(orig_ps1);
    free(orig_term);
    free(orig_colorterm);
    free(orig_ls_colors);
    
    printf("PASS\n");
}

/* Test color configuration cleanup */
void test_cleanup_color_config(void) {
    printf("Running test_cleanup_color_config... ");
    
    /* This test mainly ensures the function doesn't crash */
    cleanup_color_config(); /* Should handle NULL gracefully */
    
    /* Test with actual config */
    struct color_config *config = init_color_config();
    assert(config != NULL);
    
    free_color_config(config);
    
    printf("PASS\n");
}

int main(void) {
    printf("=== Color Functionality Tests ===\n");
    
    test_detect_terminal_colors();
    test_init_color_config();
    test_parse_ps1_colors();
    test_preserve_color_environment();
    test_cleanup_color_config();
    
    printf("\n=== Test Results ===\n");
    printf("Total tests: 5\n");
    printf("Passed: 5\n");
    printf("Failed: 0\n");
    printf("All color functionality tests passed!\n");
    
    return 0;
}
