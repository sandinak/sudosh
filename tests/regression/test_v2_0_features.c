/**
 * test_v2_0_features.c - Comprehensive v2.0 feature regression tests
 *
 * Tests all major v2.0 features to ensure no regressions
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

/* External globals */
extern int sudo_compat_mode_flag;

/* Test shell redirection feature */
int test_shell_redirection_feature(void) {
    printf("Testing shell redirection feature...\n");
    
    /* Enable sudo compatibility mode */
    sudo_compat_mode_flag = 1;
    
    /* Test that shell commands trigger redirection */
    ASSERT_EQUAL(validate_command("bash"), 2);
    ASSERT_EQUAL(validate_command("sh"), 2);
    ASSERT_EQUAL(validate_command("/bin/zsh"), 2);
    ASSERT_EQUAL(validate_command("bash -i"), 2);
    
    /* Test that non-shell commands work normally */
    ASSERT_TRUE(validate_command("ls") != 2);
    ASSERT_TRUE(validate_command("grep") != 2);
    
    /* Disable sudo compatibility mode */
    sudo_compat_mode_flag = 0;
    
    /* Test that shell commands are blocked normally */
    ASSERT_EQUAL(validate_command("bash"), 0);
    ASSERT_EQUAL(validate_command("sh"), 0);
    
    printf("✓ Shell redirection feature tests passed\n");
    return 1;}

/* Test enhanced rules command */
int test_enhanced_rules_command(void) {
    printf("Testing enhanced rules command...\n");
    
    /* Test that safe commands section function exists and works */
    int pipefd[2];
    if (pipe(pipefd) == 0) {
        pid_t pid = fork();
        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            print_safe_commands_section();
            exit(0);
        } else if (pid > 0) {
            close(pipefd[1]);
            char buffer[4096] = {0};
            read(pipefd[0], buffer, sizeof(buffer) - 1);
            close(pipefd[0]);
            waitpid(pid, NULL, 0);
            
            ASSERT_TRUE(strstr(buffer, "Always Safe Commands") != NULL);
            ASSERT_TRUE(strstr(buffer, "Text Processing:") != NULL);
        }
    }
    
    /* Test blocked commands section */
    if (pipe(pipefd) == 0) {
        pid_t pid = fork();
        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            print_blocked_commands_section();
            exit(0);
        } else if (pid > 0) {
            close(pipefd[1]);
            char buffer[4096] = {0};
            read(pipefd[0], buffer, sizeof(buffer) - 1);
            close(pipefd[0]);
            waitpid(pid, NULL, 0);
            
            ASSERT_TRUE(strstr(buffer, "Always Blocked Commands") != NULL);
            ASSERT_TRUE(strstr(buffer, "System Control:") != NULL);
        }
    }
    
    printf("✓ Enhanced rules command tests passed\n");
    return 1;}

/* Test awk/sed support */
int test_awk_sed_support(void) {
    printf("Testing awk/sed support...\n");
    
    /* Test awk with field references */
    ASSERT_TRUE(validate_command("echo hello world | awk '{print $1}'"));
    ASSERT_TRUE(validate_command("ps aux | awk 'NR>1 {print $1}'"));
    
    /* Test sed with patterns */
    ASSERT_TRUE(validate_command("echo hello | sed 's/hello/hi/'"));
    ASSERT_TRUE(validate_command("cat file | sed 's/old/new/g'"));
    
    /* Test safe redirection */
    ASSERT_TRUE(validate_command("echo data | awk 'NF' > /tmp/output.txt"));
    ASSERT_TRUE(validate_command("echo test | sed 's/test/result/' > /tmp/result.txt"));
    
    /* Test dangerous operations are blocked */
    ASSERT_FALSE(validate_command("awk 'BEGIN{system(\"ls\")}'"));
    ASSERT_FALSE(validate_command("sed 'e ls' file"));
    
    printf("✓ Awk/sed support tests passed\n");
    return 1;}

/* Test pipeline security enhancements */
int test_pipeline_security(void) {
    printf("Testing pipeline security enhancements...\n");
    
    /* Test safe pipelines */
    ASSERT_TRUE(validate_secure_pipeline("ps aux | grep nginx"));
    ASSERT_TRUE(validate_secure_pipeline("cat file | awk '{print $1}' | sort"));
    ASSERT_TRUE(validate_secure_pipeline("echo data | sed 's/old/new/' | grep result"));
    
    /* Test pipeline with redirection */
    ASSERT_TRUE(validate_secure_pipeline("ps aux | grep nginx > /tmp/processes.txt"));
    ASSERT_TRUE(validate_secure_pipeline("cat log | awk '{print $1}' | sort > /tmp/sorted.txt"));
    
    /* Test dangerous pipelines are blocked */
    ASSERT_FALSE(validate_secure_pipeline("ls | rm"));
    ASSERT_FALSE(validate_secure_pipeline("cat file | bash"));
    
    printf("✓ Pipeline security tests passed\n");
    return 1;}

/* Test command categorization */
int test_command_categorization(void) {
    printf("Testing command categorization...\n");
    
    /* Test safe command detection */
    ASSERT_TRUE(is_safe_command("ls"));
    ASSERT_TRUE(is_safe_command("grep"));
    ASSERT_TRUE(is_safe_command("awk"));
    ASSERT_TRUE(is_safe_command("sed"));
    ASSERT_TRUE(is_safe_command("echo"));
    
    /* Test dangerous command detection */
    ASSERT_TRUE(is_dangerous_command("init"));
    ASSERT_TRUE(is_dangerous_command("shutdown"));
    ASSERT_TRUE(is_dangerous_command("fdisk"));
    ASSERT_TRUE(is_dangerous_command("iptables"));
    
    /* Test shell command detection */
    ASSERT_TRUE(is_shell_command("bash"));
    ASSERT_TRUE(is_shell_command("sh"));
    ASSERT_TRUE(is_shell_command("/bin/zsh"));
    
    printf("✓ Command categorization tests passed\n");
    return 1;}

/* Test redirection security */
int test_redirection_security(void) {
    printf("Testing redirection security...\n");
    
    /* Test safe redirection targets */
    ASSERT_TRUE(is_safe_redirection_target("/tmp/file.txt"));
    ASSERT_TRUE(is_safe_redirection_target("/var/tmp/log.txt"));
    ASSERT_TRUE(is_safe_redirection_target("~/output.txt"));
    
    /* Test dangerous redirection targets */
    ASSERT_FALSE(is_safe_redirection_target("/etc/passwd"));
    ASSERT_FALSE(is_safe_redirection_target("/etc/shadow"));
    ASSERT_FALSE(is_safe_redirection_target("/boot/grub/grub.cfg"));
    
    /* Test redirection validation */
    ASSERT_TRUE(validate_safe_redirection("echo test > /tmp/output.txt"));
    ASSERT_FALSE(validate_safe_redirection("echo test > /etc/passwd"));
    
    printf("✓ Redirection security tests passed\n");
    return 1;}

/* Test backward compatibility */
int test_backward_compatibility(void) {
    printf("Testing backward compatibility...\n");
    
    /* Test that existing functionality still works */
    ASSERT_TRUE(validate_command("ls -la"));
    ASSERT_TRUE(validate_command("grep pattern file"));
    ASSERT_TRUE(validate_command("systemctl status nginx"));
    
    /* Test that dangerous commands are still blocked */
    ASSERT_FALSE(validate_command("rm -rf /"));
    ASSERT_FALSE(validate_command("dd if=/dev/zero of=/dev/sda"));
    ASSERT_FALSE(validate_command("iptables -F"));
    
    /* Test that shell commands are still blocked in normal mode */
    sudo_compat_mode_flag = 0;
    ASSERT_FALSE(validate_command("bash"));
    ASSERT_FALSE(validate_command("sh"));
    
    printf("✓ Backward compatibility tests passed\n");
    return 1;}

/* Test error handling and edge cases */
int test_error_handling(void) {
    printf("Testing error handling and edge cases...\n");
    
    /* Test NULL inputs */
    ASSERT_FALSE(validate_command(NULL));
    ASSERT_FALSE(is_safe_command(NULL));
    ASSERT_FALSE(is_dangerous_command(NULL));
    ASSERT_FALSE(is_shell_command(NULL));
    
    /* Test empty inputs */
    ASSERT_FALSE(validate_command(""));
    ASSERT_FALSE(is_safe_command(""));
    
    /* Test very long inputs */
    char long_command[2048];
    memset(long_command, 'a', sizeof(long_command) - 1);
    long_command[sizeof(long_command) - 1] = '\0';
    ASSERT_FALSE(validate_command(long_command));
    
    /* Test malformed commands */
    ASSERT_FALSE(validate_command("   "));
    ASSERT_FALSE(validate_command("\n\t"));
    
    printf("✓ Error handling tests passed\n");
    return 1;}

/* Test integration between features */
int test_feature_integration(void) {
    printf("Testing feature integration...\n");
    
    /* Test that shell redirection works with enhanced rules */
    sudo_compat_mode_flag = 1;
    ASSERT_EQUAL(validate_command("bash"), 2);
    
    /* Test that awk/sed work with pipeline security */
    ASSERT_TRUE(validate_secure_pipeline("echo data | awk '{print $1}' | sed 's/old/new/'"));
    
    /* Test that redirection works with text processing */
    ASSERT_TRUE(validate_command("echo test | awk '{print $1}' > /tmp/result.txt"));
    
    /* Test that all security controls work together */
    ASSERT_FALSE(validate_command("echo test | awk 'BEGIN{system(\"rm -rf /\")}' > /etc/passwd"));
    
    printf("✓ Feature integration tests passed\n");
    return 1;}

int main(void) {
    printf("=== Sudosh v2.0 Comprehensive Regression Tests ===\n");
    
    RUN_TEST(test_shell_redirection_feature);
    RUN_TEST(test_enhanced_rules_command);
    RUN_TEST(test_awk_sed_support);
    RUN_TEST(test_pipeline_security);
    RUN_TEST(test_command_categorization);
    RUN_TEST(test_redirection_security);
    RUN_TEST(test_backward_compatibility);
    RUN_TEST(test_error_handling);
    RUN_TEST(test_feature_integration);
    
    printf("\n=== v2.0 Regression Test Summary ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passes);
    printf("Failed: %d\n", test_failures);
    
    if (test_failures == 0) {
        printf("\n🎉 All v2.0 features working correctly - ready for release!\n");
    } else {
        printf("\n❌ Some v2.0 features have issues - review before release\n");
    }
    
    return (test_failures == 0) ? 0 : 1;
}
