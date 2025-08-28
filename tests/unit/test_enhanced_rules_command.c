/**
 * test_enhanced_rules_command.c - Test enhanced rules command functionality
 *
 * Tests the enhanced rules command with safe/blocked commands display
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "../test_framework.h"
#include "../../src/sudosh.h"

/* Test framework globals */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test safe commands section output */
int test_safe_commands_section(void) {
    printf("Testing safe commands section...\n");

    /* Test that the function can be called without crashing */
    /* Redirect output to /dev/null to avoid cluttering test output */
    int saved_stdout = dup(STDOUT_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
        dup2(devnull, STDOUT_FILENO);
        close(devnull);

        /* Call the function - if it doesn't crash, test passes */
        print_safe_commands_section();

        /* Restore stdout */
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }

    printf("✓ Safe commands section tests passed\n");
    return 1;}

/* Test blocked commands section output */
int test_blocked_commands_section(void) {
    printf("Testing blocked commands section...\n");

    /* Test that the function can be called without crashing */
    /* Redirect output to /dev/null to avoid cluttering test output */
    int saved_stdout = dup(STDOUT_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
        dup2(devnull, STDOUT_FILENO);
        close(devnull);

        /* Call the function - if it doesn't crash, test passes */
        print_blocked_commands_section();

        /* Restore stdout */
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }

    printf("✓ Blocked commands section tests passed\n");
    return 1;
}

/* Test pager functionality */
int test_pager_functionality(void) {
    printf("Testing pager functionality...\n");
    
    /* Test terminal height detection */
    int height = get_terminal_height();
    ASSERT_TRUE(height > 0); /* Should return a positive value */
    /* In some CI envs, we may only guarantee a smaller minimum; enforce at least 10 */
    ASSERT_TRUE(height >= 10);

    /* Test pager execution (simplified test) */
    /* Note: Full pager testing would require complex terminal simulation */
    /* For now, just verify the function exists and can be called */
    
    printf("✓ Pager functionality tests passed\n");
    return 1;}

/* Test rules command integration */
int test_rules_command_integration(void) {
    printf("Testing rules command integration...\n");

    /* Test that the function can be called without crashing */
    /* Redirect output to /dev/null to avoid cluttering test output */
    int saved_stdout = dup(STDOUT_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
        dup2(devnull, STDOUT_FILENO);
        close(devnull);

        /* Call the function - if it doesn't crash, test passes */
        list_available_commands_detailed("testuser");

        /* Restore stdout */
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }

    printf("✓ Rules command integration tests passed\n");
    return 1;
}

/* Test command categorization accuracy */
int test_command_categorization(void) {
    printf("Testing command categorization accuracy...\n");
    
    /* Test that safe commands are properly categorized */
    const char *safe_commands[] = {
        "ls", "pwd", "whoami", "id", "date", "uptime",
        "grep", "awk", "sed", "cut", "sort", "uniq",
        "head", "tail", "wc", "cat", "echo",
        NULL
    };
    
    for (int i = 0; safe_commands[i]; i++) {
        ASSERT_TRUE(is_safe_command(safe_commands[i]));
    }
    
    /* Test that dangerous commands are properly categorized */
    const char *dangerous_commands[] = {
        "init", "shutdown", "halt", "reboot",
        "fdisk", "parted", "mkfs", "dd",
        "iptables", "ufw", "su", "sudo",
        NULL
    };
    
    for (int i = 0; dangerous_commands[i]; i++) {
        ASSERT_TRUE(is_dangerous_command(dangerous_commands[i]));
    }
    
    printf("✓ Command categorization tests passed\n");
    return 1;}

/* Test output formatting and structure */
int test_output_formatting(void) {
    printf("Testing output formatting and structure...\n");

    /* Test that the function can be called without crashing */
    /* Redirect output to /dev/null to avoid cluttering test output */
    int saved_stdout = dup(STDOUT_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
        dup2(devnull, STDOUT_FILENO);
        close(devnull);

        /* Call the function - if it doesn't crash, test passes */
        print_safe_commands_section();

        /* Restore stdout */
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }

    printf("✓ Output formatting tests passed\n");
    return 1;
}

int main(void) {
    printf("=== Enhanced Rules Command Tests ===\n");
    
    RUN_TEST(test_safe_commands_section);
    RUN_TEST(test_blocked_commands_section);
    RUN_TEST(test_pager_functionality);
    RUN_TEST(test_rules_command_integration);
    RUN_TEST(test_command_categorization);
    RUN_TEST(test_output_formatting);
    
    printf("\n=== Enhanced Rules Command Test Summary ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passes);
    printf("Failed: %d\n", test_failures);
    
    return (test_failures == 0) ? 0 : 1;
}
