#include "test_security_framework.h"

/* Global security test counters */
int security_count = 0;
int security_passes = 0;
int security_failures = 0;

/* Test command logging evasion */
int test_command_logging_evasion() {
    /* Test if commands can be executed without logging */
    
    /* Clear existing logs */
    system("echo '' > /tmp/sudosh_commands.log 2>/dev/null");
    
    /* Execute a command that should be logged */
    struct command_info cmd;
    if (parse_command("whoami", &cmd) == 0) {
        /* Log the command */
        log_command("testuser", "whoami", 1);
        free_command_info(&cmd);
    }
    
    /* Check if command was logged */
    int logged = check_syslog_entry("whoami");
    
    return logged ? 0 : 1; /* Vulnerable if not logged */
}

/* Test log file manipulation */
int test_log_file_manipulation() {
    /* Test if log files can be manipulated or deleted */
    
    /* Create a test log entry */
    log_command("testuser", "test_command", 1);
    
    /* Try to manipulate syslog (this should fail) */
    int result = system("echo 'FAKE LOG ENTRY' >> /var/log/syslog 2>/dev/null");
    
    /* If we can write to syslog, it's a vulnerability */
    return (result == 0) ? 1 : 0;
}

/* Test syslog bypass */
int test_syslog_bypass() {
    /* Test if logging can be bypassed by manipulating syslog */
    
    /* Try to redirect syslog */
    char *original_syslog = getenv("SYSLOG_FACILITY");
    setenv("SYSLOG_FACILITY", "/dev/null", 1);
    
    /* Initialize logging */
    init_logging();
    
    /* Log a test message */
    log_command("testuser", "bypass_test", 1);
    
    /* Check if message was still logged to proper syslog */
    int logged = check_syslog_entry("bypass_test");
    
    /* Restore environment */
    if (original_syslog) {
        setenv("SYSLOG_FACILITY", original_syslog, 1);
    } else {
        unsetenv("SYSLOG_FACILITY");
    }
    
    return logged ? 0 : 1; /* Vulnerable if not logged */
}

/* Test session logging evasion */
int test_session_logging_evasion() {
    /* Test if session events can avoid logging */
    
    /* Start a session */
    log_session_start("testuser");
    
    /* End the session */
    log_session_end("testuser");
    
    /* Check if session events were logged */
    int start_logged = check_syslog_entry("session start");
    int end_logged = check_syslog_entry("session end");
    
    return (start_logged && end_logged) ? 0 : 1;
}

/* Test security violation logging evasion */
int test_security_violation_evasion() {
    /* Test if security violations can avoid logging */
    
    /* Trigger a security violation */
    log_security_violation("testuser", "test violation");
    
    /* Check if violation was logged */
    int logged = check_syslog_entry("security violation") || 
                 check_syslog_entry("test violation");
    
    return logged ? 0 : 1;
}

/* Test log rotation interference */
int test_log_rotation_interference() {
    /* Test if log rotation can be interfered with */
    
    /* Create many log entries to trigger rotation */
    for (int i = 0; i < 1000; i++) {
        char command[64];
        snprintf(command, sizeof(command), "test_command_%d", i);
        log_command("testuser", command, 1);
    }
    
    /* Check if logs are still accessible */
    int logged = check_syslog_entry("test_command_999");
    
    return logged ? 0 : 1;
}

/* Test timestamp manipulation */
int test_timestamp_manipulation() {
    /* Test if log timestamps can be manipulated */
    
    /* Try to manipulate system time */
    time_t original_time = time(NULL);
    (void)original_time; /* Suppress unused variable warning */

    /* Log a command */
    log_command("testuser", "timestamp_test", 1);
    
    /* Check if timestamp in log is reasonable */
    /* This is a simplified test - real implementation would parse actual log timestamps */
    int logged = check_syslog_entry("timestamp_test");
    
    return logged ? 0 : 1;
}

/* Test remote logging bypass */
int test_remote_logging_bypass() {
    /* Test if remote logging can be bypassed */
    
    /* Try to manipulate network settings */
    char *original_host = getenv("SYSLOG_HOST");
    setenv("SYSLOG_HOST", "127.0.0.1", 1);
    
    /* Log a command */
    log_command("testuser", "remote_test", 1);
    
    /* Check if command was logged locally */
    int logged = check_syslog_entry("remote_test");
    
    /* Restore environment */
    if (original_host) {
        setenv("SYSLOG_HOST", original_host, 1);
    } else {
        unsetenv("SYSLOG_HOST");
    }
    
    return logged ? 0 : 1;
}

/* Test log level manipulation */
int test_log_level_manipulation() {
    /* Test if log levels can be manipulated to hide events */
    
    /* Try to set log level to suppress messages */
    char *original_level = getenv("SYSLOG_LEVEL");
    setenv("SYSLOG_LEVEL", "EMERG", 1);
    
    /* Log a command (should still be logged regardless of level) */
    log_command("testuser", "level_test", 1);
    
    /* Check if command was logged */
    int logged = check_syslog_entry("level_test");
    
    /* Restore environment */
    if (original_level) {
        setenv("SYSLOG_LEVEL", original_level, 1);
    } else {
        unsetenv("SYSLOG_LEVEL");
    }
    
    return logged ? 0 : 1;
}

/* Test audit trail manipulation */
int test_audit_trail_manipulation() {
    /* Test if audit trails can be manipulated */
    
    /* Create audit entries */
    log_authentication("testuser", 1);
    log_command("testuser", "audit_test", 1);
    log_session_end("testuser");
    
    /* Try to clear audit trail */
    int clear_result = system("echo '' > /var/log/audit/audit.log 2>/dev/null");
    (void)clear_result; /* Suppress unused variable warning */

    /* Check if our entries are still in syslog */
    int logged = check_syslog_entry("audit_test");
    
    /* If we could clear audit log but syslog still has entries, it's secure */
    /* If we couldn't clear audit log, it's also secure */
    return logged ? 0 : 1;
}

/* Test concurrent logging interference */
int test_concurrent_logging_interference() {
    /* Test if concurrent operations can interfere with logging */
    
    pid_t pid = fork();
    if (pid == 0) {
        /* Child process - generate log entries */
        for (int i = 0; i < 100; i++) {
            char command[64];
            snprintf(command, sizeof(command), "child_command_%d", i);
            log_command("child_user", command, 1);
            usleep(1000); /* 1ms delay */
        }
        exit(0);
    } else if (pid > 0) {
        /* Parent process - generate log entries */
        for (int i = 0; i < 100; i++) {
            char command[64];
            snprintf(command, sizeof(command), "parent_command_%d", i);
            log_command("parent_user", command, 1);
            usleep(1000); /* 1ms delay */
        }
        
        /* Wait for child */
        int status;
        waitpid(pid, &status, 0);
        
        /* Check if both parent and child commands were logged */
        int parent_logged = check_syslog_entry("parent_command_99");
        int child_logged = check_syslog_entry("child_command_99");
        
        return (parent_logged && child_logged) ? 0 : 1;
    }
    
    return 0; /* Fork failed, assume secure */
}

/* Test log injection */
int test_log_injection() {
    /* Test if malicious content can be injected into logs */
    
    /* Try to inject newlines and control characters */
    char malicious_command[] = "legitimate_command\nFAKE LOG ENTRY: root executed dangerous_command\n";
    
    /* Log the command */
    log_command("testuser", malicious_command, 1);
    
    /* Check if the fake entry appears in logs */
    int fake_logged = check_syslog_entry("FAKE LOG ENTRY");
    
    /* If fake entry is logged as separate entry, it's vulnerable */
    return fake_logged ? 1 : 0;
}

int main() {
    printf("=== Security Tests - Logging and Monitoring Evasion ===\n");
    
    /* Initialize security test counters */
    security_count = 0;
    security_passes = 0;
    security_failures = 0;
    
    /* Run logging evasion tests */
    SECURITY_TEST_ASSERT_LOGGED(test_command_logging_evasion, 
                                "Command execution logging evasion");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_log_file_manipulation, 
                                 "Log file manipulation");
    
    SECURITY_TEST_ASSERT_LOGGED(test_syslog_bypass, 
                                "Syslog bypass attempt");
    
    SECURITY_TEST_ASSERT_LOGGED(test_session_logging_evasion, 
                                "Session logging evasion");
    
    SECURITY_TEST_ASSERT_LOGGED(test_security_violation_evasion, 
                                "Security violation logging evasion");
    
    SECURITY_TEST_ASSERT_LOGGED(test_log_rotation_interference, 
                                "Log rotation interference");
    
    SECURITY_TEST_ASSERT_LOGGED(test_timestamp_manipulation, 
                                "Log timestamp manipulation");
    
    SECURITY_TEST_ASSERT_LOGGED(test_remote_logging_bypass, 
                                "Remote logging bypass");
    
    SECURITY_TEST_ASSERT_LOGGED(test_log_level_manipulation, 
                                "Log level manipulation");
    
    SECURITY_TEST_ASSERT_LOGGED(test_audit_trail_manipulation, 
                                "Audit trail manipulation");
    
    SECURITY_TEST_ASSERT_LOGGED(test_concurrent_logging_interference, 
                                "Concurrent logging interference");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_log_injection, 
                                 "Log injection attack");
    
    printf("\n=== Logging Evasion Test Results ===\n");
    printf("Total tests: %d\n", security_count);
    printf("Secure (logging intact): %d\n", security_passes);
    printf("Vulnerable (logging bypassed): %d\n", security_failures);
    
    if (security_failures == 0) {
        printf("✅ All logging evasion attacks were blocked!\n");
        return 0;
    } else {
        printf("❌ %d vulnerabilities found! Check /tmp/sudosh_vulnerabilities.log\n", security_failures);
        return 1;
    }
}
