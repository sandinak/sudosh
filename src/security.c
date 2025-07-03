/**
 * security.c - Security Controls and Command Validation
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Implements comprehensive security controls including shell blocking,
 * dangerous command detection, signal handling, and input validation.
 */

#include "sudosh.h"

/* Global variables for signal handling */
static volatile sig_atomic_t interrupted = 0;
static volatile sig_atomic_t received_sigint = 0;
static char *current_username = NULL;

/**
 * Signal handler for cleanup
 */
void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
            /* Track SIGINT separately - don't set interrupted flag */
            /* This allows Ctrl-C to be passed to running commands */
            received_sigint = 1;
            break;
        case SIGTERM:
        case SIGQUIT:
            /* Set interrupted flag for graceful exit */
            interrupted = 1;
            /* Don't exit immediately - let main loop handle cleanup */
            break;
        case SIGTSTP:
            /* Ignore Ctrl-Z completely - don't suspend sudosh */
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
    sigaction(SIGTSTP, &sa, NULL);  /* Handle Ctrl-Z to ignore it */

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
 * Check if command is a shell or shell-like command
 */
int is_shell_command(const char *command) {
    if (!command) return 0;

    /* List of shells and shell-like commands to block */
    const char *shells[] = {
        "sh", "bash", "zsh", "csh", "tcsh", "ksh", "fish", "dash",
        "/bin/sh", "/bin/bash", "/bin/zsh", "/bin/csh", "/bin/tcsh",
        "/bin/ksh", "/bin/fish", "/bin/dash", "/usr/bin/bash",
        "/usr/bin/zsh", "/usr/bin/fish", "/usr/local/bin/bash",
        "/usr/local/bin/zsh", "/usr/local/bin/fish",
        "python", "python3", "perl", "ruby", "node", "nodejs",
        "/usr/bin/python", "/usr/bin/python3", "/usr/bin/perl",
        "/usr/bin/ruby", "/usr/bin/node", "/usr/bin/nodejs",
        "irb", "pry", "ipython", "ipython3",
        NULL
    };

    /* Extract the command name (first word) */
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;

    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }

    /* Check against shell list */
    for (int i = 0; shells[i]; i++) {
        if (strcmp(cmd_name, shells[i]) == 0) {
            free(cmd_copy);
            return 1; /* Shell command detected */
        }

        /* Also check basename for absolute paths */
        char *basename_shell = strrchr(shells[i], '/');
        if (basename_shell && strcmp(cmd_name, basename_shell + 1) == 0) {
            free(cmd_copy);
            return 1;
        }
    }

    /* Check for shell invocation patterns */
    if (strstr(command, " -c ") || strstr(command, " --command")) {
        free(cmd_copy);
        return 1;
    }

    free(cmd_copy);
    return 0;
}

/**
 * Check if command is dangerous (like init, shutdown, etc.)
 */
int is_dangerous_command(const char *command) {
    if (!command) return 0;

    /* List of dangerous commands */
    const char *dangerous[] = {
        "init", "shutdown", "halt", "reboot", "poweroff",
        "systemctl poweroff", "systemctl reboot", "systemctl halt",
        "systemctl emergency", "systemctl rescue",
        "/sbin/init", "/sbin/shutdown", "/sbin/halt", "/sbin/reboot",
        "/usr/sbin/shutdown", "/usr/sbin/halt", "/usr/sbin/reboot",
        "telinit", "/sbin/telinit", "/usr/sbin/telinit",
        "wall", "write", "mesg",
        "fdisk", "parted", "gparted", "mkfs", "fsck",
        "/sbin/fdisk", "/usr/sbin/fdisk", "/sbin/parted",
        "dd", "shred", "wipe",
        "iptables", "ip6tables", "ufw", "firewall-cmd",
        "/sbin/iptables", "/usr/sbin/iptables",
        "mount", "umount", "swapon", "swapoff",
        "/bin/mount", "/usr/bin/mount", "/sbin/mount",
        "crontab", "at", "batch",
        "su", "sudo", "pkexec",
        NULL
    };

    /* Extract the command name */
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;

    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }

    /* Check against dangerous command list */
    for (int i = 0; dangerous[i]; i++) {
        if (strcmp(cmd_name, dangerous[i]) == 0) {
            free(cmd_copy);
            return 1;
        }

        /* Check if command starts with dangerous command followed by space or end */
        size_t dangerous_len = strlen(dangerous[i]);
        if (strncmp(command, dangerous[i], dangerous_len) == 0) {
            char next_char = command[dangerous_len];
            if (next_char == '\0' || next_char == ' ' || next_char == '\t') {
                free(cmd_copy);
                return 1;
            }
        }
    }

    free(cmd_copy);
    return 0;
}

/**
 * Check for dangerous flags like -R, -rf, etc.
 */
int check_dangerous_flags(const char *command) {
    if (!command) return 0;

    /* Check for recursive flags */
    if (strstr(command, " -R") || strstr(command, " --recursive") ||
        strstr(command, " -rf") || strstr(command, " -Rf") ||
        strstr(command, " -fr") || strstr(command, " -fR")) {

        /* Check if it's with dangerous commands */
        if (strstr(command, "rm ") || strstr(command, "chmod ") ||
            strstr(command, "chown ") || strstr(command, "chgrp ")) {
            return 1;
        }
    }

    /* Check for force flags with rm */
    if ((strstr(command, "rm ") || strstr(command, "/bin/rm ")) &&
        (strstr(command, " -f") || strstr(command, " --force"))) {
        return 1;
    }

    return 0;
}

/**
 * Check if command accesses critical system directories
 */
int check_system_directory_access(const char *command) {
    if (!command) return 0;

    /* Critical system directories */
    const char *system_dirs[] = {
        "/dev", "/proc", "/sys", "/boot", "/etc",
        "/bin", "/sbin", "/usr/bin", "/usr/sbin",
        "/lib", "/lib64", "/usr/lib", "/usr/lib64",
        "/var/log", "/var/run", "/var/lib",
        "/root", "/home/root",
        NULL
    };

    /* Check if command references any system directories */
    for (int i = 0; system_dirs[i]; i++) {
        if (strstr(command, system_dirs[i])) {
            return 1;
        }
    }

    return 0;
}

/**
 * Prompt user for confirmation of dangerous command
 */
int prompt_user_confirmation(const char *command, const char *warning) {
    char response[10];

    printf("\n⚠️  WARNING: %s\n", warning);
    printf("Command: %s\n", command);
    printf("This command could be dangerous. Are you sure you want to proceed? (yes/no): ");
    fflush(stdout);

    if (fgets(response, sizeof(response), stdin) == NULL) {
        return 0; /* Default to no on error */
    }

    /* Trim newline */
    response[strcspn(response, "\n")] = '\0';

    /* Only accept explicit "yes" */
    if (strcmp(response, "yes") == 0) {
        printf("Proceeding with dangerous command...\n");
        return 1;
    }

    printf("Command cancelled for safety.\n");
    return 0;
}

/**
 * Enhanced command validation with shell and danger checks
 */
int validate_command(const char *command) {
    if (!command) {
        return 0;
    }

    /* Basic security checks first */

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

    /* Enhanced security checks */

    /* Block shell commands */
    if (is_shell_command(command)) {
        log_security_violation(current_username, "shell command blocked");
        fprintf(stderr, "sudosh: shell commands are not permitted\n");
        return 0;
    }

    /* Check for dangerous commands */
    if (is_dangerous_command(command)) {
        char violation_msg[256];
        snprintf(violation_msg, sizeof(violation_msg), "dangerous command attempted: %s", command);
        log_security_violation(current_username, violation_msg);

        if (!prompt_user_confirmation(command, "This is a potentially dangerous system command")) {
            return 0;
        }
    }

    /* Check for dangerous flags */
    if (check_dangerous_flags(command)) {
        char violation_msg[256];
        snprintf(violation_msg, sizeof(violation_msg), "dangerous flags detected: %s", command);
        log_security_violation(current_username, violation_msg);

        if (!prompt_user_confirmation(command, "This command uses dangerous recursive or force flags")) {
            return 0;
        }
    }

    /* Check for system directory access */
    if (check_system_directory_access(command)) {
        char violation_msg[256];
        snprintf(violation_msg, sizeof(violation_msg), "system directory access: %s", command);
        log_security_violation(current_username, violation_msg);

        if (!prompt_user_confirmation(command, "This command accesses critical system directories")) {
            return 0;
        }
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
 * Check if interrupted by signal (SIGTERM/SIGQUIT only)
 */
int is_interrupted(void) {
    return interrupted;
}

/**
 * Check if SIGINT was received (Ctrl-C)
 */
int received_sigint_signal(void) {
    return received_sigint;
}

/**
 * Reset SIGINT flag
 */
void reset_sigint_flag(void) {
    received_sigint = 0;
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
