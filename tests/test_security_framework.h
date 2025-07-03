#ifndef TEST_SECURITY_FRAMEWORK_H
#define TEST_SECURITY_FRAMEWORK_H

#include "test_framework.h"
#include "sudosh.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/resource.h>

/* Security test categories */
typedef enum {
    SECURITY_TEST_COMMAND_INJECTION,
    SECURITY_TEST_PRIVILEGE_ESCALATION,
    SECURITY_TEST_AUTH_BYPASS,
    SECURITY_TEST_LOGGING_EVASION,
    SECURITY_TEST_RACE_CONDITION,
    SECURITY_TEST_ENVIRONMENT_MANIPULATION,
    SECURITY_TEST_PATH_TRAVERSAL,
    SECURITY_TEST_BUFFER_OVERFLOW,
    SECURITY_TEST_SIGNAL_MANIPULATION,
    SECURITY_TEST_FILE_DESCRIPTOR_MANIPULATION
} security_test_category_t;

/* Security test result structure */
typedef struct {
    security_test_category_t category;
    char *test_name;
    char *attack_vector;
    int vulnerability_found;
    char *description;
    char *mitigation;
    int severity; /* 1-10 scale */
} security_test_result_t;

/* Security test context */
typedef struct {
    char *test_binary_path;
    char *temp_dir;
    char *log_file;
    uid_t original_uid;
    gid_t original_gid;
    int test_count;
    int vulnerabilities_found;
    security_test_result_t *results;
} security_test_context_t;

/* Security test macros */
#define SECURITY_TEST_ASSERT_BLOCKED(test_func, attack_desc) \
    do { \
        printf("Testing: %s... ", attack_desc); \
        fflush(stdout); \
        int result = test_func(); \
        if (result == 0) { \
            printf("SECURE (attack blocked)\n"); \
            security_passes++; \
        } else { \
            printf("VULNERABLE (attack succeeded)\n"); \
            security_failures++; \
            log_vulnerability(attack_desc, result); \
        } \
        security_count++; \
    } while(0)

#define SECURITY_TEST_ASSERT_LOGGED(test_func, attack_desc) \
    do { \
        printf("Testing logging for: %s... ", attack_desc); \
        fflush(stdout); \
        int logged = test_func(); \
        if (logged) { \
            printf("SECURE (attack logged)\n"); \
            security_passes++; \
        } else { \
            printf("VULNERABLE (attack not logged)\n"); \
            security_failures++; \
            log_vulnerability(attack_desc, 0); \
        } \
        security_count++; \
    } while(0)

#define SECURITY_TEST_ASSERT_SANITIZED(test_func, attack_desc) \
    do { \
        printf("Testing sanitization for: %s... ", attack_desc); \
        fflush(stdout); \
        int sanitized = test_func(); \
        if (sanitized) { \
            printf("SECURE (input sanitized)\n"); \
            security_passes++; \
        } else { \
            printf("VULNERABLE (input not sanitized)\n"); \
            security_failures++; \
            log_vulnerability(attack_desc, 0); \
        } \
        security_count++; \
    } while(0)

/* Global security test counters */
extern int security_count;
extern int security_passes;
extern int security_failures;

/* Security test utility functions */

/**
 * Initialize security test environment
 */
static inline security_test_context_t *init_security_test_context(void) {
    security_test_context_t *ctx = malloc(sizeof(security_test_context_t));
    if (!ctx) return NULL;
    
    ctx->test_binary_path = strdup("./bin/sudosh");
    ctx->temp_dir = strdup("/tmp/sudosh_security_test");
    ctx->log_file = strdup("/tmp/sudosh_security.log");
    ctx->original_uid = getuid();
    ctx->original_gid = getgid();
    ctx->test_count = 0;
    ctx->vulnerabilities_found = 0;
    ctx->results = NULL;
    
    /* Create temp directory */
    mkdir(ctx->temp_dir, 0755);
    
    return ctx;
}

/**
 * Cleanup security test environment
 */
static inline void cleanup_security_test_context(security_test_context_t *ctx) {
    if (!ctx) return;
    
    free(ctx->test_binary_path);
    free(ctx->temp_dir);
    free(ctx->log_file);
    
    if (ctx->results) {
        for (int i = 0; i < ctx->test_count; i++) {
            free(ctx->results[i].test_name);
            free(ctx->results[i].attack_vector);
            free(ctx->results[i].description);
            free(ctx->results[i].mitigation);
        }
        free(ctx->results);
    }
    
    free(ctx);
}

/**
 * Create a malicious file for testing
 */
static inline char *create_malicious_file(const char *content, const char *filename) {
    char *filepath = malloc(256);
    snprintf(filepath, 256, "/tmp/%s", filename);
    
    int fd = open(filepath, O_CREAT | O_WRONLY | O_TRUNC, 0755);
    if (fd == -1) {
        free(filepath);
        return NULL;
    }
    
    write(fd, content, strlen(content));
    close(fd);
    
    return filepath;
}

/**
 * Execute command and capture output with timeout
 */
static inline int execute_with_timeout(const char *command, int timeout_seconds, char **output) {
    int pipefd[2];
    pid_t pid;
    int status;
    
    if (pipe(pipefd) == -1) return -1;
    
    pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    if (pid == 0) {
        /* Child process */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        
        execl("/bin/sh", "sh", "-c", command, NULL);
        exit(127);
    } else {
        /* Parent process */
        close(pipefd[1]);
        
        /* Set up timeout */
        alarm(timeout_seconds);
        
        /* Wait for child */
        if (waitpid(pid, &status, 0) == -1) {
            if (errno == EINTR) {
                /* Timeout occurred */
                kill(pid, SIGKILL);
                waitpid(pid, NULL, 0);
                close(pipefd[0]);
                return -2; /* Timeout */
            }
        }
        
        alarm(0);
        
        /* Read output */
        if (output) {
            char buffer[4096];
            ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                *output = strdup(buffer);
            } else {
                *output = strdup("");
            }
        }
        
        close(pipefd[0]);
        return WEXITSTATUS(status);
    }
}

/**
 * Check if a process is running as root
 */
static inline int is_running_as_root(pid_t pid) {
    char path[256];
    char buffer[1024];
    FILE *file;
    
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    file = fopen(path, "r");
    if (!file) return 0;
    
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strncmp(buffer, "Uid:", 4) == 0) {
            int real_uid, effective_uid;
            if (sscanf(buffer, "Uid:\t%d\t%d", &real_uid, &effective_uid) == 2) {
                fclose(file);
                return (effective_uid == 0);
            }
        }
    }
    
    fclose(file);
    return 0;
}

/**
 * Log vulnerability finding
 */
static inline void log_vulnerability(const char *attack_desc, int severity) {
    FILE *log = fopen("/tmp/sudosh_vulnerabilities.log", "a");
    if (log) {
        time_t now = time(NULL);
        fprintf(log, "[%s] VULNERABILITY: %s (Severity: %d)\n", 
                ctime(&now), attack_desc, severity);
        fclose(log);
    }
}

/**
 * Check if command was logged to syslog
 */
static inline int check_syslog_entry(const char *expected_entry) {
    /* This is a simplified check - in real testing you'd parse actual syslog */
    FILE *log = fopen("/var/log/syslog", "r");
    if (!log) {
        /* Try alternative log locations */
        log = fopen("/var/log/messages", "r");
        if (!log) return 0;
    }
    
    char line[1024];
    int found = 0;
    
    /* Check last 100 lines for the entry */
    while (fgets(line, sizeof(line), log) && !found) {
        if (strstr(line, "sudosh") && strstr(line, expected_entry)) {
            found = 1;
        }
    }
    
    fclose(log);
    return found;
}

#endif /* TEST_SECURITY_FRAMEWORK_H */
