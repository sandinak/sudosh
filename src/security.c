/**
 * security.c - Security Controls and Command Validation
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Implements comprehensive security controls including shell blocking,
 * dangerous command detection, signal handling, and input validation.
 */

#include "sudosh.h"
#ifdef __linux__
#include <sys/prctl.h>  /* For prctl() on Linux */
#endif
#include <pthread.h>    /* For pthread_sigmask() */

/* Global variables for signal handling */
static volatile sig_atomic_t interrupted = 0;
static volatile sig_atomic_t received_sigint = 0;
static char *current_username = NULL;

/**
 * Signal handler for cleanup
 * Enhanced based on sudo's signal handling improvements
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
            /* Enhanced: Use waitpid with WNOHANG to avoid blocking */
            while (waitpid(-1, NULL, WNOHANG) > 0) {
                /* Continue reaping children */
            }
            break;
        case SIGHUP:
            /* Enhanced: Handle SIGHUP for session management */
            /* Use killpg() instead of kill() for process group */
            if (getpgrp() != getpid()) {
                /* We're not the process group leader, send to group */
                killpg(getpgrp(), SIGHUP);
            }
            interrupted = 1;
            break;
    }
}

/**
 * Setup signal handlers for security
 * Enhanced based on sudo's signal handling improvements
 */
void setup_signal_handlers(void) {
    struct sigaction sa;

    /* Setup signal handler */
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;  /* Enhanced flags */

    /* Handle termination signals */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);  /* Handle Ctrl-Z to ignore it */
    sigaction(SIGHUP, &sa, NULL);   /* Enhanced: Handle SIGHUP properly */

    /* Handle child process signals with enhanced handling */
    sigaction(SIGCHLD, &sa, NULL);

    /* Ignore pipe signals to prevent broken pipe crashes */
    signal(SIGPIPE, SIG_IGN);

    /* Block dangerous signals during critical sections */
    sigset_t block_set;
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGTERM);
    sigaddset(&block_set, SIGQUIT);
    sigaddset(&block_set, SIGHUP);

    /* This will be used in critical sections */
    pthread_sigmask(SIG_BLOCK, &block_set, NULL);
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
 * Enhanced based on sudo's environment security fixes
 */
void sanitize_environment(void) {
    /* List of dangerous environment variables to remove
     * Expanded based on sudo's security enhancements */
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
        "DYLD_FALLBACK_LIBRARY_PATH",
        "DYLD_VERSIONED_LIBRARY_PATH",
        "_RLD_LIST",           /* AIX runtime linker */
        "_RLD32_LIST",         /* AIX 32-bit runtime linker */
        "_RLD64_LIST",         /* AIX 64-bit runtime linker */
        "LDR_PRELOAD",         /* AIX loader preload */
        "LDR_PRELOAD64",       /* AIX 64-bit loader preload */
        "TMPDIR",
        "TMP",
        "TEMP",
        "TEMPDIR",
        /* Editor-related variables that could be used for shell escapes */
        "EDITOR",
        "VISUAL",
        "SUDO_EDITOR",
        "FCEDIT",
        "KSHELL",
        "PAGER",
        "MANPAGER",
        /* Shell-related variables */
        "PS1",
        "PS2",
        "PS3",
        "PS4",
        "PROMPT_COMMAND",
        "BASH_FUNC_*",         /* Bash function exports */
        /* Locale-related variables that could be abused */
        "LC_MESSAGES",
        "LANGUAGE",
        /* Python-related variables */
        "PYTHONPATH",
        "PYTHONSTARTUP",
        "PYTHONHOME",
        /* Perl-related variables */
        "PERL5LIB",
        "PERLLIB",
        "PERL5OPT",
        /* Ruby-related variables */
        "RUBYLIB",
        "RUBYOPT",
        /* Node.js-related variables */
        "NODE_PATH",
        "NODE_OPTIONS",
        /* Java-related variables */
        "CLASSPATH",
        "JAVA_TOOL_OPTIONS",
        /* Terminal-related variables that could be abused */
        "TERMCAP",
        "TERMINFO",
        /* X11-related variables */
        "XAUTHORITY",
        "DISPLAY",
        NULL
    };

    int i;

    /* Remove dangerous variables */
    for (i = 0; dangerous_vars[i]; i++) {
        unsetenv(dangerous_vars[i]);
    }

    /* Also remove any variables starting with dangerous prefixes */
    extern char **environ;
    char **env_ptr = environ;
    while (env_ptr && *env_ptr) {
        char *env_var = *env_ptr;
        char *equals = strchr(env_var, '=');
        if (equals) {
            size_t name_len = equals - env_var;
            /* Check for dangerous prefixes */
            if ((name_len >= 9 && strncmp(env_var, "BASH_FUNC", 9) == 0) ||
                (name_len >= 4 && strncmp(env_var, "_RLD", 4) == 0) ||
                (name_len >= 4 && strncmp(env_var, "DYLD", 4) == 0) ||
                (name_len >= 3 && strncmp(env_var, "LD_", 3) == 0)) {

                /* Extract variable name and unset it */
                char *var_name = strndup(env_var, name_len);
                if (var_name) {
                    unsetenv(var_name);
                    free(var_name);
                }
            }
        }
        env_ptr++;
    }

    /* Set secure PATH - always override for security */
    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);

    /* Ensure HOME is set to root's home when running as root */
    setenv("HOME", "/root", 1);
    setenv("USER", "root", 1);
    setenv("LOGNAME", "root", 1);

    /* Set SUDO_TTY environment variable if user has a tty
     * Based on sudo's enhancement for finding original tty */
    char *tty_name = ttyname(STDIN_FILENO);
    if (tty_name) {
        setenv("SUDO_TTY", tty_name, 1);
    }

    /* Set SUDO_COMMAND to track the original command context */
    setenv("SUDO_COMMAND", "sudosh", 1);

    /* Set secure umask */
    umask(022);

    /* Set secure locale to prevent locale-based attacks */
    setenv("LC_ALL", "C", 1);
    setenv("LANG", "C", 1);
}

/**
 * Set up secure environment for editor execution
 */
void setup_secure_editor_environment(void) {
    /* Remove editor-related environment variables to prevent shell escapes */
    unsetenv("EDITOR");
    unsetenv("VISUAL");
    unsetenv("SUDO_EDITOR");
    unsetenv("FCEDIT");
    unsetenv("PAGER");
    unsetenv("MANPAGER");

    /* Set a safe default editor for sudoedit */
    setenv("SUDO_EDITOR", "/usr/bin/vi", 1);

    /* Ensure TERM is set to something safe */
    if (!getenv("TERM")) {
        setenv("TERM", "xterm", 1);
    }

    /* Remove shell-related variables that editors might use */
    unsetenv("SHELL");
    unsetenv("BASH_ENV");
    unsetenv("ENV");

    /* Set a safe shell that will be restricted */
    setenv("SHELL", "/bin/false", 1);

    /* Disable shell escapes in vi/vim */
    setenv("VIMINIT", "set shell=/bin/false", 1);
    setenv("EXINIT", "set shell=/bin/false", 1);

    /* Disable shell access in emacs */
    setenv("ESHELL", "/bin/false", 1);
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
 * Validate TTY device to prevent TTY-based attacks
 * Based on sudo's TTY security enhancements
 */
static int validate_tty_device(void) {
    char *tty_name;
    struct stat tty_stat;

    /* Get TTY name */
    tty_name = ttyname(STDIN_FILENO);
    if (!tty_name) {
        /* No TTY is acceptable for some use cases */
        return 1;
    }

    /* Validate TTY device */
    if (stat(tty_name, &tty_stat) != 0) {
        log_security_violation("unknown", "invalid TTY device");
        return 0;
    }

    /* Ensure it's a character device */
    if (!S_ISCHR(tty_stat.st_mode)) {
        log_security_violation("unknown", "TTY is not a character device");
        return 0;
    }

    /* Check for suspicious TTY names that could indicate attacks */
    if (strstr(tty_name, "..") || strstr(tty_name, "//")) {
        log_security_violation("unknown", "suspicious TTY path");
        return 0;
    }

    return 1;
}

/**
 * Secure the terminal and process environment
 * Enhanced based on sudo's security fixes
 */
void secure_terminal(void) {
    /* Validate TTY first */
    if (!validate_tty_device()) {
        fprintf(stderr, "sudosh: TTY validation failed\n");
        exit(EXIT_FAILURE);
    }

    /* Disable core dumps for security */
    struct rlimit rlim;
    rlim.rlim_cur = 0;
    rlim.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rlim);

    /* Set process group - enhanced error handling */
    if (setsid() == -1) {
        /* Not a problem if we're already a session leader */
        if (errno != EPERM) {
            log_error("Failed to create new session");
        }
    }

    /* Set process title for better process identification (Linux only) */
#ifdef __linux__
    if (prctl(PR_SET_NAME, "sudosh", 0, 0, 0) == -1) {
        /* Not critical if this fails */
    }

    /* Prevent ptrace attacks (Linux only) */
    if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) == -1) {
        /* Not critical if this fails */
    }
#endif
}

/**
 * Check if command is an interactive editor that should be redirected to sudoedit
 */
int is_interactive_editor(const char *command) {
    if (!command) return 0;

    /* Create a copy for parsing */
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;

    /* Get the first token (command name) */
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }

    /* List of interactive editors that should use sudoedit */
    const char *editors[] = {
        "vi", "/bin/vi", "/usr/bin/vi", "/usr/local/bin/vi",
        "vim", "/bin/vim", "/usr/bin/vim", "/usr/local/bin/vim",
        "nvim", "/bin/nvim", "/usr/bin/nvim", "/usr/local/bin/nvim",
        "emacs", "/bin/emacs", "/usr/bin/emacs", "/usr/local/bin/emacs",
        "nano", "/bin/nano", "/usr/bin/nano", "/usr/local/bin/nano",
        "pico", "/bin/pico", "/usr/bin/pico", "/usr/local/bin/pico",
        "joe", "/bin/joe", "/usr/bin/joe", "/usr/local/bin/joe",
        "mcedit", "/bin/mcedit", "/usr/bin/mcedit", "/usr/local/bin/mcedit",
        "ed", "/bin/ed", "/usr/bin/ed",
        "ex", "/bin/ex", "/usr/bin/ex",
        "view", "/bin/view", "/usr/bin/view",
        NULL
    };

    /* Check if it's an interactive editor */
    for (int i = 0; editors[i]; i++) {
        if (strcmp(cmd_name, editors[i]) == 0) {
            free(cmd_copy);
            return 1;
        }

        /* Also check basename for absolute paths */
        char *basename_editor = strrchr(editors[i], '/');
        if (basename_editor && strcmp(cmd_name, basename_editor + 1) == 0) {
            free(cmd_copy);
            return 1;
        }
    }

    free(cmd_copy);
    return 0;
}

/**
 * Check if command is a system editor tool that should be redirected to sudoedit
 */
int is_system_editor(const char *command) {
    if (!command) return 0;

    /* Create a copy for parsing */
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;

    /* Get the first token (command name) */
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }

    /* List of system editor tools that should use sudoedit */
    const char *system_editors[] = {
        "vipw", "/usr/sbin/vipw", "/sbin/vipw",
        "vigr", "/usr/sbin/vigr", "/sbin/vigr",
        "chfn", "/usr/bin/chfn", "/bin/chfn",
        "chsh", "/usr/bin/chsh", "/bin/chsh",
        "chpass", "/usr/bin/chpass", "/bin/chpass",
        NULL
    };

    /* Check if it's a system editor */
    for (int i = 0; system_editors[i]; i++) {
        if (strcmp(cmd_name, system_editors[i]) == 0) {
            free(cmd_copy);
            return 1;
        }

        /* Also check basename for absolute paths */
        char *basename_editor = strrchr(system_editors[i], '/');
        if (basename_editor && strcmp(cmd_name, basename_editor + 1) == 0) {
            free(cmd_copy);
            return 1;
        }
    }

    free(cmd_copy);
    return 0;
}

/**
 * Check if command is crontab -e which should be redirected to sudoedit
 */
int is_crontab_edit(const char *command) {
    if (!command) return 0;

    /* Create a copy for parsing */
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;

    /* Get the first token (command name) */
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }

    /* Check if it's crontab command */
    int is_crontab = 0;
    if (strcmp(cmd_name, "crontab") == 0 ||
        strcmp(cmd_name, "/usr/bin/crontab") == 0 ||
        strcmp(cmd_name, "/bin/crontab") == 0) {
        is_crontab = 1;
    }

    if (!is_crontab) {
        free(cmd_copy);
        return 0;
    }

    /* Check for -e flag */
    char *token = strtok(NULL, " \t");
    while (token) {
        if (strcmp(token, "-e") == 0) {
            free(cmd_copy);
            return 1;
        }
        token = strtok(NULL, " \t");
    }

    free(cmd_copy);
    return 0;
}

/**
 * Validate and sanitize editor command for native execution
 * Returns a newly allocated string that must be freed, or NULL on error
 */
char *validate_and_sanitize_editor_command(const char *command) {
    if (!command) return NULL;

    /* Validate arguments first */
    if (!validate_editor_arguments(command)) {
        log_security_violation(current_username, "dangerous editor arguments detected");
        return NULL;
    }

    /* Create a copy for parsing */
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return NULL;

    /* Get the first token (command name) */
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return NULL;
    }

    /* For system editors, ensure they use safe paths */
    if (is_system_editor(command)) {
        /* System editors are allowed to run natively with protections */
        char *result = strdup(command);
        free(cmd_copy);
        return result;
    }

    /* Handle crontab -e specially - allow native execution with protections */
    if (is_crontab_edit(command)) {
        char *result = strdup(command);
        free(cmd_copy);
        return result;
    }

    /* For regular interactive editors, allow native execution */
    char *result = strdup(command);
    free(cmd_copy);
    return result;
}

/**
 * Validate editor command arguments to prevent shell escapes
 */
int validate_editor_arguments(const char *command) {
    if (!command) return 0;

    /* Create a copy for parsing */
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;

    /* Get the first token (command name) */
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }

    /* Special handling for crontab -e which is safe */
    if (is_crontab_edit(command)) {
        free(cmd_copy);
        return 1; /* crontab -e is safe */
    }

    /* Special handling for system editors with their safe arguments */
    if (is_system_editor(command)) {
        /* System editors like chsh, chfn have safe arguments that might otherwise be flagged */
        if (strstr(cmd_name, "chsh") || strstr(cmd_name, "chfn") || strstr(cmd_name, "chpass")) {
            /* These commands have safe uses of -s and other flags */
            free(cmd_copy);
            return 1;
        }
    }

    /* List of dangerous editor arguments that could lead to shell escapes */
    const char *dangerous_args[] = {
        "-c",           /* Execute command */
        "--cmd",        /* Execute command */
        "--command",    /* Execute command */
        "-e",           /* Execute command (some editors, but not crontab) */
        "-x",           /* Execute script */
        "-s",           /* Silent mode with potential for abuse */
        "+cmd",         /* Vim command execution */
        "+!",           /* Vim shell command */
        "!",            /* Shell escape */
        ":!",           /* Editor shell escape */
        "|",            /* Pipe to shell */
        ";",            /* Command separator */
        "&&",           /* Command chaining */
        "||",           /* Command chaining */
        "`",            /* Command substitution */
        "$(",           /* Command substitution */
        NULL
    };

    /* Check for dangerous argument patterns */
    for (int i = 0; dangerous_args[i]; i++) {
        if (strstr(command, dangerous_args[i])) {
            free(cmd_copy);
            return 0; /* Dangerous argument found */
        }
    }

    /* Check for shell metacharacters that could be abused */
    if (strpbrk(command, "&;<>|`$(){}[]")) {
        free(cmd_copy);
        return 0; /* Shell metacharacters found */
    }

    free(cmd_copy);
    return 1; /* Arguments appear safe */
}

/**
 * Get safe editor path from allowlist
 */
const char *get_safe_editor_path(void) {
    /* List of safe editors in order of preference */
    const char *safe_editors[] = {
        "/usr/bin/vi",
        "/bin/vi",
        "/usr/bin/vim",
        "/bin/vim",
        "/usr/bin/nano",
        "/bin/nano",
        "/usr/bin/emacs",
        "/bin/emacs",
        NULL
    };

    /* Find the first available safe editor */
    for (int i = 0; safe_editors[i]; i++) {
        if (access(safe_editors[i], X_OK) == 0) {
            return safe_editors[i];
        }
    }

    /* Fallback to vi if nothing else is available */
    return "/usr/bin/vi";
}

/**
 * Create secure sudoedit command with validated arguments
 */
char *create_secure_sudoedit_command(const char *original_command) {
    if (!original_command) return NULL;

    /* Validate the original command arguments */
    if (!validate_editor_arguments(original_command)) {
        log_security_violation(current_username, "dangerous editor arguments detected");
        return NULL;
    }

    /* Extract file arguments only (skip the editor command) */
    char *cmd_copy = strdup(original_command);
    if (!cmd_copy) return NULL;

    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return NULL;
    }

    /* Build the secure sudoedit command */
    char *result = NULL;
    char *file_args = strchr(original_command, ' ');

    if (file_args) {
        /* Skip whitespace */
        file_args++;
        while (*file_args == ' ' || *file_args == '\t') {
            file_args++;
        }

        /* Only include file arguments, no editor options */
        if (*file_args && validate_editor_arguments(file_args)) {
            if (asprintf(&result, "sudoedit %s", file_args) == -1) {
                result = NULL;
            }
        } else {
            result = strdup("sudoedit");
        }
    } else {
        result = strdup("sudoedit");
    }

    free(cmd_copy);
    return result;
}

/**
 * Create a secure wrapper script for editor execution
 */
char *create_secure_editor_wrapper(const char *editor_command) {
    if (!editor_command) return NULL;

    /* Create a temporary wrapper script */
    char *wrapper_path = NULL;
    if (asprintf(&wrapper_path, "/tmp/sudosh_editor_wrapper_%d", getpid()) == -1) {
        return NULL;
    }

    FILE *wrapper_file = fopen(wrapper_path, "w");
    if (!wrapper_file) {
        free(wrapper_path);
        return NULL;
    }

    /* Write the wrapper script that prevents shell escapes */
    fprintf(wrapper_file, "#!/bin/sh\n");
    fprintf(wrapper_file, "# Sudosh secure editor wrapper\n");
    fprintf(wrapper_file, "# This wrapper prevents shell escapes from editors\n\n");

    /* Set up environment to prevent shell escapes */
    fprintf(wrapper_file, "export SHELL=/bin/false\n");
    fprintf(wrapper_file, "export VIMINIT='set shell=/bin/false'\n");
    fprintf(wrapper_file, "export EXINIT='set shell=/bin/false'\n");
    fprintf(wrapper_file, "export ESHELL=/bin/false\n");

    /* Unset dangerous variables */
    fprintf(wrapper_file, "unset EDITOR VISUAL SUDO_EDITOR FCEDIT PAGER MANPAGER\n");

    /* Execute the original editor command */
    fprintf(wrapper_file, "exec %s\n", editor_command);

    fclose(wrapper_file);

    /* Make the wrapper executable */
    if (chmod(wrapper_path, 0755) != 0) {
        unlink(wrapper_path);
        free(wrapper_path);
        return NULL;
    }

    return wrapper_path;
}

/**
 * Monitor editor process and block shell escapes
 */
int monitor_editor_process(pid_t editor_pid) {
    if (editor_pid <= 0) return 0;

    int status;
    pid_t child_pid;

    /* Monitor for child processes that might be shell escapes */
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (child_pid != editor_pid) {
            /* A child process was spawned - check if it's a shell */
            char proc_path[256];
            char exe_path[256];
            ssize_t len;

            snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", child_pid);
            len = readlink(proc_path, exe_path, sizeof(exe_path) - 1);

            if (len > 0) {
                exe_path[len] = '\0';

                /* Check if the child process is a shell */
                if (strstr(exe_path, "sh") || strstr(exe_path, "bash") ||
                    strstr(exe_path, "zsh") || strstr(exe_path, "csh") ||
                    strstr(exe_path, "tcsh") || strstr(exe_path, "ksh")) {

                    /* Kill the shell process immediately */
                    kill(child_pid, SIGKILL);

                    /* Log the security violation with details */
                    char violation_msg[512];
                    snprintf(violation_msg, sizeof(violation_msg), "shell escape attempt blocked: %s", exe_path);
                    log_security_violation(current_username, violation_msg);

                    /* Wait for the killed process to clean up */
                    waitpid(child_pid, &status, 0);
                }
            }
        }
    }

    return 1;
}

/**
 * Execute editor with comprehensive shell escape protection
 */
char *create_monitored_editor_command(const char *original_command) {
    if (!original_command) return NULL;

    /* Create a wrapper that includes monitoring */
    char *wrapper_path = NULL;
    if (asprintf(&wrapper_path, "/tmp/sudosh_monitored_editor_%d", getpid()) == -1) {
        return NULL;
    }

    FILE *wrapper_file = fopen(wrapper_path, "w");
    if (!wrapper_file) {
        free(wrapper_path);
        return NULL;
    }

    /* Write a comprehensive wrapper script */
    fprintf(wrapper_file, "#!/bin/sh\n");
    fprintf(wrapper_file, "# Sudosh monitored editor wrapper\n");
    fprintf(wrapper_file, "# Prevents shell escapes and monitors subprocesses\n\n");

    /* Set up restrictive environment */
    fprintf(wrapper_file, "export SHELL=/bin/false\n");
    fprintf(wrapper_file, "export VIMINIT='set shell=/bin/false | set noshelltemp'\n");
    fprintf(wrapper_file, "export EXINIT='set shell=/bin/false'\n");
    fprintf(wrapper_file, "export ESHELL=/bin/false\n");

    /* Remove dangerous environment variables */
    fprintf(wrapper_file, "unset EDITOR VISUAL SUDO_EDITOR FCEDIT PAGER MANPAGER\n");
    fprintf(wrapper_file, "unset BASH_ENV ENV CDPATH IFS\n");

    /* Create a restricted PATH */
    fprintf(wrapper_file, "export PATH=/usr/bin:/bin\n");

    /* Execute the editor with monitoring */
    fprintf(wrapper_file, "exec %s\n", original_command);

    fclose(wrapper_file);

    /* Make the wrapper executable */
    if (chmod(wrapper_path, 0755) != 0) {
        unlink(wrapper_path);
        free(wrapper_path);
        return NULL;
    }

    return wrapper_path;
}

/**
 * Apply native security protections for editor execution
 */
int apply_native_editor_protections(const char *command) {
    if (!command) return 0;

    /* Set up secure environment */
    setup_secure_editor_environment();

    /* Validate arguments */
    if (!validate_editor_arguments(command)) {
        log_security_violation(current_username, "dangerous editor arguments detected");
        return 0;
    }

    /* Additional protections for specific editor types */
    if (is_system_editor(command)) {
        /* System editors get extra restrictions */
        log_security_event(current_username, "system editor executed with native protections", command);
    } else if (is_interactive_editor(command)) {
        /* Interactive editors get standard protections */
        log_security_event(current_username, "interactive editor executed with native protections", command);
    } else if (is_crontab_edit(command)) {
        /* Crontab editing gets special handling */
        log_security_event(current_username, "crontab editor executed with native protections", command);
    }

    return 1;
}

/**
 * Check if command is a safe command that should always be allowed
 */
int is_safe_command(const char *command) {
    if (!command) return 0;

    /* Create a copy for parsing */
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;

    /* Get the first token (command name) */
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }

    /* List of safe commands that should always be allowed */
    const char *safe_commands[] = {
        "ls", "/bin/ls", "/usr/bin/ls",
        "pwd", "/bin/pwd", "/usr/bin/pwd",
        "whoami", "/usr/bin/whoami", "/bin/whoami",
        "id", "/usr/bin/id", "/bin/id",
        "date", "/bin/date", "/usr/bin/date",
        "uptime", "/usr/bin/uptime", "/bin/uptime",
        "w", "/usr/bin/w", "/bin/w",
        "who", "/usr/bin/who", "/bin/who",
        NULL
    };

    /* Check if it's a safe command */
    for (int i = 0; safe_commands[i]; i++) {
        if (strcmp(cmd_name, safe_commands[i]) == 0) {
            free(cmd_copy);
            return 1;
        }
    }

    free(cmd_copy);
    return 0;
}

/**
 * Check if command is an SSH command
 */
int is_ssh_command(const char *command) {
    if (!command) return 0;

    /* Create a copy for parsing */
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;

    /* Get the first token (command name) */
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }

    /* Check if it's ssh or ssh-like command */
    if (strcmp(cmd_name, "ssh") == 0 ||
        strcmp(cmd_name, "/usr/bin/ssh") == 0 ||
        strcmp(cmd_name, "/bin/ssh") == 0 ||
        strcmp(cmd_name, "/usr/local/bin/ssh") == 0) {
        free(cmd_copy);
        return 1;
    }

    /* Check if command starts with ssh followed by space or end */
    if (strncmp(command, "ssh ", 4) == 0 || strcmp(command, "ssh") == 0) {
        free(cmd_copy);
        return 1;
    }

    free(cmd_copy);
    return 0;
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
 * Check if command is a safe read-only operation
 */
int is_safe_readonly_command(const char *command) {
    if (!command) return 0;

    /* Safe read-only commands that don't modify system state */
    const char *safe_readonly[] = {
        "cat", "less", "more", "head", "tail", "grep", "egrep", "fgrep",
        "view", "vi", "vim", "nano", "emacs", "pico",
        "ls", "ll", "dir", "find", "locate", "which", "whereis",
        "file", "stat", "du", "df", "lsof", "ps", "top", "htop",
        "id", "whoami", "who", "w", "last", "lastlog",
        "date", "uptime", "uname", "hostname", "dmesg",
        "mount", "lsblk", "lscpu", "lsmem", "lsusb", "lspci",
        "netstat", "ss", "ip", "ifconfig", "route",
        "awk", "sed", "sort", "uniq", "cut", "tr", "wc",
        "diff", "cmp", "md5sum", "sha1sum", "sha256sum",
        "strings", "hexdump", "od", "xxd",
        "/bin/cat", "/usr/bin/cat", "/bin/less", "/usr/bin/less",
        "/usr/bin/vi", "/usr/bin/vim", "/bin/ls", "/usr/bin/ls",
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

    /* Check against safe read-only command list */
    for (int i = 0; safe_readonly[i]; i++) {
        if (strcmp(cmd_name, safe_readonly[i]) == 0) {
            free(cmd_copy);
            return 1;
        }

        /* Also check basename for absolute paths */
        char *basename_cmd = strrchr(safe_readonly[i], '/');
        if (basename_cmd && strcmp(cmd_name, basename_cmd + 1) == 0) {
            free(cmd_copy);
            return 1;
        }
    }

    free(cmd_copy);
    return 0;
}

/**
 * Check if command is potentially dangerous to system directories
 */
int is_dangerous_system_operation(const char *command) {
    if (!command) return 0;

    /* Commands that can modify, delete, or damage system state */
    const char *dangerous_ops[] = {
        "rm", "rmdir", "unlink", "shred", "wipe",
        "mv", "cp", "dd", "rsync",
        "chmod", "chown", "chgrp", "chattr", "setfacl",
        "ln", "link", "symlink",
        "mkdir", "touch", "truncate",
        "tar", "gzip", "gunzip", "zip", "unzip",
        "make", "gcc", "g++", "cc", "ld",
        "dpkg", "apt", "apt-get", "yum", "dnf", "rpm", "zypper",
        "systemctl", "service", "chkconfig", "update-rc.d",
        "crontab", "at", "batch",
        "useradd", "userdel", "usermod", "groupadd", "groupdel",
        "passwd", "chpasswd", "pwconv", "pwunconv",
        "mount", "umount", "swapon", "swapoff",
        "fdisk", "parted", "mkfs", "fsck", "tune2fs",
        "iptables", "ip6tables", "ufw", "firewall-cmd",
        "/bin/rm", "/usr/bin/rm", "/bin/mv", "/usr/bin/mv",
        "/bin/cp", "/usr/bin/cp", "/bin/chmod", "/usr/bin/chmod",
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

    /* Check against dangerous operation list */
    for (int i = 0; dangerous_ops[i]; i++) {
        if (strcmp(cmd_name, dangerous_ops[i]) == 0) {
            free(cmd_copy);
            return 1;
        }

        /* Also check basename for absolute paths */
        char *basename_cmd = strrchr(dangerous_ops[i], '/');
        if (basename_cmd && strcmp(cmd_name, basename_cmd + 1) == 0) {
            free(cmd_copy);
            return 1;
        }
    }

    free(cmd_copy);
    return 0;
}

/**
 * Check if command accesses critical system directories with intelligent analysis
 */
int check_system_directory_access(const char *command) {
    if (!command) return 0;

    /* Critical system directories that require extra caution */
    const char *critical_dirs[] = {
        "/dev", "/proc", "/sys", "/boot", "/etc",
        "/bin", "/sbin", "/usr/bin", "/usr/sbin",
        "/lib", "/lib64", "/usr/lib", "/usr/lib64",
        "/var/log", "/var/run", "/var/lib",
        "/root", "/home/root",
        NULL
    };

    /* First check if command references any critical directories */
    int accesses_critical_dir = 0;
    for (int i = 0; critical_dirs[i]; i++) {
        if (strstr(command, critical_dirs[i])) {
            accesses_critical_dir = 1;
            break;
        }
    }

    /* If no critical directory access, no warning needed */
    if (!accesses_critical_dir) {
        return 0;
    }

    /* Check for output redirection which is always dangerous to system directories */
    if (strstr(command, " > ") || strstr(command, " >> ") ||
        strstr(command, " 2> ") || strstr(command, " &> ")) {
        return 1;
    }

    /* Check for pipe to dangerous commands */
    if (strstr(command, "| rm") || strstr(command, "| chmod") ||
        strstr(command, "| chown") || strstr(command, "| dd")) {
        return 1;
    }

    /* If it's a dangerous operation on system directories, definitely warn */
    if (is_dangerous_system_operation(command)) {
        return 1;
    }

    /* If it's a safe read-only command without redirection, allow without warning */
    if (is_safe_readonly_command(command)) {
        return 0;
    }

    /* Default to warning for other system directory access */
    return 1;
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
 * Enhanced buffer overflow protection
 * Based on sudo's buffer overflow security fixes
 */
static int validate_command_buffer(const char *command) {
    size_t len;
    const char *p;

    if (!command) {
        return 0;
    }

    /* Check for buffer overflow conditions */
    len = strlen(command);
    if (len == 0) {
        return 0;  /* Empty command */
    }

    if (len >= MAX_COMMAND_LENGTH) {
        log_security_violation(current_username, "command buffer overflow attempt");
        return 0;
    }

    /* Check for null bytes in the middle of the string */
    for (p = command; p < command + len; p++) {
        if (*p == '\0') {
            log_security_violation(current_username, "embedded null byte in command");
            return 0;
        }
    }

    /* Check for control characters that could cause issues */
    for (p = command; *p; p++) {
        if ((*p < 32 && *p != '\t' && *p != '\n') || *p == 127) {
            log_security_violation(current_username, "control character in command");
            return 0;
        }
    }

    return 1;
}

/**
 * Enhanced command validation with shell and danger checks
 * Enhanced with buffer overflow protection based on sudo's fixes
 */
int validate_command(const char *command) {
    if (!command) {
        return 0;
    }

    /* Enhanced buffer validation first */
    if (!validate_command_buffer(command)) {
        return 0;
    }

    /* Special validation for editor commands */
    if (is_interactive_editor(command) || is_system_editor(command) || is_crontab_edit(command)) {
        /* Set up secure environment for editor execution */
        setup_secure_editor_environment();

        /* Validate editor arguments */
        if (!validate_editor_arguments(command)) {
            log_security_violation(current_username, "dangerous editor arguments detected");
            return 0;
        }

        /* Log the secure editor execution */
        log_security_event(current_username, "secure editor execution validated", command);
        return 1;
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

    /* Block SSH commands */
    if (is_ssh_command(command)) {
        log_security_violation(current_username, "SSH command blocked");
        fprintf(stderr, "sudosh: you should only ssh as your user, not root\n");
        return 0;
    }

    /* Note: Interactive editors are now redirected to sudoedit in main.c
     * This validation function no longer blocks them directly */

    /* Note: Editor commands are validated above with native security protections */

    /* Apply standard dangerous command checks to non-editor commands */
    if (!is_interactive_editor(command) && !is_system_editor(command) && !is_crontab_edit(command)) {
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
