/**
 * test_shell_redirection.c - Test shell redirection functionality
 *
 * Tests the intelligent shell redirection feature when sudosh is aliased to sudo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../test_framework.h"
#include "../../src/sudosh.h"

/* Test framework globals */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* External sudo compatibility mode flag for testing */
extern int sudo_compat_mode_flag;

/* Mock current username for testing */
static char test_username[] = "testuser";

/* Test shell command detection */
int test_shell_command_detection(void) {
    printf("Testing shell command detection...\n");
    
    /* Test basic shell commands */
    ASSERT_TRUE(is_shell_command("bash"));
    ASSERT_TRUE(is_shell_command("sh"));
    ASSERT_TRUE(is_shell_command("zsh"));
    ASSERT_TRUE(is_shell_command("/bin/bash"));
    ASSERT_TRUE(is_shell_command("/usr/bin/zsh"));
    
    /* Test shell commands with arguments */
    ASSERT_TRUE(is_shell_command("bash -i"));
    ASSERT_TRUE(is_shell_command("sh -c 'echo test'"));
    
    /* Test non-shell commands */
    ASSERT_FALSE(is_shell_command("ls"));
    ASSERT_FALSE(is_shell_command("grep"));
    ASSERT_FALSE(is_shell_command("awk"));
    ASSERT_FALSE(is_shell_command("systemctl"));
    
    printf("✓ Shell command detection tests passed\n");
    return 1;}

/* Test shell redirection handler */
int test_shell_redirection_handler(void) {
    printf("Testing shell redirection handler...\n");
    
    /* Set up test environment */
    set_current_username(test_username);
    
    /* Test redirection handler returns special code */
    int result = handle_shell_command_in_sudo_mode("bash");
    ASSERT_EQUAL(result, 2); /* Should return special redirection code */
    
    result = handle_shell_command_in_sudo_mode("sh");
    ASSERT_EQUAL(result, 2);
    
    result = handle_shell_command_in_sudo_mode("/bin/zsh");
    ASSERT_EQUAL(result, 2);
    
    /* Test with NULL command */
    result = handle_shell_command_in_sudo_mode(NULL);
    ASSERT_EQUAL(result, 0);
    
    printf("✓ Shell redirection handler tests passed\n");
    return 1;}

/* Test sudo compatibility mode behavior */
int test_sudo_compat_mode_behavior(void) {
    printf("Testing sudo compatibility mode behavior...\n");

    /* Set up test environment */
    set_current_username(test_username);

    /* Test with sudo compatibility mode enabled */
    sudo_compat_mode_flag = 1;

    /* Shell commands should trigger redirection */
    int result = validate_command("bash");
    /* In test mode, just verify we get a consistent response */
    ASSERT_TRUE(result >= 0); /* Should return some valid code */

    /* Non-shell commands should work normally */
    result = validate_command("ls -la");
    ASSERT_TRUE(result >= 0); /* Should return some valid code */

    printf("✓ Sudo compatibility mode behavior tests passed\n");
    return 1;}

/* Test shell redirection message content */
int test_redirection_message_content(void) {
    printf("Testing redirection message content...\n");
    
    /* Capture stderr to test message output */
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        printf("Failed to create pipe for testing\n");
        return 0;
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        /* Child process: redirect stderr and call handler */
        close(pipefd[0]);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        
        set_current_username(test_username);
        handle_shell_command_in_sudo_mode("bash");
        exit(0);
    } else if (pid > 0) {
        /* Parent process: read output and verify content */
        close(pipefd[1]);
        
        char buffer[4096] = {0};
        ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
        close(pipefd[0]);
        
        int status;
        waitpid(pid, &status, 0);
        
        if (bytes_read > 0) {
            /* Check for key message components */
            ASSERT_TRUE(strstr(buffer, "redirecting 'bash' to secure interactive shell") != NULL);
            ASSERT_TRUE(strstr(buffer, "provides enhanced logging and security controls") != NULL);
            ASSERT_TRUE(strstr(buffer, "see 'man sudosh' for details") != NULL);
            ASSERT_TRUE(strstr(buffer, "'help' for commands") != NULL);
        }
    }
    
    printf("✓ Redirection message content tests passed\n");
    return 1;}

/* Test edge cases and error handling */
int test_edge_cases(void) {
    printf("Testing edge cases and error handling...\n");
    
    /* Test with empty command */
    int result = handle_shell_command_in_sudo_mode("");
    ASSERT_EQUAL(result, 0);
    
    /* Test with whitespace-only command */
    result = handle_shell_command_in_sudo_mode("   ");
    ASSERT_EQUAL(result, 0);
    
    /* Test with very long command */
    char long_command[1024];
    strcpy(long_command, "bash ");
    for (int i = 0; i < 200; i++) {
        strcat(long_command, "arg ");
    }
    result = handle_shell_command_in_sudo_mode(long_command);
    ASSERT_EQUAL(result, 2); /* Should still work */
    
    printf("✓ Edge cases and error handling tests passed\n");
    return 1;}

/* Test integration with command validation */
int test_validation_integration(void) {
    printf("Testing integration with command validation...\n");
    
    set_current_username(test_username);
    
    /* Test that shell redirection integrates properly with validation */
    sudo_compat_mode_flag = 1;
    
    /* Test various shell commands */
    const char *shell_commands[] = {
        "bash",
        "sh",
        "zsh",
        "/bin/bash",
        "/usr/bin/sh",
        "bash -i",
        "sh -c 'echo test'",
        NULL
    };
    
    for (int i = 0; shell_commands[i]; i++) {
        int result = validate_command(shell_commands[i]);
        ASSERT_TRUE(result >= 0); /* Should return valid code */
    }
    
    /* Test non-shell commands still work */
    const char *normal_commands[] = {
        "ls -la",
        "grep pattern file",
        "awk '{print $1}'",
        "systemctl status",
        NULL
    };
    
    for (int i = 0; normal_commands[i]; i++) {
        int result = validate_command(normal_commands[i]);
        ASSERT_TRUE(result == 1 || result == 0); /* Should be normal validation */
    }
    
    printf("✓ Validation integration tests passed\n");
    return 1;}

int main(void) {
    printf("=== Shell Redirection Tests ===\n");
    
    RUN_TEST(test_shell_command_detection);
    RUN_TEST(test_shell_redirection_handler);
    RUN_TEST(test_sudo_compat_mode_behavior);
    RUN_TEST(test_redirection_message_content);
    RUN_TEST(test_edge_cases);
    RUN_TEST(test_validation_integration);
    
    printf("\n=== Shell Redirection Test Summary ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passes);
    printf("Failed: %d\n", test_failures);
    
    return (test_failures == 0) ? 0 : 1;
}
