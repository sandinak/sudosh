#include "test_framework.h"
#include "sudosh.h"
#include <sys/wait.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test sudosh command line options */
int test_command_line_options() {
    capture_result_t *result;
    
    /* Test --help option */
    result = capture_command_output("./bin/sudosh --help");
    TEST_ASSERT_NOT_NULL(result, "help command should execute");
    if (result->exit_code != -1) {
        TEST_ASSERT_EQ(0, WEXITSTATUS(result->exit_code), "help should exit with code 0");
        if (result->stdout_content) {
            TEST_ASSERT(strstr(result->stdout_content, "Usage:") != NULL, "help should contain usage information");
            TEST_ASSERT(strstr(result->stdout_content, "sudosh") != NULL, "help should mention sudosh");
        }
    }
    free_capture_result(result);
    
    /* Test --version option */
    result = capture_command_output("./bin/sudosh --version");
    TEST_ASSERT_NOT_NULL(result, "version command should execute");
    TEST_ASSERT_EQ(0, WEXITSTATUS(result->exit_code), "version should exit with code 0");
    TEST_ASSERT(strstr(result->stdout_content, "sudosh") != NULL, "version should mention sudosh");
    TEST_ASSERT(strstr(result->stdout_content, "1.3.0") != NULL, "version should show version number");
    free_capture_result(result);
    
    /* Test invalid option */
    result = capture_command_output("./bin/sudosh --invalid-option");
    TEST_ASSERT_NOT_NULL(result, "invalid option command should execute");
    TEST_ASSERT_NE(0, WEXITSTATUS(result->exit_code), "invalid option should exit with non-zero code");
    TEST_ASSERT(strstr(result->stderr_content, "unknown option") != NULL, "should report unknown option");
    free_capture_result(result);
    
    return 1;
}

/* Test binary existence and permissions */
int test_binary_properties() {
    /* Check if binary exists */
    TEST_ASSERT_EQ(0, access("./bin/sudosh", F_OK), "sudosh binary should exist");
    
    /* Check if binary is executable */
    TEST_ASSERT_EQ(0, access("./bin/sudosh", X_OK), "sudosh binary should be executable");
    
    /* Get file stats */
    struct stat st;
    TEST_ASSERT_EQ(0, stat("./bin/sudosh", &st), "should be able to stat sudosh binary");
    
    /* Check that it's a regular file */
    TEST_ASSERT(S_ISREG(st.st_mode), "sudosh should be a regular file");
    
    /* Check that owner has execute permission */
    TEST_ASSERT(st.st_mode & S_IXUSR, "sudosh should be executable by owner");
    
    return 1;
}

/* Test that sudosh can be started (but will fail auth in test environment) */
int test_sudosh_startup() {
    /* Create a test script that starts sudosh and immediately exits */
    char *test_script = create_temp_file("#!/bin/bash\necho 'exit' | timeout 5 ./bin/sudosh 2>&1\n");
    TEST_ASSERT_NOT_NULL(test_script, "should be able to create test script");
    
    /* Make script executable */
    chmod(test_script, 0755);
    
    /* Run the test script */
    char command[256];
    snprintf(command, sizeof(command), "%s", test_script);
    capture_result_t *result = capture_command_output(command);
    
    TEST_ASSERT_NOT_NULL(result, "test script should execute");
    
    /* sudosh should start and show some output before failing */
    TEST_ASSERT(result->stdout_content != NULL || result->stderr_content != NULL, 
                "sudosh should produce some output");
    
    /* Check for expected startup messages */
    char *all_output = malloc(strlen(result->stdout_content ? result->stdout_content : "") + 
                             strlen(result->stderr_content ? result->stderr_content : "") + 1);
    strcpy(all_output, result->stdout_content ? result->stdout_content : "");
    strcat(all_output, result->stderr_content ? result->stderr_content : "");
    
    /* Should contain some expected text */
    TEST_ASSERT(strstr(all_output, "sudosh") != NULL || 
                strstr(all_output, "trust") != NULL ||
                strstr(all_output, "privilege") != NULL ||
                strstr(all_output, "authentication") != NULL,
                "sudosh should show expected startup messages");
    
    free(all_output);
    free_capture_result(result);
    remove_temp_file(test_script);
    
    return 1;
}

/* Test logging initialization */
int test_logging_integration() {
    /* Initialize logging */
    init_logging();
    
    /* Log some test messages */
    log_error("Integration test error message");
    log_authentication("testuser", 1);
    log_authentication("testuser", 0);
    log_session_start("testuser");
    log_command("testuser", "test command", 1);
    log_command("testuser", "failed command", 0);
    log_session_end("testuser");
    log_security_violation("testuser", "test violation");
    
    /* Close logging */
    close_logging();
    
    /* We can't easily verify syslog output in tests, but we can verify
     * the functions don't crash */
    printf("  (Logging integration completed without errors) ");
    
    return 1;
}

/* Test full security initialization */
int test_security_integration() {
    /* Save original environment */
    char *orig_path = getenv("PATH") ? strdup(getenv("PATH")) : NULL;
    char *orig_home = getenv("HOME") ? strdup(getenv("HOME")) : NULL;
    
    /* Set some test environment variables */
    setenv("LD_PRELOAD", "malicious.so", 1);
    setenv("TMPDIR", "/tmp/malicious", 1);
    
    /* Initialize security (but skip privilege check) */
    setup_signal_handlers();
    secure_terminal();
    sanitize_environment();
    
    /* Verify environment was sanitized */
    TEST_ASSERT_NULL(getenv("LD_PRELOAD"), "LD_PRELOAD should be removed");
    TEST_ASSERT_NULL(getenv("TMPDIR"), "TMPDIR should be removed");
    TEST_ASSERT_STR_EQ("root", getenv("USER"), "USER should be set to root");
    
    /* Restore some original environment */
    if (orig_path) {
        setenv("PATH", orig_path, 1);
        free(orig_path);
    }
    if (orig_home) {
        setenv("HOME", orig_home, 1);
        free(orig_home);
    }
    
    return 1;
}

/* Test command parsing and validation integration */
int test_command_integration() {
    struct command_info cmd;
    
    /* Test parsing and validation of various commands */
    const char *test_commands[] = {
        "ls -la",
        "ps aux",
        "echo 'hello world'",
        "date",
        "uptime",
        NULL
    };
    
    for (int i = 0; test_commands[i]; i++) {
        /* Parse command */
        int parse_result = parse_command(test_commands[i], &cmd);
        TEST_ASSERT_EQ(0, parse_result, "command should parse successfully");
        
        /* Validate command */
        int validate_result = validate_command(test_commands[i]);
        TEST_ASSERT_EQ(1, validate_result, "command should pass validation");
        
        /* Clean up */
        free_command_info(&cmd);
    }
    
    /* Test invalid commands */
    const char *invalid_commands[] = {
        "cat ../../../etc/passwd",
        "ls ..\\windows",
        NULL
    };
    
    for (int i = 0; invalid_commands[i]; i++) {
        int validate_result = validate_command(invalid_commands[i]);
        TEST_ASSERT_EQ(0, validate_result, "invalid command should fail validation");
    }
    
    return 1;
}

/* Test user information integration */
int test_user_integration() {
    /* Get current user */
    char *username = get_current_username();
    TEST_ASSERT_NOT_NULL(username, "should get current username");
    
    /* Get user info */
    struct user_info *user = get_user_info(username);
    TEST_ASSERT_NOT_NULL(user, "should get user info");
    
    /* Verify user info is consistent */
    TEST_ASSERT_STR_EQ(username, user->username, "usernames should match");
    /* Note: uid_t and gid_t are unsigned, so they're always >= 0 */
    TEST_ASSERT(user->uid != (uid_t)-1, "UID should be valid");
    TEST_ASSERT(user->gid != (gid_t)-1, "GID should be valid");
    TEST_ASSERT_NOT_NULL(user->home_dir, "home directory should be set");
    TEST_ASSERT_NOT_NULL(user->shell, "shell should be set");
    
    /* Check sudo privileges */
    int has_sudo = check_sudo_privileges(username);
    printf("  (User %s %s sudo privileges) ", username, has_sudo ? "has" : "lacks");
    
    /* Clean up */
    free_user_info(user);
    free(username);
    
    return 1;
}

TEST_SUITE_BEGIN("Integration Tests - Basic")
    RUN_TEST(test_command_line_options);
    RUN_TEST(test_binary_properties);
    RUN_TEST(test_sudosh_startup);
    RUN_TEST(test_logging_integration);
    RUN_TEST(test_security_integration);
    RUN_TEST(test_command_integration);
    RUN_TEST(test_user_integration);
TEST_SUITE_END()
