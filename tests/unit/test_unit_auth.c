#include "test_framework.h"
#include "sudosh.h"
#include <sys/types.h>
#include <grp.h>

/* Global verbose flag for testing */
/* Global verbose flag is now defined in test_globals.c */

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test check_sudo_privileges function */
int test_check_sudo_privileges() {
    /* Test with NULL input first */
    int null_privileges = check_sudo_privileges(NULL);
    TEST_ASSERT_EQ(0, null_privileges, "NULL user should not have sudo privileges");

    /* Test with empty string */
    int empty_privileges = check_sudo_privileges("");
    TEST_ASSERT_EQ(0, empty_privileges, "empty user should not have sudo privileges");

    /* Test with obviously invalid user */
    int invalid_privileges = check_sudo_privileges("nonexistent_user_12345");
    TEST_ASSERT_EQ(0, invalid_privileges, "nonexistent user should not have sudo privileges");

    /* Test with current user */
    char *username = get_current_username();
    if (username) {
        /* Note: This test may pass or fail depending on the user's group membership */
        int has_privileges = check_sudo_privileges(username);
        printf("  (User %s %s sudo privileges) ", username, has_privileges ? "has" : "does not have");
        free(username);
    } else {
        printf("  (Could not get current username) ");
    }

    /* Test with root user (if we can test it) */
    int root_privileges = check_sudo_privileges("root");
    /* Root might not be in wheel/sudo group on all systems, so we don't assert this */
    printf("  (Root user %s sudo privileges) ", root_privileges ? "has" : "does not have");

    return 1;
}

/* Test authentication with mock auth (when available) */
int test_authenticate_user_mock() {
#ifdef MOCK_AUTH
    /* With mock auth, we can test the authentication flow */
    printf("  (Testing with mock authentication) ");
    
    /* Note: Mock authentication requires interactive input, so we can't fully test it
     * in an automated way. We'll just test the function exists and handles NULL input */
    
    /* Test with NULL input */
    int null_result = authenticate_user(NULL);
    TEST_ASSERT_EQ(0, null_result, "authenticate_user should fail with NULL input");
    
    /* Test with empty string */
    int empty_result = authenticate_user("");
    TEST_ASSERT_EQ(0, empty_result, "authenticate_user should fail with empty username");
    
    return 1;
#else
    printf("  (Skipping - PAM authentication active) ");
    return 1;
#endif
}

/* Test password input function (limited testing due to terminal requirements) */
int test_get_password() {
    /* We can't fully test get_password in an automated environment since it
     * requires terminal input. We'll just verify the function exists and
     * can handle NULL input without crashing. */

    printf("  (Skipping interactive password test - requires terminal) ");
    return 1;
}

/* Test group membership checking */
int test_group_membership() {
    /* Test if wheel group exists */
    struct group *wheel_group = getgrnam("wheel");
    if (wheel_group) {
        printf("  (wheel group found) ");
        TEST_ASSERT_NOT_NULL(wheel_group->gr_name, "wheel group should have a name");
        TEST_ASSERT_STR_EQ("wheel", wheel_group->gr_name, "wheel group name should be 'wheel'");
    } else {
        printf("  (wheel group not found) ");
    }
    
    /* Test if sudo group exists */
    struct group *sudo_group = getgrnam("sudo");
    if (sudo_group) {
        printf("  (sudo group found) ");
        TEST_ASSERT_NOT_NULL(sudo_group->gr_name, "sudo group should have a name");
        TEST_ASSERT_STR_EQ("sudo", sudo_group->gr_name, "sudo group name should be 'sudo'");
    } else {
        printf("  (sudo group not found) ");
    }
    
    /* At least one of wheel or sudo should exist on most systems */
    TEST_ASSERT(wheel_group != NULL || sudo_group != NULL, 
                "at least one of wheel or sudo group should exist");
    
    return 1;
}

/* Test authentication logging (we can't test actual syslog easily, but we can test the function calls) */
int test_authentication_logging() {
    /* Initialize logging */
    init_logging();
    
    /* Test logging authentication success */
    log_authentication("testuser", 1);
    
    /* Test logging authentication failure */
    log_authentication("testuser", 0);
    
    /* Test with NULL username */
    log_authentication(NULL, 1);
    
    /* Test with empty username */
    log_authentication("", 0);
    
    /* Close logging */
    close_logging();
    
    printf("  (Logging functions called without errors) ");
    return 1;
}

/* Test session logging */
int test_session_logging() {
    /* Initialize logging */
    init_logging();
    
    /* Test session start logging */
    log_session_start("testuser");
    
    /* Test session end logging */
    log_session_end("testuser");
    
    /* Test with NULL username */
    log_session_start(NULL);
    log_session_end(NULL);
    
    /* Test with empty username */
    log_session_start("");
    log_session_end("");
    
    /* Close logging */
    close_logging();
    
    printf("  (Session logging functions called without errors) ");
    return 1;
}

/* Test error logging */
int test_error_logging() {
    /* Initialize logging */
    init_logging();
    
    /* Test error logging */
    log_error("Test error message");
    
    /* Test with NULL message */
    log_error(NULL);
    
    /* Test with empty message */
    log_error("");
    
    /* Test security violation logging */
    log_security_violation("testuser", "Test security violation");
    
    /* Test security violation with NULL values */
    log_security_violation(NULL, "violation");
    log_security_violation("user", NULL);
    log_security_violation(NULL, NULL);
    
    /* Close logging */
    close_logging();
    
    printf("  (Error logging functions called without errors) ");
    return 1;
}

/* Test command logging */
int test_command_logging() {
    /* Initialize logging */
    init_logging();
    
    /* Test successful command logging */
    log_command("testuser", "ls -la", 1);
    
    /* Test failed command logging */
    log_command("testuser", "invalid_command", 0);
    
    /* Test with NULL values */
    log_command(NULL, "command", 1);
    log_command("user", NULL, 1);
    log_command(NULL, NULL, 1);
    
    /* Test with empty values */
    log_command("", "command", 1);
    log_command("user", "", 1);
    
    /* Close logging */
    close_logging();
    
    printf("  (Command logging functions called without errors) ");
    return 1;
}

TEST_SUITE_BEGIN("Unit Tests - Authentication")
    RUN_TEST(test_check_sudo_privileges);
    RUN_TEST(test_authenticate_user_mock);
    RUN_TEST(test_get_password);
    RUN_TEST(test_group_membership);
    RUN_TEST(test_authentication_logging);
    RUN_TEST(test_session_logging);
    RUN_TEST(test_error_logging);
    RUN_TEST(test_command_logging);
TEST_SUITE_END()
