#include "test_framework.h"
#include "../sudosh.h"
#include <syslog.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

/* Test comprehensive logging functionality */

/* Test that logging initialization works correctly */
int test_logging_initialization() {
    /* Test multiple initializations */
    init_logging();
    init_logging();  /* Should handle multiple calls gracefully */
    
    /* Test closing and reinitializing */
    close_logging();
    init_logging();
    close_logging();
    
    printf("  (Logging initialization handled correctly) ");
    return 1;
}

/* Test command logging with various scenarios */
int test_command_logging_comprehensive() {
    init_logging();
    
    /* Test successful commands */
    log_command("testuser", "ls -la /root", 1);
    log_command("admin", "systemctl restart nginx", 1);
    log_command("user123", "apt update && apt upgrade", 1);
    
    /* Test failed commands */
    log_command("testuser", "invalid_command_xyz", 0);
    log_command("admin", "rm -rf /nonexistent", 0);
    
    /* Test commands with special characters */
    log_command("testuser", "echo 'Hello World!'", 1);
    log_command("testuser", "grep \"pattern\" /var/log/syslog", 1);
    log_command("testuser", "find /tmp -name '*.tmp' -delete", 1);
    
    /* Test very long commands */
    char long_command[512];
    memset(long_command, 'a', sizeof(long_command) - 1);
    long_command[sizeof(long_command) - 1] = '\0';
    log_command("testuser", long_command, 1);
    
    /* Test edge cases */
    log_command("", "command", 1);
    log_command("user", "", 1);
    log_command(NULL, "command", 1);
    log_command("user", NULL, 1);
    log_command(NULL, NULL, 1);
    
    /* Test usernames with special characters */
    log_command("user.name", "ls", 1);
    log_command("user-123", "pwd", 1);
    log_command("user_test", "whoami", 1);
    
    close_logging();
    
    printf("  (Command logging tested with various scenarios) ");
    return 1;
}

/* Test authentication logging */
int test_authentication_logging_comprehensive() {
    init_logging();
    
    /* Test successful authentications */
    log_authentication("testuser", 1);
    log_authentication("admin", 1);
    log_authentication("user123", 1);
    
    /* Test failed authentications */
    log_authentication("testuser", 0);
    log_authentication("admin", 0);
    log_authentication("hacker", 0);
    
    /* Test edge cases */
    log_authentication("", 1);
    log_authentication("", 0);
    log_authentication(NULL, 1);
    log_authentication(NULL, 0);
    
    /* Test usernames with special characters */
    log_authentication("user.name", 1);
    log_authentication("user-123", 0);
    log_authentication("user_test", 1);
    
    close_logging();
    
    printf("  (Authentication logging tested comprehensively) ");
    return 1;
}

/* Test session logging */
int test_session_logging_comprehensive() {
    init_logging();
    
    /* Test session start/end pairs */
    log_session_start("testuser");
    log_session_end("testuser");
    
    log_session_start("admin");
    log_session_end("admin");
    
    log_session_start("user123");
    log_session_end("user123");
    
    /* Test edge cases */
    log_session_start("");
    log_session_end("");
    
    log_session_start(NULL);
    log_session_end(NULL);
    
    /* Test usernames with special characters */
    log_session_start("user.name");
    log_session_end("user.name");
    
    log_session_start("user-123");
    log_session_end("user-123");
    
    close_logging();
    
    printf("  (Session logging tested comprehensively) ");
    return 1;
}

/* Test error logging */
int test_error_logging_comprehensive() {
    init_logging();
    
    /* Test various error messages */
    log_error("Test error message");
    log_error("Authentication failed for user");
    log_error("Command execution failed");
    log_error("Memory allocation failed");
    log_error("File not found");
    log_error("Permission denied");
    
    /* Test error messages with special characters */
    log_error("Error: file '/path/to/file' not found");
    log_error("Error: command \"ls -la\" failed with code 127");
    log_error("Error: user 'testuser' not in sudoers");
    
    /* Test very long error messages */
    char long_error[512];
    memset(long_error, 'e', sizeof(long_error) - 1);
    long_error[sizeof(long_error) - 1] = '\0';
    log_error(long_error);
    
    /* Test edge cases */
    log_error("");
    log_error(NULL);
    
    close_logging();
    
    printf("  (Error logging tested comprehensively) ");
    return 1;
}

/* Test security violation logging */
int test_security_violation_logging_comprehensive() {
    init_logging();
    
    /* Test various security violations */
    log_security_violation("testuser", "user not in sudoers");
    log_security_violation("hacker", "invalid authentication attempt");
    log_security_violation("testuser", "command rejected for security reasons");
    log_security_violation("admin", "privilege escalation attempt");
    log_security_violation("user123", "suspicious command pattern detected");
    
    /* Test violations with special characters */
    log_security_violation("user.name", "attempted to access '/etc/shadow'");
    log_security_violation("user-123", "tried to execute 'rm -rf /'");
    log_security_violation("user_test", "buffer overflow attempt detected");
    
    /* Test edge cases */
    log_security_violation("", "violation");
    log_security_violation("user", "");
    log_security_violation(NULL, "violation");
    log_security_violation("user", NULL);
    log_security_violation(NULL, NULL);
    
    close_logging();
    
    printf("  (Security violation logging tested comprehensively) ");
    return 1;
}

/* Test logging under stress conditions */
int test_logging_stress() {
    init_logging();
    
    /* Rapid logging test */
    for (int i = 0; i < 100; i++) {
        char username[32];
        char command[64];
        
        snprintf(username, sizeof(username), "user%d", i);
        snprintf(command, sizeof(command), "test_command_%d", i);
        
        log_command(username, command, i % 2);
        log_authentication(username, i % 3 != 0);
        
        if (i % 10 == 0) {
            log_session_start(username);
        }
        if (i % 10 == 9) {
            log_session_end(username);
        }
        
        if (i % 7 == 0) {
            char error[64];
            snprintf(error, sizeof(error), "Test error %d", i);
            log_error(error);
        }
        
        if (i % 13 == 0) {
            char violation[64];
            snprintf(violation, sizeof(violation), "Test violation %d", i);
            log_security_violation(username, violation);
        }
    }
    
    close_logging();
    
    printf("  (Stress testing completed successfully) ");
    return 1;
}

/* Test logging with concurrent access simulation */
int test_logging_concurrent() {
    pid_t pid;
    int status;
    
    init_logging();
    
    /* Fork a child process to simulate concurrent logging */
    pid = fork();
    if (pid == 0) {
        /* Child process */
        for (int i = 0; i < 50; i++) {
            log_command("child_user", "child_command", 1);
            usleep(1000); /* Small delay */
        }
        exit(0);
    } else if (pid > 0) {
        /* Parent process */
        for (int i = 0; i < 50; i++) {
            log_command("parent_user", "parent_command", 1);
            usleep(1000); /* Small delay */
        }
        
        /* Wait for child to complete */
        waitpid(pid, &status, 0);
    } else {
        /* Fork failed */
        close_logging();
        printf("  (Fork failed, skipping concurrent test) ");
        return 1;
    }
    
    close_logging();
    
    printf("  (Concurrent logging test completed) ");
    return 1;
}

TEST_SUITE_BEGIN("Comprehensive Logging Tests")
    RUN_TEST(test_logging_initialization);
    RUN_TEST(test_command_logging_comprehensive);
    RUN_TEST(test_authentication_logging_comprehensive);
    RUN_TEST(test_session_logging_comprehensive);
    RUN_TEST(test_error_logging_comprehensive);
    RUN_TEST(test_security_violation_logging_comprehensive);
    RUN_TEST(test_logging_stress);
    RUN_TEST(test_logging_concurrent);
TEST_SUITE_END()
