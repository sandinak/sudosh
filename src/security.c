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
    /* Preserve color-related environment variables before sanitization */
    preserve_color_environment();

    /* List of dangerous environment variables to remove */
    const char *dangerous_vars[] = {
        "IFS",
        "CDPATH",
        "ENV",
        "BASH_ENV",
        "GLOBIGNORE",
        "PS4",
        "SHELLOPTS",
        "HISTFILE",
        "HISTCMD",
        "HISTCONTROL",
        "HISTIGNORE",
        "HISTSIZE",
        "HISTTIMEFORMAT",
        "LD_PRELOAD",
        "LD_LIBRARY_PATH",
        "SHLIB_PATH",
        "LIBPATH",
        "DYLD_LIBRARY_PATH",
        "DYLD_INSERT_LIBRARIES",
        "DYLD_FORCE_FLAT_NAMESPACE",
        "DYLD_FALLBACK_LIBRARY_PATH",
        "TMPDIR",
        "TMP",
        "TEMP",
        "EDITOR",
        "VISUAL",
        "PAGER",
        "BROWSER",
        NULL
    };

    int i;

    /* Remove dangerous variables */
    for (i = 0; dangerous_vars[i]; i++) {
        unsetenv(dangerous_vars[i]);
    }

    /* Set secure PATH - always override for security */
    const char *secure_path = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    setenv("PATH", secure_path, 1);

    /* Verify PATH doesn't contain dangerous elements */
    char *path = getenv("PATH");
    if (path && (strstr(path, ".:") || strstr(path, ":.") ||
                 strstr(path, "::") || path[0] == ':' ||
                 path[strlen(path)-1] == ':')) {
        /* PATH contains dangerous elements, reset to secure default */
        setenv("PATH", secure_path, 1);
        log_security_violation(current_username, "dangerous PATH detected and sanitized");
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
 * Secure the terminal and file descriptors
 */
void secure_terminal(void) {
    /* Disable core dumps for security */
    struct rlimit rlim;
    rlim.rlim_cur = 0;
    rlim.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rlim);

    /* Close all file descriptors except stdin, stdout, stderr */
    int max_fd = sysconf(_SC_OPEN_MAX);
    if (max_fd == -1) {
        max_fd = 1024; /* fallback value */
    }

    for (int fd = 3; fd < max_fd; fd++) {
        close(fd);
    }

    /* Ensure stdin, stdout, stderr are properly connected */
    if (fcntl(STDIN_FILENO, F_GETFD) == -1) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd != STDIN_FILENO) {
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
    }

    if (fcntl(STDOUT_FILENO, F_GETFD) == -1) {
        int fd = open("/dev/null", O_WRONLY);
        if (fd != STDOUT_FILENO) {
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
    }

    if (fcntl(STDERR_FILENO, F_GETFD) == -1) {
        int fd = open("/dev/null", O_WRONLY);
        if (fd != STDERR_FILENO) {
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
    }

    /* Set process group */
    if (setsid() == -1) {
        /* Not a problem if we're already a session leader */
    }
}

/**
 * Check if command is an interactive editor that could allow shell escape
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

    /* List of interactive editors that can execute shell commands */
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
 * Check if user has unrestricted sudo access (ALL commands)
 */
int user_has_unrestricted_access(const char *username) {
    if (!username) return 0;

    struct sudoers_config *sudoers_config = parse_sudoers_file(NULL);
    if (!sudoers_config) return 0;

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }

    struct sudoers_userspec *spec = sudoers_config->userspecs;
    while (spec) {
        /* Check if this rule applies to the user */
        if (spec->users) {
            for (int i = 0; spec->users && spec->users[i]; i++) {
                int user_matches = 0;

                /* Direct username match */
                if (strcmp(spec->users[i], username) == 0) {
                    user_matches = 1;
                }
                /* Group membership check */
                else if (spec->users[i][0] == '%') {
                    struct group *grp = getgrnam(spec->users[i] + 1);
                    if (grp && grp->gr_mem) {
                        for (char **member = grp->gr_mem; member && *member; member++) {
                            if (*member && strcmp(*member, username) == 0) {
                                user_matches = 1;
                                break;
                            }
                        }
                    }
                }

                if (user_matches) {
                    /* Check if hostname matches */
                    if (spec->hosts) {
                        for (int j = 0; spec->hosts[j]; j++) {
                            if (strcmp(spec->hosts[j], "ALL") == 0 ||
                                strcmp(spec->hosts[j], hostname) == 0) {
                                /* Check if commands include ALL */
                                if (spec->commands) {
                                    for (int k = 0; spec->commands[k]; k++) {
                                        if (strcmp(spec->commands[k], "ALL") == 0) {
                                            free_sudoers_config(sudoers_config);
                                            return 1; /* User has unrestricted access */
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        spec = spec->next;
    }

    free_sudoers_config(sudoers_config);
    return 0;
}

/**
 * Check if command is a potentially destructive archive operation
 */
int is_destructive_archive_operation(const char *command) {
    if (!command) return 0;

    /* Archive extraction commands that could overwrite files */
    const char *archive_commands[] = {
        "tar", "untar", "gtar",
        "unzip", "gunzip", "bunzip2", "unxz",
        "dump", "restore", "cpio",
        "7z", "7za", "7zr",
        "rar", "unrar",
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

    /* Check if it's an archive command */
    int is_archive_cmd = 0;
    for (int i = 0; archive_commands[i]; i++) {
        if (strcmp(cmd_name, archive_commands[i]) == 0) {
            is_archive_cmd = 1;
            break;
        }
        /* Also check basename for absolute paths */
        char *basename_cmd = strrchr(archive_commands[i], '/');
        if (basename_cmd && strcmp(cmd_name, basename_cmd + 1) == 0) {
            is_archive_cmd = 1;
            break;
        }
    }

    if (!is_archive_cmd) {
        free(cmd_copy);
        return 0;
    }

    /* Check for extraction flags and potentially dangerous patterns */
    if (strstr(command, " -x") ||           /* tar extract */
        strstr(command, " --extract") ||
        strstr(command, " -d ") ||          /* unzip to directory */
        strstr(command, " --overwrite") ||  /* force overwrite */
        strstr(command, " --force") ||
        strstr(command, " -f ") ||          /* force flags */
        strstr(command, " -o ") ||          /* overwrite flags */
        strstr(command, " -y ") ||          /* yes to all */
        (strcmp(cmd_name, "unzip") == 0) || /* unzip is extraction by default */
        (strcmp(cmd_name, "gunzip") == 0) ||
        (strcmp(cmd_name, "bunzip2") == 0) ||
        (strcmp(cmd_name, "unxz") == 0)) {

        /* Check if extracting to existing directories or system paths */
        if (strstr(command, " /") ||        /* absolute paths */
            strstr(command, " ./") ||       /* current directory */
            strstr(command, " ../") ||      /* parent directory */
            strstr(command, " ~") ||        /* home directory */
            strstr(command, "/etc") ||      /* system directories */
            strstr(command, "/usr") ||
            strstr(command, "/var") ||
            strstr(command, "/opt")) {
            free(cmd_copy);
            return 1;
        }

        /* Default warning for extraction operations */
        free(cmd_copy);
        return 1;
    }

    free(cmd_copy);
    return 0;
}

/**
 * Prompt user for confirmation of dangerous command
 */
int prompt_user_confirmation(const char *command, const char *warning) {
    char response[10];
    (void)command; /* Suppress unused parameter warning */

    printf("\n⚠️  %s\n", warning);
    printf("Continue? (y/N): ");
    fflush(stdout);

    if (fgets(response, sizeof(response), stdin) == NULL) {
        return 0; /* Default to no on error */
    }

    /* Trim newline */
    response[strcspn(response, "\n")] = '\0';

    /* Accept 'y', 'Y', 'yes', 'YES' */
    if (strcmp(response, "y") == 0 || strcmp(response, "Y") == 0 ||
        strcmp(response, "yes") == 0 || strcmp(response, "YES") == 0) {
        return 1;
    }

    printf("Cancelled.\n");
    return 0;
}

/**
 * Enhanced command validation with comprehensive injection protection
 */
int validate_command(const char *command) {
    if (!command) {
        return 0;
    }

    /* Basic security checks first */

    /* Check for null bytes (potential injection) */
    size_t cmd_len = strlen(command);
    for (size_t i = 0; i < cmd_len; i++) {
        if (command[i] == '\0') {
            log_security_violation(current_username, "null byte in command");
            return 0;
        }
    }

    /* Check for extremely long commands */
    if (cmd_len > MAX_COMMAND_LENGTH) {
        log_security_violation(current_username, "command too long");
        return 0;
    }

    /* Enhanced path traversal detection */
    if (strstr(command, "../") || strstr(command, "..\\") ||
        strstr(command, "/.../") || strstr(command, "\\.../")) {
        log_security_violation(current_username, "path traversal attempt");
        return 0;
    }

    /* Check for shell metacharacters that enable command injection */
    const char *dangerous_chars = ";|&`$(){}[]<>*?~";
    for (const char *p = dangerous_chars; *p; p++) {
        if (strchr(command, *p)) {
            char violation_msg[256];
            snprintf(violation_msg, sizeof(violation_msg),
                    "shell metacharacter '%c' detected in command", *p);
            log_security_violation(current_username, violation_msg);
            return 0;
        }
    }

    /* Check for command substitution patterns */
    if (strstr(command, "$(") || strstr(command, "`") ||
        strstr(command, "${") || strstr(command, "$[")) {
        log_security_violation(current_username, "command substitution attempt");
        return 0;
    }

    /* Check for I/O redirection that could be used maliciously */
    if (strstr(command, " > ") || strstr(command, " >> ") ||
        strstr(command, " < ") || strstr(command, " 2> ") ||
        strstr(command, " 2>> ") || strstr(command, " &> ") ||
        strstr(command, " &>> ")) {
        log_security_violation(current_username, "I/O redirection attempt");
        return 0;
    }

    /* Check for environment variable injection */
    if (strstr(command, "$HOME") || strstr(command, "$PATH") ||
        strstr(command, "$USER") || strstr(command, "$SHELL") ||
        strstr(command, "$PWD") || strstr(command, "$OLDPWD")) {
        log_security_violation(current_username, "environment variable injection attempt");
        return 0;
    }

    /* Check for format string vulnerabilities */
    if (strstr(command, "%s") || strstr(command, "%d") ||
        strstr(command, "%x") || strstr(command, "%n") ||
        strstr(command, "%p")) {
        log_security_violation(current_username, "format string injection attempt");
        return 0;
    }

    /* Check for Unicode and encoding attacks */
    for (size_t i = 0; i < cmd_len; i++) {
        unsigned char c = (unsigned char)command[i];
        /* Block all non-printable characters except space */
        if (c < 32 && c != ' ') {
            char violation_msg[256];
            snprintf(violation_msg, sizeof(violation_msg),
                    "non-printable character 0x%02x in command", c);
            log_security_violation(current_username, violation_msg);
            return 0;
        }
        /* Block high-bit characters that could be encoding attacks */
        if (c > 126) {
            char violation_msg[256];
            snprintf(violation_msg, sizeof(violation_msg),
                    "high-bit character 0x%02x encoding attack", c);
            log_security_violation(current_username, violation_msg);
            return 0;
        }
    }

    /* Check for URL encoding attacks (case insensitive) */
    if (strstr(command, "%00") || strstr(command, "%0a") || strstr(command, "%0A") ||
        strstr(command, "%0d") || strstr(command, "%0D") || strstr(command, "%09") ||
        strstr(command, "%2e%2e") || strstr(command, "%2E%2E") ||
        strstr(command, "%2f") || strstr(command, "%2F")) {
        log_security_violation(current_username, "URL encoding attack detected");
        return 0;
    }

    /* Check for Unicode escape sequences */
    if (strstr(command, "\\u0000") || strstr(command, "\\x00") ||
        strstr(command, "\\n") || strstr(command, "\\r") ||
        strstr(command, "\\t")) {
        log_security_violation(current_username, "Unicode escape sequence attack");
        return 0;
    }

    /* Check for additional dangerous patterns */
    if (strstr(command, "export ") || strstr(command, "unset ") ||
        strstr(command, "alias ") || strstr(command, "function ")) {
        log_security_violation(current_username, "shell environment manipulation attempt");
        return 0;
    }

    /* Check for environment disclosure commands */
    if (strcmp(command, "env") == 0 || strcmp(command, "printenv") == 0 ||
        strncmp(command, "env ", 4) == 0 || strncmp(command, "printenv ", 9) == 0) {
        log_security_violation(current_username, "environment disclosure command blocked");
        return 0;
    }

    /* Check for variable assignment patterns */
    if (strstr(command, "HOME=") || strstr(command, "PATH=") ||
        strstr(command, "LD_PRELOAD=") || strstr(command, "SHELL=")) {
        log_security_violation(current_username, "environment variable assignment attempt");
        return 0;
    }

    /* Check for quotes that could be used for injection */
    if (strchr(command, '\'') || strchr(command, '"') || strchr(command, '\\')) {
        log_security_violation(current_username, "quote character injection attempt");
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

    /* Block interactive editors that can execute shell commands */
    if (is_interactive_editor(command)) {
        log_security_violation(current_username, "interactive editor blocked");
        fprintf(stderr, "sudosh: interactive editors are not permitted (use sudoedit instead)\n");
        fprintf(stderr, "sudosh: editors like vi/vim/emacs can execute shell commands and bypass security\n");
        return 0;
    }

    /* Check if user has unrestricted access - if so, skip warnings but still log */
    int has_unrestricted_access = user_has_unrestricted_access(current_username);

    /* Check for dangerous commands */
    if (is_dangerous_command(command)) {
        char violation_msg[256];
        snprintf(violation_msg, sizeof(violation_msg), "dangerous command attempted: %s", command);
        log_security_violation(current_username, violation_msg);

        if (!has_unrestricted_access && !prompt_user_confirmation(command, "Potentially dangerous system command")) {
            return 0;
        }
    }

    /* Check for dangerous flags */
    if (check_dangerous_flags(command)) {
        char violation_msg[256];
        snprintf(violation_msg, sizeof(violation_msg), "dangerous flags detected: %s", command);
        log_security_violation(current_username, violation_msg);

        if (!has_unrestricted_access && !prompt_user_confirmation(command, "Command uses recursive/force flags")) {
            return 0;
        }
    }

    /* Check for destructive archive operations */
    if (is_destructive_archive_operation(command)) {
        char violation_msg[256];
        snprintf(violation_msg, sizeof(violation_msg), "destructive archive operation: %s", command);
        log_security_violation(current_username, violation_msg);

        if (!has_unrestricted_access && !prompt_user_confirmation(command, "Archive extraction may overwrite files")) {
            return 0;
        }
    }

    /* Check for system directory access */
    if (check_system_directory_access(command)) {
        char violation_msg[256];
        snprintf(violation_msg, sizeof(violation_msg), "system directory access: %s", command);
        log_security_violation(current_username, violation_msg);

        if (!has_unrestricted_access && !prompt_user_confirmation(command, "Accesses system directories")) {
            return 0;
        }
    }

    return 1;
}

/**
 * Enhanced command validation with buffer length for testing null byte injection
 */
int validate_command_with_length(const char *command, size_t buffer_len) {
    if (!command) {
        return 0;
    }

    /* Check for embedded null bytes by comparing strlen with buffer_len */
    size_t cmd_len = strlen(command);
    if (buffer_len > cmd_len + 1) {
        /* Buffer is longer than string length + null terminator,
           suggesting embedded null bytes */
        log_security_violation(current_username, "embedded null byte detected in command");
        return 0;
    }

    /* Use the regular validation for other checks */
    return validate_command(command);
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
