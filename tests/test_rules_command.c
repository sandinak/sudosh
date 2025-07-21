/**
 * test_rules_command.c - Test the new 'rules' command and enhanced error messages
 *
 * This test validates that the new 'rules' built-in command works correctly
 * and that the enhanced error messages for redirects and pipes are displayed.
 */

#include "../src/sudosh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* Test mode flag */
extern int test_mode;

/**
 * Test the 'rules' built-in command
 */
int test_rules_command() {
    printf("Testing 'rules' built-in command...\n");
    
    /* Test that the rules command is recognized as a built-in */
    int result = handle_builtin_command("rules");
    if (result != 1) {
        printf("FAIL: 'rules' command not recognized as built-in (returned %d)\n", result);
        return 1;
    }
    
    printf("  'rules' command recognized as built-in: PASS\n");
    
    /* Test that rules command with arguments is still handled */
    result = handle_builtin_command("rules -v");
    if (result != 1) {
        printf("FAIL: 'rules' command with arguments not handled properly\n");
        return 1;
    }
    
    printf("  'rules' command with arguments handled: PASS\n");
    
    return 0;
}

/**
 * Test enhanced redirect error messages
 */
int test_redirect_error_messages() {
    printf("Testing enhanced redirect error messages...\n");

    /* Set current username for security logging */
    set_current_username("testuser");

    /* Capture stderr to test error messages */
    FILE *original_stderr = stderr;
    char error_buffer[2048];
    memset(error_buffer, 0, sizeof(error_buffer));
    FILE *error_stream = fmemopen(error_buffer, sizeof(error_buffer), "w");
    if (!error_stream) {
        printf("FAIL: Could not create error stream for testing\n");
        return 1;
    }

    stderr = error_stream;

    /* Test redirect validation - note the spaces around > are required */
    int result = validate_command("echo test > /tmp/file");

    /* Flush the stream to ensure all output is captured */
    fflush(error_stream);

    /* Restore stderr */
    fclose(error_stream);
    stderr = original_stderr;

    /* Check that command was rejected */
    if (result != 0) {
        printf("FAIL: Redirect command should have been rejected (got %d)\n", result);
        return 1;
    }

    /* Check that enhanced error message is present */
    if (!strstr(error_buffer, "I/O redirection is blocked for security reasons") ||
        !strstr(error_buffer, "overwrite critical system files") ||
        !strstr(error_buffer, "bypass file permissions")) {
        printf("FAIL: Enhanced redirect error message not found\n");
        printf("Error buffer: '%s'\n", error_buffer);
        return 1;
    }
    
    printf("  Enhanced redirect error message: PASS\n");
    
    return 0;
}

/**
 * Test enhanced pipe error messages
 */
int test_pipe_error_messages() {
    printf("Testing enhanced pipe error messages...\n");

    /* Set current username for security logging */
    set_current_username("testuser");

    /* Capture stderr to test error messages */
    FILE *original_stderr = stderr;
    char error_buffer[2048];
    memset(error_buffer, 0, sizeof(error_buffer));
    FILE *error_stream = fmemopen(error_buffer, sizeof(error_buffer), "w");
    if (!error_stream) {
        printf("FAIL: Could not create error stream for testing\n");
        return 1;
    }

    stderr = error_stream;

    /* Test pipe validation */
    int result = validate_command("ls | grep test");

    /* Restore stderr */
    fclose(error_stream);
    stderr = original_stderr;
    
    /* Check that command was rejected */
    if (result != 0) {
        printf("FAIL: Pipe command should have been rejected\n");
        return 1;
    }
    
    /* Check that enhanced error message is present */
    if (!strstr(error_buffer, "pipes (|) are blocked for security reasons") ||
        !strstr(error_buffer, "chain commands and bypass security controls") ||
        !strstr(error_buffer, "run commands individually")) {
        printf("FAIL: Enhanced pipe error message not found\n");
        printf("Error buffer: %s\n", error_buffer);
        return 1;
    }
    
    printf("  Enhanced pipe error message: PASS\n");
    
    return 0;
}

/**
 * Test help command includes 'rules'
 */
int test_help_includes_rules() {
    printf("Testing help command includes 'rules'...\n");
    
    /* Capture stdout to test help output */
    FILE *original_stdout = stdout;
    char help_buffer[4096];
    FILE *help_stream = fmemopen(help_buffer, sizeof(help_buffer), "w");
    if (!help_stream) {
        printf("FAIL: Could not create help stream for testing\n");
        return 1;
    }
    
    stdout = help_stream;
    
    /* Call print_help function */
    print_help();
    
    /* Restore stdout */
    fclose(help_stream);
    stdout = original_stdout;
    
    /* Check that 'rules' is mentioned in help */
    if (!strstr(help_buffer, "rules")) {
        printf("FAIL: 'rules' command not found in help output\n");
        printf("Help buffer: %s\n", help_buffer);
        return 1;
    }
    
    printf("  'rules' command found in help: PASS\n");
    
    return 0;
}

/**
 * Test commands list includes 'rules'
 */
int test_commands_includes_rules() {
    printf("Testing commands list includes 'rules'...\n");
    
    /* Capture stdout to test commands output */
    FILE *original_stdout = stdout;
    char commands_buffer[4096];
    FILE *commands_stream = fmemopen(commands_buffer, sizeof(commands_buffer), "w");
    if (!commands_stream) {
        printf("FAIL: Could not create commands stream for testing\n");
        return 1;
    }
    
    stdout = commands_stream;
    
    /* Call print_commands function */
    print_commands();
    
    /* Restore stdout */
    fclose(commands_stream);
    stdout = original_stdout;
    
    /* Check that 'rules' is mentioned in commands list */
    if (!strstr(commands_buffer, "rules")) {
        printf("FAIL: 'rules' command not found in commands output\n");
        printf("Commands buffer: %s\n", commands_buffer);
        return 1;
    }
    
    printf("  'rules' command found in commands list: PASS\n");
    
    return 0;
}

int main() {
    /* Enable test mode to avoid interactive prompts */
    test_mode = 1;
    
    printf("=== Rules Command and Enhanced Error Messages Tests ===\n");
    
    int failures = 0;
    
    /* Run all tests */
    failures += test_rules_command();
    failures += test_redirect_error_messages();
    failures += test_pipe_error_messages();
    failures += test_help_includes_rules();
    failures += test_commands_includes_rules();
    
    /* Summary */
    printf("\n=== Test Results ===\n");
    if (failures == 0) {
        printf("✅ All rules command and error message tests PASSED!\n");
        printf("New features are working correctly.\n");
        return 0;
    } else {
        printf("❌ %d test(s) FAILED!\n", failures);
        printf("New features need fixes.\n");
        return 1;
    }
}
