#include "sudosh.h"

/* Global variables for signal handling */
static volatile sig_atomic_t interrupted = 0;
static char *current_username = NULL;

/**
 * Signal handler for cleanup
 */
void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
        case SIGQUIT:
            interrupted = 1;
            if (current_username) {
                log_session_end(current_username);
            }
            close_logging();
            exit(EXIT_FAILURE);
            break;
        case SIGCHLD:
            /* Don't reap children here - let execute_command handle it */
            /* This prevents the "waitpid: no child process" error */
            break;
    }
}

/**
 * Setup signal handlers for security
 */
void setup_signal_handlers(void) {
    struct sigaction sa;

    /* Setup signal handler */
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    /* Handle termination signals */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    /* Handle child process signals - but don't reap them */
    sigaction(SIGCHLD, &sa, NULL);

    /* Ignore some signals */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
}

/**
 * Set current username for signal handler
 */
void set_current_username(const char *username) {
    if (current_username) {
        free(current_username);
    }
    current_username = username ? strdup(username) : NULL;
}

/**
 * Sanitize environment variables for security
 */
void sanitize_environment(void) {
    /* List of dangerous environment variables to remove */
    const char *dangerous_vars[] = {
        "IFS",
        "CDPATH", 
        "ENV",
        "BASH_ENV",
        "LD_PRELOAD",
        "LD_LIBRARY_PATH",
        "SHLIB_PATH",
        "LIBPATH",
        "DYLD_LIBRARY_PATH",
        "DYLD_INSERT_LIBRARIES",
        "DYLD_FORCE_FLAT_NAMESPACE",
        "TMPDIR",
        "TMP",
        "TEMP",
        NULL
    };

    int i;

    /* Remove dangerous variables */
    for (i = 0; dangerous_vars[i]; i++) {
        unsetenv(dangerous_vars[i]);
    }

    /* Set secure PATH if not already set */
    if (!getenv("PATH")) {
        setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
    }

    /* Ensure HOME is set to root's home when running as root */
    setenv("HOME", "/root", 1);
    setenv("USER", "root", 1);
    setenv("LOGNAME", "root", 1);

    /* Set secure umask */
    umask(022);
}

/**
 * Drop privileges back to original user
 */
void drop_privileges(void) {
    uid_t real_uid = getuid();
    gid_t real_gid = getgid();

    /* Drop group privileges first */
    if (setgid(real_gid) != 0) {
        perror("setgid");
        exit(EXIT_FAILURE);
    }

    /* Drop user privileges */
    if (setuid(real_uid) != 0) {
        perror("setuid");
        exit(EXIT_FAILURE);
    }
}

/**
 * Check if we're running with appropriate privileges
 */
int check_privileges(void) {
    uid_t euid = geteuid();
    
    /* We need to be setuid root or running as root */
    if (euid != 0) {
        fprintf(stderr, "sudosh: must be run as root or with setuid bit\n");
        return 0;
    }

    return 1;
}

/**
 * Secure the terminal
 */
void secure_terminal(void) {
    /* Disable core dumps for security */
    struct rlimit rlim;
    rlim.rlim_cur = 0;
    rlim.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rlim);

    /* Set process group */
    if (setsid() == -1) {
        /* Not a problem if we're already a session leader */
    }
}

/**
 * Validate command for security
 */
int validate_command(const char *command) {
    if (!command) {
        return 0;
    }

    /* Check for null bytes (potential injection) */
    if (strlen(command) != strcspn(command, "\0")) {
        log_security_violation(current_username, "null byte in command");
        return 0;
    }

    /* Check for extremely long commands */
    if (strlen(command) > MAX_COMMAND_LENGTH) {
        log_security_violation(current_username, "command too long");
        return 0;
    }

    /* Check for suspicious patterns */
    if (strstr(command, "../") || strstr(command, "..\\")) {
        log_security_violation(current_username, "path traversal attempt");
        return 0;
    }

    return 1;
}

/**
 * Initialize security measures
 */
void init_security(void) {
    /* Check if we have appropriate privileges */
    if (!check_privileges()) {
        exit(EXIT_FAILURE);
    }

    /* Setup signal handlers */
    setup_signal_handlers();

    /* Secure the terminal */
    secure_terminal();

    /* Sanitize environment */
    sanitize_environment();
}

/**
 * Check if interrupted by signal
 */
int is_interrupted(void) {
    return interrupted;
}

/**
 * Cleanup security resources
 */
void cleanup_security(void) {
    if (current_username) {
        free(current_username);
        current_username = NULL;
    }
}
