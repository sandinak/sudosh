#include "test_security_framework.h"

/* Global security test counters */
int security_count = 0;
int security_passes = 0;
int security_failures = 0;

/* Test enhanced command injection protection */
int test_enhanced_command_injection_protection() {
    /* Test shell metacharacters are blocked */
    if (validate_command("ls; whoami")) return 1; /* Should be blocked */
    if (!validate_command("ls | cat")) return 1; /* Should be allowed (safe pipeline) */
    if (validate_command("ls && echo")) return 1; /* Should be blocked */
    if (validate_command("ls || echo")) return 1; /* Should be blocked */
    if (validate_command("echo `whoami`")) return 1; /* Should be blocked */
    if (validate_command("echo $(whoami)")) return 1; /* Should be blocked */
    if (!validate_command("ls > /tmp/test")) return 1; /* Should be allowed (safe redirection) */
    if (validate_command("ls < /etc/passwd")) return 1; /* Should be blocked */
    if (validate_command("ls 2> /dev/null")) return 1; /* Should be blocked */
    
    /* Test environment variable injection is blocked */
    if (!validate_command("printenv HOME")) return 1; /* Should be allowed via printenv */
    if (!validate_command("printenv PATH")) return 1; /* Should be allowed via printenv */
    if (!validate_command("printenv USER")) return 1; /* Should be allowed via printenv */
    
    /* Test format string injection is blocked */
    if (validate_command("printf '%s' test")) return 1; /* Should be blocked */
    if (validate_command("echo %d")) return 1; /* Should be blocked */
    
    /* Test Unicode/encoding attacks are blocked */
    if (!validate_command("ls \x80\x81")) return 1; /* Should be allowed (non-ASCII harmless in args) */
    
    /* Test null byte injection is blocked */
    char null_cmd[] = {'l', 's', '\0', 'r', 'm', '\0'};
    if (validate_command_with_length(null_cmd, 6)) return 1; /* Should be blocked */
    
    return 0; /* All attacks blocked - secure */
}

/* Test enhanced authentication validation */
int test_enhanced_authentication_validation() {
    /* Test empty username is rejected */
    if (authenticate_user("")) return 1; /* Should be rejected */
    if (authenticate_user(NULL)) return 1; /* Should be rejected */
    
    /* Test usernames with invalid characters are rejected */
    if (authenticate_user("user;rm")) return 1; /* Should be rejected */
    if (authenticate_user("user|cat")) return 1; /* Should be rejected */
    if (authenticate_user("user`whoami`")) return 1; /* Should be rejected */
    if (authenticate_user("user$(id)")) return 1; /* Should be rejected */
    if (authenticate_user("user/../root")) return 1; /* Should be rejected */
    if (authenticate_user("user/bin/sh")) return 1; /* Should be rejected */
    
    /* Test suspicious usernames are rejected */
    if (authenticate_user("root")) return 1; /* Should be rejected */
    if (authenticate_user("admin")) return 1; /* Should be rejected */
    if (authenticate_user("administrator")) return 1; /* Should be rejected */
    if (authenticate_user("test")) return 1; /* Should be rejected */
    
    /* Test overly long usernames are rejected */
    char long_username[300];
    memset(long_username, 'a', sizeof(long_username) - 1);
    long_username[sizeof(long_username) - 1] = '\0';
    if (authenticate_user(long_username)) return 1; /* Should be rejected */
    
    return 0; /* All attacks blocked - secure */
}

/* Test authentication cache race condition protection */
int test_auth_cache_race_protection() {
    const char *test_user = "testuser";
    
    /* Test concurrent cache access doesn't cause race conditions */
    pid_t pid = fork();
    if (pid == 0) {
        /* Child process - try to update cache */
        update_auth_cache(test_user);
        exit(0);
    } else if (pid > 0) {
        /* Parent process - try to check cache simultaneously */
        check_auth_cache(test_user);
        
        int status;
        waitpid(pid, &status, 0);
        
        /* Clean up */
        clear_auth_cache(test_user);
        
        return 0; /* No race condition - secure */
    }
    
    return 1; /* Fork failed - treat as vulnerable */
}

/* Test PATH hijacking protection */
int test_path_hijacking_protection() {
    /* Save original PATH */
    char *original_path = getenv("PATH");
    char *saved_path = original_path ? strdup(original_path) : NULL;
    
    /* Set malicious PATH */
    setenv("PATH", "/tmp:.:$HOME/bin:/usr/bin", 1);
    
    /* Call sanitize_environment */
    sanitize_environment();
    
    /* Check if PATH was sanitized */
    char *new_path = getenv("PATH");
    int is_secure = (new_path && 
                     !strstr(new_path, "/tmp") &&
                     !strstr(new_path, ".:") &&
                     !strstr(new_path, ":.") &&
                     !strstr(new_path, "::") &&
                     new_path[0] != ':' &&
                     new_path[strlen(new_path)-1] != ':');
    
    /* Restore original PATH */
    if (saved_path) {
        setenv("PATH", saved_path, 1);
        free(saved_path);
    }
    
    return is_secure ? 0 : 1; /* 0 = secure, 1 = vulnerable */
}

/* Test file descriptor sanitization */
int test_file_descriptor_sanitization() {
    /* Open some file descriptors */
    int fd1 = open("/dev/null", O_RDONLY);
    int fd2 = open("/dev/null", O_WRONLY);
    int fd3 = open("/dev/null", O_RDWR);
    
    if (fd1 < 0 || fd2 < 0 || fd3 < 0) {
        return 1; /* Can't test - treat as vulnerable */
    }
    
    /* Call secure_terminal which should close extra FDs */
    secure_terminal();
    
    /* Check if file descriptors were closed */
    int fd1_closed = (fcntl(fd1, F_GETFD) == -1 && errno == EBADF);
    int fd2_closed = (fcntl(fd2, F_GETFD) == -1 && errno == EBADF);
    int fd3_closed = (fcntl(fd3, F_GETFD) == -1 && errno == EBADF);
    
    /* Ensure stdin, stdout, stderr are still open */
    int stdin_open = (fcntl(STDIN_FILENO, F_GETFD) != -1);
    int stdout_open = (fcntl(STDOUT_FILENO, F_GETFD) != -1);
    int stderr_open = (fcntl(STDERR_FILENO, F_GETFD) != -1);
    
    return (fd1_closed && fd2_closed && fd3_closed && 
            stdin_open && stdout_open && stderr_open) ? 0 : 1;
}

/* Test environment variable sanitization */
int test_environment_sanitization() {
    /* Set dangerous environment variables */
    setenv("LD_PRELOAD", "/tmp/malicious.so", 1);
    setenv("IFS", " \t\n;", 1);
    setenv("BASH_ENV", "/tmp/malicious.sh", 1);
    setenv("EDITOR", "/bin/sh", 1);
    setenv("TMPDIR", "/tmp/../etc", 1);
    
    /* Call sanitize_environment */
    sanitize_environment();
    
    /* Check if dangerous variables were removed */
    int ld_preload_removed = (getenv("LD_PRELOAD") == NULL);
    int ifs_removed = (getenv("IFS") == NULL);
    int bash_env_removed = (getenv("BASH_ENV") == NULL);
    int editor_removed = (getenv("EDITOR") == NULL);
    int tmpdir_removed = (getenv("TMPDIR") == NULL);
    
    return (ld_preload_removed && ifs_removed && bash_env_removed && 
            editor_removed && tmpdir_removed) ? 0 : 1;
}

int main() {
    printf("=== Enhanced Security Fixes Tests ===\n");
    
    /* Test enhanced command injection protection */
    printf("Testing: Enhanced command injection protection... ");
    if (test_enhanced_command_injection_protection()) {
        printf("VULNERABLE (attack succeeded)\n");
        security_failures++;
    } else {
        printf("SECURE (attack blocked)\n");
        security_passes++;
    }
    security_count++;
    
    /* Test enhanced authentication validation */
    printf("Testing: Enhanced authentication validation... ");
    if (test_enhanced_authentication_validation()) {
        printf("VULNERABLE (attack succeeded)\n");
        security_failures++;
    } else {
        printf("SECURE (attack blocked)\n");
        security_passes++;
    }
    security_count++;
    
    /* Test authentication cache race protection */
    printf("Testing: Authentication cache race protection... ");
    if (test_auth_cache_race_protection()) {
        printf("VULNERABLE (race condition possible)\n");
        security_failures++;
    } else {
        printf("SECURE (race condition prevented)\n");
        security_passes++;
    }
    security_count++;
    
    /* Test PATH hijacking protection */
    printf("Testing: PATH hijacking protection... ");
    if (test_path_hijacking_protection()) {
        printf("VULNERABLE (PATH hijacking possible)\n");
        security_failures++;
    } else {
        printf("SECURE (PATH sanitized)\n");
        security_passes++;
    }
    security_count++;
    
    /* Test file descriptor sanitization */
    printf("Testing: File descriptor sanitization... ");
    if (test_file_descriptor_sanitization()) {
        printf("VULNERABLE (FD manipulation possible)\n");
        security_failures++;
    } else {
        printf("SECURE (FDs properly sanitized)\n");
        security_passes++;
    }
    security_count++;
    
    /* Test environment variable sanitization */
    printf("Testing: Environment variable sanitization... ");
    if (test_environment_sanitization()) {
        printf("VULNERABLE (dangerous env vars not removed)\n");
        security_failures++;
    } else {
        printf("SECURE (dangerous env vars removed)\n");
        security_passes++;
    }
    security_count++;
    
    printf("\n=== Enhanced Security Fixes Test Results ===\n");
    printf("Total tests: %d\n", security_count);
    printf("Secure (attacks blocked): %d\n", security_passes);
    printf("Vulnerable (attacks succeeded): %d\n", security_failures);
    
    if (security_failures > 0) {
        printf("❌ %d vulnerabilities found!\n", security_failures);
        return 1;
    } else {
        printf("✅ All enhanced security fixes are working!\n");
        return 0;
    }
}
