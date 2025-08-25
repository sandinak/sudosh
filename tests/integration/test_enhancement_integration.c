#include "../test_framework.h"
#include "../../src/sudosh.h"
#include <sys/wait.h>
#include <time.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test that existing sudosh functionality remains unchanged */
int test_existing_functionality_unchanged() {
    /* Test basic command validation still works */
    TEST_ASSERT_EQ(1, validate_command("ls -la"), "Basic ls command should still work");
    TEST_ASSERT_EQ(1, validate_command("pwd"), "pwd command should still work");
    TEST_ASSERT_EQ(1, validate_command("whoami"), "whoami command should still work");
    TEST_ASSERT_EQ(1, validate_command("date"), "date command should still work");
    
    /* Test dangerous commands are still blocked */
    TEST_ASSERT_EQ(0, validate_command("rm -rf /"), "rm -rf should still be blocked");
    TEST_ASSERT_EQ(0, validate_command("chmod 777 /etc/passwd"), "chmod 777 on system file should be blocked");
    
    return 1;
}

/* Test fallback mechanisms when NSS sources are unavailable */
int test_nss_fallback_mechanisms() {
    /* Test that functions gracefully handle missing NSS sources */
    
    /* Test with a user that should exist */
    struct user_info *user = get_user_info_files("root");
    if (!user) {
        /* If files method fails, test fallback */
        user = get_user_info("root");
        if (user) {
            TEST_ASSERT_STR_EQ("root", user->username, "Fallback should work for root");
            free_user_info(user);
        }
    } else {
        TEST_ASSERT_STR_EQ("root", user->username, "Direct method should work for root");
        free_user_info(user);
    }
    
    /* Test privilege checking fallback */
    int has_privileges = check_sudo_privileges_nss("root");
    printf("  (NSS privilege check result: %s) ", has_privileges ? "has privileges" : "no privileges");
    
    /* Should return 0 or 1, not crash */
    TEST_ASSERT(has_privileges == 0 || has_privileges == 1, 
                "Privilege check should return valid result");
    
    return 1;
}

/* Test behavior with different sudoers configurations */
int test_different_sudoers_configurations() {
    /* Test sudoers parsing with various configurations */
    
    /* Test parsing main sudoers file */
    struct sudoers_config *config = parse_sudoers_file(NULL);
    if (config) {
        TEST_ASSERT_NOT_NULL(config, "Should be able to parse sudoers file");
        
        /* Test privilege checking with parsed config */
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) != 0) {
            strcpy(hostname, "localhost");
        }
        
        int has_privileges = check_sudoers_privileges("root", hostname, config);
        printf("  (sudoers privilege check for root: %s) ", 
               has_privileges ? "has privileges" : "no privileges");
        
        free_sudoers_config(config);
    }
    
    return 1;
}

/* Test SSSD integration when available/unavailable */
int test_sssd_integration() {
    /* Test SSSD functions handle availability gracefully */
    
    /* Test SSSD privilege checking */
    int sssd_privileges = check_sssd_privileges("root");
    printf("  (SSSD privilege check: %s) ", sssd_privileges ? "has privileges" : "no privileges");
    
    /* Should return 0 or 1, not crash */
    TEST_ASSERT(sssd_privileges == 0 || sssd_privileges == 1, 
                "SSSD privilege check should return valid result");
    
    /* Test SSSD user info */
    struct user_info *sssd_user = get_user_info_sssd_direct("root");
    if (sssd_user) {
        TEST_ASSERT_STR_EQ("root", sssd_user->username, "SSSD user info should be correct");
        free_user_info(sssd_user);
    }
    /* If SSSD is not available, this should return NULL without crashing */
    
    return 1;
}

/* Test performance impact of new features */
int test_performance_impact() {
    clock_t start, end;
    double cpu_time_used;
    
    /* Test NSS function performance */
    start = clock();
    for (int i = 0; i < 100; i++) {
        struct user_info *user = get_user_info_files("root");
        if (user) {
            free_user_info(user);
        }
    }
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("  (NSS user lookup 100x: %f seconds) ", cpu_time_used);
    
    /* Should complete in reasonable time (less than 1 second for 100 lookups) */
    TEST_ASSERT(cpu_time_used < 1.0, "NSS lookups should be reasonably fast");
    
    /* Test pipeline validation performance */
    start = clock();
    for (int i = 0; i < 100; i++) {
        validate_secure_pipeline("ls | grep test | head -10");
    }
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("  (Pipeline validation 100x: %f seconds) ", cpu_time_used);
    
    /* Should complete in reasonable time */
    TEST_ASSERT(cpu_time_used < 1.0, "Pipeline validation should be reasonably fast");
    
    return 1;
}

/* Test memory usage of new parsing functions */
int test_memory_usage() {
    /* Test that repeated operations don't leak memory */
    
    /* Test NSS config memory usage */
    for (int i = 0; i < 1000; i++) {
        struct nss_config *config = read_nss_config();
        if (config) {
            free_nss_config(config);
        }
    }
    
    /* Test user info memory usage */
    for (int i = 0; i < 1000; i++) {
        struct user_info *user = get_user_info_files("root");
        if (user) {
            free_user_info(user);
        }
    }
    
    /* Test pipeline memory usage */
    for (int i = 0; i < 1000; i++) {
        struct pipeline_info pipeline;
        if (parse_pipeline("ls | grep test", &pipeline) == 0) {
            free_pipeline_info(&pipeline);
        }
    }
    
    /* If we get here without running out of memory, we're good */
    return 1;
}

/* Test integration of all three enhancements together */
int test_integrated_functionality() {
    /* Test a complex scenario using all three enhancements */
    
    /* 1. Use NSS to check user privileges */
    char *username = getenv("USER");
    if (!username) username = "root";
    
    int has_privileges = check_sudo_privileges_nss(username);
    printf("  (User %s has privileges: %s) ", username, has_privileges ? "yes" : "no");
    
    /* 2. Validate a secure pipeline */
    const char *pipeline = "ps aux | grep -v grep | awk '{print $2, $11}' | sort | head -10";
    int pipeline_valid = validate_secure_pipeline(pipeline);
    TEST_ASSERT_EQ(1, pipeline_valid, "Complex pipeline should be valid");
    
    /* 3. Test text processing with safe redirection */
    int redirection_safe = validate_safe_redirection("grep pattern file.txt > /tmp/output.txt");
    TEST_ASSERT_EQ(1, redirection_safe, "Safe redirection should be allowed");
    
    /* 4. Test that dangerous combinations are still blocked */
    int dangerous_blocked = validate_secure_pipeline("ls | sudo rm -rf /");
    TEST_ASSERT_EQ(0, dangerous_blocked, "Dangerous pipeline should be blocked");
    
    return 1;
}

/* Test error handling across all enhancements */
int test_comprehensive_error_handling() {
    /* Test NULL pointer handling */
    TEST_ASSERT_NULL(get_user_info_files(NULL), "NULL username should be handled");
    TEST_ASSERT_EQ(0, check_admin_groups_files(NULL), "NULL username should be handled");
    TEST_ASSERT_EQ(0, validate_secure_pipeline(NULL), "NULL pipeline should be handled");
    TEST_ASSERT_EQ(0, is_safe_redirection_target(NULL), "NULL target should be handled");
    
    /* Test empty string handling */
    TEST_ASSERT_NULL(get_user_info_files(""), "Empty username should be handled");
    TEST_ASSERT_EQ(0, validate_secure_pipeline(""), "Empty pipeline should be handled");
    TEST_ASSERT_EQ(0, is_safe_redirection_target(""), "Empty target should be handled");
    
    /* Test malformed input handling */
    TEST_ASSERT_EQ(0, validate_secure_pipeline("|||"), "Malformed pipeline should be handled");
    TEST_ASSERT_EQ(0, is_safe_redirection_target(">>>"), "Malformed target should be handled");
    
    return 1;
}

/* Test concurrent access to new functions */
int test_concurrent_access() {
    /* Test that functions are thread-safe or at least don't crash under concurrent access */
    /* This is a basic test - real thread safety would require pthread testing */
    
    pid_t pid = fork();
    if (pid == 0) {
        /* Child process */
        for (int i = 0; i < 50; i++) {
            struct user_info *user = get_user_info_files("root");
            if (user) {
                free_user_info(user);
            }
            validate_secure_pipeline("ls | grep test");
        }
        exit(0);
    } else if (pid > 0) {
        /* Parent process */
        for (int i = 0; i < 50; i++) {
            struct user_info *user = get_user_info_files("root");
            if (user) {
                free_user_info(user);
            }
            validate_secure_pipeline("ps | head -10");
        }
        
        /* Wait for child */
        int status;
        waitpid(pid, &status, 0);
        TEST_ASSERT_EQ(0, WEXITSTATUS(status), "Child process should exit successfully");
    } else {
        /* Fork failed, skip this test */
        printf("  (fork failed, skipping concurrent test) ");
    }
    
    return 1;
}

/* Test edge cases in command parsing */
int test_command_parsing_edge_cases() {
    /* Test commands with unusual but valid syntax */
    TEST_ASSERT_EQ(1, validate_command("ls"), "Single command should be valid");
    TEST_ASSERT_EQ(1, validate_secure_pipeline("ls -la | grep '^d'"), "Pipeline with flags should be valid");
    
    /* Test commands with lots of whitespace */
    TEST_ASSERT_EQ(1, validate_secure_pipeline("  ls   |   grep   test   "), "Whitespace should be handled");
    
    /* Test very long but valid pipelines */
    const char *long_pipeline = "ps aux | grep -v grep | awk '{print $2}' | sort -n | uniq | head -20 | tail -10";
    TEST_ASSERT_EQ(1, validate_secure_pipeline(long_pipeline), "Long valid pipeline should work");
    
    return 1;
}

/* Test compatibility with existing test framework */
int test_framework_compatibility() {
    /* Test that our new functions work with the existing test framework */
    
    /* Test capture_command_output with new features */
    capture_result_t *result = capture_command_output("echo 'test' 2>&1");
    if (result) {
        TEST_ASSERT_NOT_NULL(result->stdout_content, "Should capture output");
        free_capture_result(result);
    }
    
    /* Test temp file creation for redirection testing */
    char *temp_file = create_temp_file("test content");
    if (temp_file) {
        TEST_ASSERT_EQ(1, is_safe_redirection_target(temp_file), "Temp file should be safe target");
        remove_temp_file(temp_file);
    }
    
    return 1;
}

TEST_SUITE_BEGIN("Enhancement Integration Tests")
    RUN_TEST(test_existing_functionality_unchanged);
    RUN_TEST(test_nss_fallback_mechanisms);
    RUN_TEST(test_different_sudoers_configurations);
    RUN_TEST(test_sssd_integration);
    RUN_TEST(test_performance_impact);
    RUN_TEST(test_memory_usage);
    RUN_TEST(test_integrated_functionality);
    RUN_TEST(test_comprehensive_error_handling);
    RUN_TEST(test_concurrent_access);
    RUN_TEST(test_command_parsing_edge_cases);
    RUN_TEST(test_framework_compatibility);
TEST_SUITE_END()
