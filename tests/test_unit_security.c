#include "test_framework.h"
#include "sudosh.h"
#include <signal.h>
#include <unistd.h>
#include <sys/resource.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test environment sanitization */
int test_sanitize_environment() {
    /* Set some dangerous environment variables */
    setenv("LD_PRELOAD", "/malicious/lib.so", 1);
    setenv("IFS", "malicious", 1);
    setenv("CDPATH", "/malicious/path", 1);
    setenv("TMPDIR", "/malicious/tmp", 1);
    
    /* Sanitize environment */
    sanitize_environment();
    
    /* Check that dangerous variables are removed */
    TEST_ASSERT_NULL(getenv("LD_PRELOAD"), "LD_PRELOAD should be removed");
    TEST_ASSERT_NULL(getenv("IFS"), "IFS should be removed");
    TEST_ASSERT_NULL(getenv("CDPATH"), "CDPATH should be removed");
    TEST_ASSERT_NULL(getenv("TMPDIR"), "TMPDIR should be removed");
    
    /* Check that safe variables are set */
    TEST_ASSERT_NOT_NULL(getenv("PATH"), "PATH should be set");
    TEST_ASSERT_STR_EQ("root", getenv("USER"), "USER should be set to root");
    TEST_ASSERT_STR_EQ("root", getenv("LOGNAME"), "LOGNAME should be set to root");
    TEST_ASSERT_STR_EQ("/root", getenv("HOME"), "HOME should be set to /root");
    
    return 1;
}

/* Test privilege checking */
int test_check_privileges() {
    /* This test depends on how the test is run */
    int has_privileges = check_privileges();
    
    /* We can't assert a specific result since it depends on the execution context */
    printf("  (Current process %s root privileges) ", has_privileges ? "has" : "does not have");
    
    /* The function should return 0 or 1, not other values */
    TEST_ASSERT(has_privileges == 0 || has_privileges == 1, 
                "check_privileges should return 0 or 1");
    
    return 1;
}

/* Test signal handler setup */
int test_setup_signal_handlers() {
    /* Setup signal handlers */
    setup_signal_handlers();
    
    /* We can't easily test signal handlers in unit tests, but we can verify
     * the function doesn't crash */
    printf("  (Signal handlers set up without errors) ");
    
    return 1;
}

/* Test username setting for signal handler */
int test_set_current_username() {
    /* Test setting username */
    set_current_username("testuser");
    
    /* Test setting NULL username */
    set_current_username(NULL);
    
    /* Test setting empty username */
    set_current_username("");
    
    /* Test setting another username */
    set_current_username("anotheruser");
    
    printf("  (Username setting functions called without errors) ");
    
    return 1;
}

/* Test command validation */
int test_validate_command_security() {
    /* Test valid commands that don't trigger interactive prompts */
    TEST_ASSERT_EQ(1, validate_command("ls"), "simple command should be valid");
    TEST_ASSERT_EQ(1, validate_command("ls -la /home"), "command with args should be valid");
    TEST_ASSERT_EQ(1, validate_command("ps aux"), "process listing should be valid");
    
    /* Test invalid commands */
    TEST_ASSERT_EQ(0, validate_command(NULL), "NULL command should be invalid");
    
    /* Test null byte injection - note: C strings terminate at first null byte,
     * so this test actually just tests "ls" which is valid. */
    char null_cmd[] = "ls\0rm -rf /";
    int null_result = validate_command(null_cmd);
    printf("  (null byte test result: %d) ", null_result);
    
    /* Test path traversal */
    TEST_ASSERT_EQ(0, validate_command("cat ../../../etc/passwd"), "path traversal should be invalid");
    TEST_ASSERT_EQ(0, validate_command("ls ..\\windows\\system32"), "windows path traversal should be invalid");
    
    /* Test overly long command */
    char *long_cmd = malloc(MAX_COMMAND_LENGTH + 100);
    if (long_cmd) {
        memset(long_cmd, 'a', MAX_COMMAND_LENGTH + 50);
        long_cmd[MAX_COMMAND_LENGTH + 50] = '\0';
        TEST_ASSERT_EQ(0, validate_command(long_cmd), "overly long command should be invalid");
        free(long_cmd);
    }

    /* Test shell command detection (these should be blocked without prompts) */
    TEST_ASSERT_EQ(0, validate_command("bash"), "bash should be blocked");
    TEST_ASSERT_EQ(0, validate_command("sh -c 'ls'"), "sh with command should be blocked");
    TEST_ASSERT_EQ(0, validate_command("/bin/zsh"), "absolute shell path should be blocked");

    return 1;
}

/* Test dangerous command detection functions directly */
int test_dangerous_command_detection() {
    /* Test dangerous command detection */
    TEST_ASSERT_EQ(1, is_dangerous_command("init"), "init should be detected as dangerous");
    TEST_ASSERT_EQ(1, is_dangerous_command("shutdown"), "shutdown should be detected as dangerous");
    TEST_ASSERT_EQ(1, is_dangerous_command("reboot"), "reboot should be detected as dangerous");
    TEST_ASSERT_EQ(1, is_dangerous_command("systemctl poweroff"), "systemctl poweroff should be dangerous");
    TEST_ASSERT_EQ(0, is_dangerous_command("ls"), "ls should not be dangerous");
    TEST_ASSERT_EQ(0, is_dangerous_command("ps aux"), "ps aux should not be dangerous");

    /* Test dangerous flags detection */
    TEST_ASSERT_EQ(1, check_dangerous_flags("rm -rf /tmp"), "rm -rf should be detected as dangerous");
    TEST_ASSERT_EQ(1, check_dangerous_flags("chmod -R 777 /"), "chmod -R should be detected as dangerous");
    TEST_ASSERT_EQ(1, check_dangerous_flags("chown -R user /"), "chown -R should be detected as dangerous");
    TEST_ASSERT_EQ(0, check_dangerous_flags("ls -la"), "ls -la should not have dangerous flags");
    TEST_ASSERT_EQ(0, check_dangerous_flags("find . -name test"), "find should not have dangerous flags");

    /* Test system directory access detection */
    TEST_ASSERT_EQ(1, check_system_directory_access("ls /dev"), "/dev access should be detected");
    TEST_ASSERT_EQ(1, check_system_directory_access("cat /proc/version"), "/proc access should be detected");
    TEST_ASSERT_EQ(1, check_system_directory_access("ls /sys"), "/sys access should be detected");
    TEST_ASSERT_EQ(0, check_system_directory_access("ls /home"), "/home access should be allowed");
    TEST_ASSERT_EQ(0, check_system_directory_access("ls /tmp"), "/tmp access should be allowed");

    return 1;
}

/* Test signal handling behavior */
int test_signal_handling() {
    /* Test that SIGINT doesn't set interrupted flag */
    TEST_ASSERT_EQ(0, is_interrupted(), "should not be interrupted initially");
    TEST_ASSERT_EQ(0, received_sigint_signal(), "should not have received SIGINT initially");

    /* Simulate SIGINT */
    kill(getpid(), SIGINT);
    usleep(10000); /* Give signal time to be processed */

    /* SIGINT should not set interrupted flag but should set SIGINT flag */
    TEST_ASSERT_EQ(0, is_interrupted(), "SIGINT should not set interrupted flag");
    TEST_ASSERT_EQ(1, received_sigint_signal(), "should have received SIGINT");

    /* Reset SIGINT flag */
    reset_sigint_flag();
    TEST_ASSERT_EQ(0, received_sigint_signal(), "SIGINT flag should be reset");

    return 1;
}

/* Test terminal security */
int test_secure_terminal() {
    /* Call secure_terminal function */
    secure_terminal();
    
    /* Check that core dumps are disabled */
    struct rlimit rlim;
    if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
        TEST_ASSERT_EQ(0, rlim.rlim_cur, "core dump limit should be set to 0");
    }
    
    printf("  (Terminal security measures applied) ");
    
    return 1;
}

/* Test security initialization */
int test_init_security() {
    /* Note: init_security() calls check_privileges() which might exit
     * if we don't have root privileges. We'll test the individual components instead. */
    
    /* Test individual security components */
    setup_signal_handlers();
    secure_terminal();
    sanitize_environment();
    
    printf("  (Security initialization components tested) ");
    
    return 1;
}

/* Test interrupt checking */
int test_is_interrupted() {
    /* Initially should not be interrupted */
    int interrupted = is_interrupted();
    TEST_ASSERT(interrupted == 0 || interrupted == 1, 
                "is_interrupted should return 0 or 1");
    
    printf("  (Interrupt status: %s) ", interrupted ? "interrupted" : "not interrupted");
    
    return 1;
}

/* Test security cleanup */
int test_cleanup_security() {
    /* Set a username first */
    set_current_username("testuser");
    
    /* Clean up security */
    cleanup_security();
    
    /* Call cleanup again to test double cleanup */
    cleanup_security();
    
    printf("  (Security cleanup completed without errors) ");
    
    return 1;
}

/* Test privilege dropping (if we have privileges) */
int test_drop_privileges() {
    /* Note: drop_privileges() will actually change our process privileges
     * and we might not be able to get them back. We'll only test this
     * if we're not running as root. */
    
    uid_t current_uid = getuid();
    uid_t current_euid = geteuid();
    
    if (current_euid == 0) {
        printf("  (Skipping privilege drop test - running as root) ");
    } else {
        /* If we're not root, drop_privileges should be safe to call */
        drop_privileges();
        
        /* Verify privileges were dropped */
        TEST_ASSERT_EQ(current_uid, getuid(), "UID should remain the same");
        TEST_ASSERT_EQ(current_uid, geteuid(), "EUID should be set to real UID");
        
        printf("  (Privileges dropped successfully) ");
    }
    
    return 1;
}

TEST_SUITE_BEGIN("Unit Tests - Security")
    RUN_TEST(test_sanitize_environment);
    RUN_TEST(test_check_privileges);
    RUN_TEST(test_setup_signal_handlers);
    RUN_TEST(test_set_current_username);
    RUN_TEST(test_validate_command_security);
    RUN_TEST(test_dangerous_command_detection);
    RUN_TEST(test_signal_handling);
    RUN_TEST(test_secure_terminal);
    RUN_TEST(test_init_security);
    RUN_TEST(test_is_interrupted);
    RUN_TEST(test_cleanup_security);
    RUN_TEST(test_drop_privileges);
TEST_SUITE_END()
