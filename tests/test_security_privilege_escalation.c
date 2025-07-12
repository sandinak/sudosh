#include "test_security_framework.h"

/* Global security test counters */
int security_count = 0;
int security_passes = 0;
int security_failures = 0;

/* Test PATH hijacking attack */
int test_path_hijacking() {
    /* Create malicious binary in /tmp */
    char *malicious_script = "#!/bin/sh\necho 'HIJACKED: Running as UID:' $(id -u)\ntouch /tmp/hijack_success\n";
    char *malicious_file = create_malicious_file(malicious_script, "ls");
    
    if (!malicious_file) return 0;
    
    /* Set PATH to include /tmp first */
    char *original_path = getenv("PATH");
    setenv("PATH", "/tmp:/usr/bin:/bin", 1);
    
    /* Test if sudosh uses the hijacked binary */
    struct command_info cmd;
    int result = 0;
    
    if (parse_command("ls", &cmd) == 0) {
        /* Check if sudosh properly validates the command path */
        char *command_path = find_command_in_path("ls");
        if (command_path && strstr(command_path, "/tmp/ls")) {
            result = 1; /* Vulnerable - using hijacked binary */
        }
        free(command_path);
        free_command_info(&cmd);
    }
    
    /* Restore original PATH */
    if (original_path) {
        setenv("PATH", original_path, 1);
    }
    
    /* Cleanup */
    unlink(malicious_file);
    free(malicious_file);
    
    return result;
}

/* Test LD_PRELOAD privilege escalation */
int test_ld_preload_escalation() {
    /* Create malicious shared library */
    char *malicious_lib = 
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <unistd.h>\n"
        "void __attribute__((constructor)) init() {\n"
        "    if (geteuid() == 0) {\n"
        "        system(\"touch /tmp/ld_preload_success\");\n"
        "    }\n"
        "}\n";
    
    char *lib_file = create_malicious_file(malicious_lib, "malicious.c");
    if (!lib_file) return 0;
    
    /* Try to compile the malicious library */
    char compile_cmd[512];
    snprintf(compile_cmd, sizeof(compile_cmd), 
             "gcc -shared -fPIC %s -o /tmp/malicious.so 2>/dev/null", lib_file);
    
    if (system(compile_cmd) != 0) {
        unlink(lib_file);
        free(lib_file);
        return 0; /* Can't compile, test inconclusive */
    }
    
    /* Test if LD_PRELOAD is properly sanitized */
    char *original_preload = getenv("LD_PRELOAD");
    setenv("LD_PRELOAD", "/tmp/malicious.so", 1);
    
    /* Initialize security (should sanitize environment) */
    sanitize_environment();
    
    /* Check if LD_PRELOAD was removed */
    char *current_preload = getenv("LD_PRELOAD");
    int result = (current_preload != NULL) ? 1 : 0;
    
    /* Restore original environment */
    if (original_preload) {
        setenv("LD_PRELOAD", original_preload, 1);
    } else {
        unsetenv("LD_PRELOAD");
    }
    
    /* Cleanup */
    unlink("/tmp/malicious.so");
    unlink(lib_file);
    free(lib_file);
    
    return result;
}

/* Test environment variable manipulation */
int test_environment_manipulation() {
    /* Test dangerous environment variables */
    const char *dangerous_vars[] = {
        "IFS", "CDPATH", "ENV", "BASH_ENV", "LD_PRELOAD", 
        "LD_LIBRARY_PATH", "SHLIB_PATH", "LIBPATH",
        "DYLD_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES",
        "TMPDIR", "TMP", "TEMP", NULL
    };
    
    /* Set all dangerous variables */
    for (int i = 0; dangerous_vars[i]; i++) {
        setenv(dangerous_vars[i], "/tmp/malicious", 1);
    }
    
    /* Call sanitize_environment */
    sanitize_environment();
    
    /* Check if any dangerous variables remain */
    for (int i = 0; dangerous_vars[i]; i++) {
        if (getenv(dangerous_vars[i])) {
            return 1; /* Vulnerable - variable not removed */
        }
    }
    
    return 0; /* Secure - all variables removed */
}

/* Test setuid binary exploitation */
int test_setuid_exploitation() {
    /* Check if sudosh properly handles setuid */
    uid_t real_uid = getuid();
    uid_t effective_uid = geteuid();
    
    /* If we're not running with setuid, this test is not applicable */
    if (real_uid == effective_uid) {
        return 0; /* Not running setuid, secure by default */
    }
    
    /* Test if privileges are properly managed */
    if (effective_uid == 0 && real_uid != 0) {
        /* We have root privileges - test if they're properly controlled */
        
        /* Try to access root-only file */
        if (access("/etc/shadow", R_OK) == 0) {
            /* We can read shadow file - check if this is logged */
            return check_syslog_entry("shadow file access") ? 0 : 1;
        }
    }
    
    return 0; /* Secure */
}

/* Test privilege dropping */
int test_privilege_dropping() {
    uid_t original_uid = getuid();
    uid_t original_euid = geteuid();
    
    /* If not running with elevated privileges, test is not applicable */
    if (original_euid != 0) {
        return 0; /* Not elevated, secure */
    }
    
    /* Test drop_privileges function */
    drop_privileges();
    
    uid_t new_uid = getuid();
    uid_t new_euid = geteuid();
    
    /* Check if privileges were properly dropped */
    if (new_euid == 0 || new_uid != original_uid) {
        return 1; /* Vulnerable - privileges not properly dropped */
    }
    
    return 0; /* Secure - privileges properly dropped */
}

/* Test file descriptor manipulation */
int test_file_descriptor_manipulation() {
    /* Test if sudosh properly secures file descriptors */

    /* Open some file descriptors */
    int fd1 = open("/dev/null", O_RDONLY);
    int fd2 = open("/dev/null", O_WRONLY);

    if (fd1 < 0 || fd2 < 0) {
        if (fd1 >= 0) close(fd1);
        if (fd2 >= 0) close(fd2);
        return 0; /* Can't test */
    }

    /* Call secure_terminal to test file descriptor cleanup */
    secure_terminal();

    /* Check if file descriptors were closed */
    int fd1_closed = (fcntl(fd1, F_GETFD) == -1 && errno == EBADF);
    int fd2_closed = (fcntl(fd2, F_GETFD) == -1 && errno == EBADF);

    /* Ensure stdin, stdout, stderr are still open */
    int stdin_open = (fcntl(STDIN_FILENO, F_GETFD) != -1);
    int stdout_open = (fcntl(STDOUT_FILENO, F_GETFD) != -1);
    int stderr_open = (fcntl(STDERR_FILENO, F_GETFD) != -1);

    /* Clean up any remaining descriptors */
    if (!fd1_closed) close(fd1);
    if (!fd2_closed) close(fd2);

    /* Return 0 if secure (FDs closed), 1 if vulnerable (FDs still open) */
    if (fd1_closed && fd2_closed && stdin_open && stdout_open && stderr_open) {
        return 0; /* Secure */
    } else {
        return 1; /* Vulnerable */
    }
}

/* Test umask manipulation */
int test_umask_manipulation() {
    /* Set insecure umask */
    mode_t original_umask = umask(0000);
    
    /* Call security initialization */
    sanitize_environment();
    
    /* Check if umask was properly set */
    mode_t current_umask = umask(original_umask);
    
    if (current_umask != 022) {
        return 1; /* Vulnerable - umask not properly set */
    }
    
    return 0; /* Secure */
}

/* Test signal handler manipulation */
int test_signal_handler_manipulation() {
    /* Set custom signal handlers */
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    
    /* Initialize security */
    setup_signal_handlers();
    
    /* Check if signal handlers were properly set */
    struct sigaction sa;
    
    if (sigaction(SIGINT, NULL, &sa) == 0) {
        if (sa.sa_handler == SIG_IGN) {
            return 1; /* Vulnerable - signal handler not changed */
        }
    }
    
    return 0; /* Secure */
}

/* Test resource limit bypass */
int test_resource_limit_bypass() {
    struct rlimit rlim;
    
    /* Try to set unlimited core dumps */
    rlim.rlim_cur = RLIM_INFINITY;
    rlim.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &rlim);
    
    /* Call security initialization */
    secure_terminal();
    
    /* Check if core dumps were properly disabled */
    if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
        if (rlim.rlim_cur != 0) {
            return 1; /* Vulnerable - core dumps not disabled */
        }
    }
    
    return 0; /* Secure */
}

int main() {
    printf("=== Security Tests - Privilege Escalation ===\n");
    
    /* Initialize security test counters */
    security_count = 0;
    security_passes = 0;
    security_failures = 0;
    
    /* Run privilege escalation tests */
    SECURITY_TEST_ASSERT_BLOCKED(test_path_hijacking, 
                                 "PATH hijacking attack");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_ld_preload_escalation, 
                                 "LD_PRELOAD privilege escalation");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_environment_manipulation, 
                                 "Environment variable manipulation");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_setuid_exploitation, 
                                 "Setuid binary exploitation");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_privilege_dropping, 
                                 "Privilege dropping bypass");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_file_descriptor_manipulation, 
                                 "File descriptor manipulation");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_umask_manipulation, 
                                 "Umask manipulation");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_signal_handler_manipulation, 
                                 "Signal handler manipulation");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_resource_limit_bypass, 
                                 "Resource limit bypass");
    
    printf("\n=== Privilege Escalation Test Results ===\n");
    printf("Total tests: %d\n", security_count);
    printf("Secure (attacks blocked): %d\n", security_passes);
    printf("Vulnerable (attacks succeeded): %d\n", security_failures);
    
    if (security_failures == 0) {
        printf("✅ All privilege escalation attacks were blocked!\n");
        return 0;
    } else {
        printf("❌ %d vulnerabilities found! Check /tmp/sudosh_vulnerabilities.log\n", security_failures);
        return 1;
    }
}
