/**
 * command.c - Command Parsing and Execution
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Handles command parsing, validation, and execution with proper
 * privilege escalation and target user support.
 */

#include "sudosh.h"

/**
 * Expand = expressions in command arguments (like zsh)
 */
char *expand_equals_expression(const char *arg) {
    if (!arg || arg[0] != '=') {
        return safe_strdup(arg);  /* Return copy of original if not = expression */
    }

    /* Skip the = character */
    const char *command_name = arg + 1;
    if (strlen(command_name) == 0) {
        return safe_strdup(arg);  /* Return original if just = */
    }

    /* Get PATH environment variable */
    char *path_env = getenv("PATH");
    if (!path_env) {
        return safe_strdup(arg);  /* Return original if no PATH */
    }

    /* Make a copy of PATH for tokenization */
    char *path_copy = safe_strdup(path_env);
    if (!path_copy) {
        return safe_strdup(arg);
    }

    char *dir, *saveptr;
    char *result = NULL;

    /* Search each directory in PATH */
    dir = strtok_r(path_copy, ":", &saveptr);
    while (dir != NULL) {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, command_name);

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
            /* Found executable - return full path */
            result = safe_strdup(full_path);
            break;
        }

        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);

    /* Return the expanded path or original if not found */
    return result ? result : safe_strdup(arg);
}

/**
 * Parse command line input into command structure with shell syntax awareness
 */
int parse_command(const char *input, struct command_info *cmd) {
    char *input_copy;
    int argc = 0;
    int argv_size = 16;

    if (!input || !cmd) {
        return -1;
    }

    /* Initialize command structure */
    memset(cmd, 0, sizeof(struct command_info));

    /* Make a copy of input for parsing */
    input_copy = safe_strdup(input);
    if (!input_copy) {
        return -1;
    }

    /* First, check for shell operators and handle them appropriately */
    if (contains_shell_operators(input_copy)) {
        /* This command contains shell operators that need special handling */
        int result = parse_command_with_shell_operators(input_copy, cmd);
        free(input_copy);
        return result;
    }

    /* Allocate initial argv array */
    cmd->argv = malloc(argv_size * sizeof(char *));
    if (!cmd->argv) {
        free(input_copy);
        return -1;
    }

    /* Parse the command using shell-aware tokenization */
    if (tokenize_command_line(input_copy, &cmd->argv, &argc, &argv_size) != 0) {
        free_command_info(cmd);
        free(input_copy);
        return -1;
    }

    /* Null-terminate argv */
    cmd->argv[argc] = NULL;
    cmd->argc = argc;

    /* Store the full command */
    cmd->command = safe_strdup(input);
    if (!cmd->command) {
        free_command_info(cmd);
        free(input_copy);
        return -1;
    }

    free(input_copy);
    return 0;
}

/**
 * Check if a command would be allowed via sudo for the given user
 */
int check_sudo_command_allowed(const char *username, const char *command) {
    if (!username || !command) {
        return 0;
    }

    /* For now, use a simplified check based on sudoers integration */
    /* In a full implementation, this would integrate with sudoers parsing */

    /* Check if user has general sudo access */
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        snprintf(hostname, sizeof(hostname), "%s", "localhost");
    }

    if (check_sudoers_nopasswd(username, hostname, NULL)) {
        return 1;  /* User has sudo access */
    }

    /* For now, allow common safe commands */
    const char *safe_commands[] = {
        "whoami", "id", "pwd", "ls", "cat", "echo", "date", "uptime",
        "systemctl", "service", "mount", "umount", "df", "du",
        NULL
    };

    for (int i = 0; safe_commands[i]; i++) {
        if (strcmp(command, safe_commands[i]) == 0) {
            return 1;
        }
    }

    /* Default to allowing for now - in production this should be more restrictive */
    return 1;
}

/**
 * Validate command against sudoers rules for Ansible sessions
 */
int validate_ansible_command(const char *command, const char *username) {
    if (!command || !username) {
        return 0;
    }

    /* For Ansible sessions, ensure command compliance with sudoers rules */
    extern struct ansible_detection_info *global_ansible_info;
    if (global_ansible_info && global_ansible_info->is_ansible_session) {
        /* Extract the actual command from potential shell wrappers */
        char *cmd_copy = safe_strdup(command);
        if (!cmd_copy) {
            return 0;
        }

        /* Parse the command to get the executable */
        char *token = strtok(cmd_copy, " \t");
        if (!token) {
            free(cmd_copy);
            return 0;
        }

        /* Check if this command would be allowed via sudo */
        int allowed = check_sudo_command_allowed(username, token);

        /* Log validation attempt */
        if (allowed) {
            syslog(LOG_INFO, "ANSIBLE_SESSION: Command validation passed for %s: %s",
                   username, token);
        } else {
            syslog(LOG_WARNING, "ANSIBLE_SESSION: Command validation failed for %s: %s",
                   username, token);
        }

        free(cmd_copy);
        return allowed;
    }

    /* For non-Ansible sessions, use standard validation */
    return 1;
}

/**
 * Execute command with elevated privileges
 */
int execute_command(struct command_info *cmd, struct user_info *user) {
    (void)user;  /* Suppress unused parameter warning */
    pid_t pid;
    int status;
    char *command_path;
    struct passwd *target_pwd = NULL;

    if (!cmd || !cmd->argv || !cmd->argv[0]) {
        return -1;
    }

    /* Validate command for Ansible sessions */
    if (!validate_ansible_command(cmd->argv[0], getenv("USER"))) {
        fprintf(stderr, "Error: Command not authorized for Ansible session\n");
        log_security_violation(getenv("USER"), "ansible command validation failed");
        return -1;
    }

    /* Get target user info if target user is specified */
    if (target_user) {
        target_pwd = getpwnam(target_user);
        if (!target_pwd) {
            fprintf(stderr, "sudosh: target user '%s' not found\n", target_user);
            return -1;
        }
    }

    /* Find the command in PATH if it's not an absolute path */
    if (cmd->argv[0][0] != '/') {
        command_path = find_command_in_path(cmd->argv[0]);
        if (!command_path) {
            fprintf(stderr, "sudosh: %s: command not found\n", cmd->argv[0]);
            return EXIT_COMMAND_NOT_FOUND;
        }
    } else {
        command_path = safe_strdup(cmd->argv[0]);
        if (!command_path) {
            return -1;
        }
    }

    /* Check if command exists and is executable */
    if (access(command_path, X_OK) != 0) {
        fprintf(stderr, "sudosh: %s: permission denied or not found\n", command_path);
        free(command_path);
        return EXIT_COMMAND_NOT_FOUND;
    }

    /* Handle file locking for editing commands before forking */
    char *file_to_edit = NULL;
    int file_lock_acquired = 0;
    if (is_editing_command(cmd->command)) {
        /* Check if file locking system is available */
        if (!is_file_locking_available()) {
            fprintf(stderr, "sudosh: warning: file locking unavailable for editing command\n");
            fprintf(stderr, "sudosh: cannot ensure exclusive file access\n");
            free(command_path);
            return -1;
        }

        file_to_edit = extract_file_argument(cmd->command);
        if (file_to_edit) {
            if (acquire_file_lock(file_to_edit, user->username, getpid()) == 0) {
                file_lock_acquired = 1;
            } else {
                /* Lock acquisition failed - for secure editors, continue anyway */
                /* This ensures secure editors work even if file locking has issues */
                if (is_secure_editor(cmd->command)) {
                    char log_msg[512];
                    snprintf(log_msg, sizeof(log_msg),
                            "file lock failed for secure editor, proceeding anyway: %s", cmd->command);
                    log_security_violation(user->username, log_msg);
                    /* Continue execution for secure editors */
                } else {
                    /* For non-secure editors, block execution on lock failure */
                    free(file_to_edit);
                    free(command_path);
                    return -1;
                }
            }
        }
    }

    /* Fork and execute */
    pid = fork();
    if (pid == -1) {
        perror("fork");
        free(command_path);
        return -1;
    }

    if (pid == 0) {
        /* Child process */

        /* Reset signal handlers to default for the child */
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        signal(SIGHUP, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        /* Make stdout and stderr unbuffered for immediate output */
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);

        /* Close all file descriptors except stdin, stdout, stderr for security */
        int max_fd = sysconf(_SC_OPEN_MAX);
        if (max_fd == -1) {
            max_fd = 1024; /* fallback value */
        }

        for (int fd = 3; fd < max_fd; fd++) {
            close(fd);
        }

        /* Set up environment */
        if (cmd->envp) {
            environ = cmd->envp;
        }

        /* Set up secure environment for pagers */
        if (is_secure_pager(cmd->command)) {
            setup_secure_pager_environment();
        }

        /* Set up secure environment for editors */
        if (is_secure_editor(cmd->command)) {
            setup_secure_editor_environment();
        }

        /* Change to target user privileges */
        extern int test_mode;
        if (target_pwd) {
            /* Running as specific target user */
            if (test_mode) {
                /* Test mode: skip privilege changes */
            } else if (setgid(target_pwd->pw_gid) != 0) {
                perror("setgid");
                exit(EXIT_FAILURE);
            }

            /* Set supplementary groups for target user */
            if (test_mode) {
                /* Test mode: skip privilege changes */
            } else if (initgroups(target_pwd->pw_name, target_pwd->pw_gid) != 0) {
                perror("initgroups");
                exit(EXIT_FAILURE);
            }

            if (test_mode) {
                /* Test mode: skip privilege changes */
            } else if (setuid(target_pwd->pw_uid) != 0) {
                perror("setuid");
                exit(EXIT_FAILURE);
            }

            /* Set HOME environment variable for target user */
            if (setenv("HOME", target_pwd->pw_dir, 1) != 0) {
                perror("setenv HOME");
                /* Non-fatal, continue */
            }

            /* Set USER and LOGNAME environment variables */
            if (setenv("USER", target_pwd->pw_name, 1) != 0) {
                perror("setenv USER");
                /* Non-fatal, continue */
            }

            if (setenv("LOGNAME", target_pwd->pw_name, 1) != 0) {
                perror("setenv LOGNAME");
                /* Non-fatal, continue */
            }
        } else {
            /* Default behavior - change to root privileges */
            if (test_mode) {
                /* Test mode: skip privilege changes */
            } else if (setgid(0) != 0) {
                perror("setgid");
                exit(EXIT_FAILURE);
            }

            if (test_mode) {
                /* Test mode: skip privilege changes */
            } else if (setuid(0) != 0) {
                perror("setuid");
                exit(EXIT_FAILURE);
            }
        }

        /* Handle redirection if specified */
        if (cmd->redirect_type != REDIRECT_NONE && cmd->redirect_file) {
            int redirect_fd;

            switch (cmd->redirect_type) {
                case REDIRECT_OUTPUT:
                    redirect_fd = open(cmd->redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (redirect_fd == -1) {
                        perror("open output file");
                        exit(EXIT_FAILURE);
                    }
                    if (dup2(redirect_fd, STDOUT_FILENO) == -1) {
                        perror("dup2 stdout");
                        exit(EXIT_FAILURE);
                    }
                    close(redirect_fd);
                    break;

                case REDIRECT_OUTPUT_APPEND:
                    redirect_fd = open(cmd->redirect_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (redirect_fd == -1) {
                        perror("open output file for append");
                        exit(EXIT_FAILURE);
                    }
                    if (dup2(redirect_fd, STDOUT_FILENO) == -1) {
                        perror("dup2 stdout append");
                        exit(EXIT_FAILURE);
                    }
                    close(redirect_fd);
                    break;

                case REDIRECT_INPUT:
                    redirect_fd = open(cmd->redirect_file, O_RDONLY);
                    if (redirect_fd == -1) {
                        perror("open input file");
                        exit(EXIT_FAILURE);
                    }
                    if (dup2(redirect_fd, STDIN_FILENO) == -1) {
                        perror("dup2 stdin");
                        exit(EXIT_FAILURE);
                    }
                    close(redirect_fd);
                    break;

                default:
                    break;
            }
        }

        /* Execute the command */
        execv(command_path, cmd->argv);

        /* If we get here, exec failed */
        perror("execv");
        exit(EXIT_COMMAND_NOT_FOUND);
    } else {
        /* Parent process */
        free(command_path);

        /* Set up signal handling for interactive programs */
        struct sigaction old_sigint, old_sigquit, old_sigtstp;
        struct sigaction ignore_action;

        ignore_action.sa_handler = SIG_IGN;
        sigemptyset(&ignore_action.sa_mask);
        ignore_action.sa_flags = 0;

        /* Temporarily ignore signals that should be handled by the child */
        sigaction(SIGINT, &ignore_action, &old_sigint);
        sigaction(SIGQUIT, &ignore_action, &old_sigquit);
        sigaction(SIGTSTP, &ignore_action, &old_sigtstp);

        /* Wait for child to complete */
        int wait_result;
        do {
            wait_result = waitpid(pid, &status, 0);
        } while (wait_result == -1 && errno == EINTR);

        /* Restore original signal handlers */
        sigaction(SIGINT, &old_sigint, NULL);
        sigaction(SIGQUIT, &old_sigquit, NULL);
        sigaction(SIGTSTP, &old_sigtstp, NULL);

        if (wait_result == -1) {
            if (errno != ECHILD) {  /* Don't report error if child already reaped */
                perror("waitpid");
            }
            /* Clean up file lock if it was acquired */
            if (file_lock_acquired && file_to_edit) {
                release_file_lock(file_to_edit, user->username, pid);
            }
            if (file_to_edit) {
                free(file_to_edit);
            }
            return -1;
        }

        /* Clean up file lock if it was acquired */
        if (file_lock_acquired && file_to_edit) {
            release_file_lock(file_to_edit, user->username, pid);
        }
        if (file_to_edit) {
            free(file_to_edit);
        }

        /* Return the exit status */
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            /* Don't print message for common interactive program signals */
            if (sig != SIGPIPE && sig != SIGINT && sig != SIGQUIT && sig != SIGTERM) {
                fprintf(stderr, "Command terminated by signal %d\n", sig);
            }
            return 128 + sig;
        }
    }

    return 0;
}

/**
 * Find command in PATH
 */
char *find_command_in_path(const char *command) {
    char *path_env, *path_copy, *dir, *saveptr;
    char *full_path;
    size_t path_len;

    if (!command) {
        return NULL;
    }

    /* Use secure hardcoded PATH to prevent hijacking attacks */
    path_env = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";

    path_copy = strdup(path_env);
    if (!path_copy) {
        return NULL;
    }

    /* Search each directory in PATH */
    dir = strtok_r(path_copy, ":", &saveptr);
    while (dir != NULL) {
        path_len = strlen(dir) + strlen(command) + 2;
        full_path = malloc(path_len);
        if (!full_path) {
            free(path_copy);
            return NULL;
        }

        snprintf(full_path, path_len, "%s/%s", dir, command);

        /* Check if file exists and is executable */
        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return full_path;
        }

        free(full_path);
        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);
    return NULL;
}

/**
 * Free command_info structure
 */
void free_command_info(struct command_info *cmd) {
    int i;

    if (!cmd) {
        return;
    }

    if (cmd->command) {
        free(cmd->command);
        cmd->command = NULL;
    }

    if (cmd->argv) {
        for (i = 0; i < cmd->argc; i++) {
            if (cmd->argv[i]) {
                free(cmd->argv[i]);
            }
        }
        free(cmd->argv);
        cmd->argv = NULL;
    }

    if (cmd->redirect_file) {
        free(cmd->redirect_file);
        cmd->redirect_file = NULL;
    }

    cmd->argc = 0;
    cmd->redirect_type = REDIRECT_NONE;
    cmd->redirect_append = 0;
}

/**
 * Check if command is empty or whitespace only
 */
int is_empty_command(const char *command) {
    if (!command) {
        return 1;
    }

    while (*command) {
        if (*command != ' ' && *command != '\t' && *command != '\n') {
            return 0;
        }
        command++;
    }

    return 1;
}

/**
 * Check if command contains shell operators that need special handling
 */
int contains_shell_operators(const char *input) {
    if (!input) {
        return 0;
    }

    int in_quotes = 0;
    char quote_char = 0;

    for (const char *p = input; *p; p++) {
        /* Handle escaped characters */
        if (*p == '\\' && *(p+1)) {
            p++; /* Skip the escaped character */
            continue;
        }

        if (!in_quotes && (*p == '"' || *p == '\'')) {
            in_quotes = 1;
            quote_char = *p;
        } else if (in_quotes && *p == quote_char) {
            in_quotes = 0;
            quote_char = 0;
        } else if (!in_quotes) {
            /* Check for shell operators outside of quotes */
            if (*p == '>' || *p == '<' || *p == '|' || *p == ';' || *p == '&') {
                return 1;
            }
            /* Check for command substitution */
            if (*p == '`' || (*p == '$' && *(p+1) == '(')) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Parse command with shell operators (redirection, pipes, etc.)
 */
int parse_command_with_shell_operators(const char *input, struct command_info *cmd) {
    if (!input || !cmd) {
        return -1;
    }

    /* Check for redirection operators */
    if (strchr(input, '>') || strchr(input, '<')) {
        return parse_command_with_redirection(input, cmd);
    }

    /* Check for pipe operators */
    if (strchr(input, '|')) {
        /* This should be handled by pipeline parsing, not regular command parsing */
        fprintf(stderr, "sudosh: pipe operations should be handled by pipeline parser\n");
        return -1;
    }

    /* Check for command chaining operators */
    if (strchr(input, ';') || strstr(input, "&&") || strstr(input, "||")) {
        fprintf(stderr, "sudosh: command chaining operators are not allowed for security reasons\n");
        return -1;
    }

    /* Check for background execution */
    if (strchr(input, '&')) {
        fprintf(stderr, "sudosh: background execution is not allowed for security reasons\n");
        return -1;
    }

    /* Check for command substitution */
    if (strchr(input, '`') || strstr(input, "$(")) {
        fprintf(stderr, "sudosh: command substitution is not allowed for security reasons\n");
        return -1;
    }

    /* If we get here, there's an unhandled shell operator */
    fprintf(stderr, "sudosh: unhandled shell operator in command\n");
    return -1;
}

/**
 * Parse command with redirection operators
 */
int parse_command_with_redirection(const char *input, struct command_info *cmd) {
    char *input_copy, *redirect_pos;
    char *command_part, *redirect_part;
    int argc = 0;
    int argv_size = 16;

    if (!input || !cmd) {
        return -1;
    }

    /* Initialize command structure */
    memset(cmd, 0, sizeof(struct command_info));

    input_copy = strdup(input);
    if (!input_copy) {
        return -1;
    }

    /* Find the redirection operator */
    redirect_pos = strchr(input_copy, '>');
    if (!redirect_pos) {
        redirect_pos = strchr(input_copy, '<');
    }

    if (!redirect_pos) {
        free(input_copy);
        return -1;
    }

    /* Check for multiple redirection operators (security issue) */
    char *second_redirect = strchr(redirect_pos + 1, '>');
    if (!second_redirect) {
        second_redirect = strchr(redirect_pos + 1, '<');
    }
    if (second_redirect) {
        /* Skip >> case (which is valid) */
        if (!(redirect_pos[0] == '>' && redirect_pos[1] == '>')) {
            free(input_copy);
            return -1; /* Multiple redirection operators not allowed */
        }
    }

    /* Save the redirect character before modifying the string */
    char redirect_char = *redirect_pos;

    /* Split into command part and redirection part */
    *redirect_pos = '\0';
    command_part = input_copy;
    redirect_part = redirect_pos + 1;

    /* Check for >> (append) operator */
    int is_append = 0;
    if (redirect_char == '>' && *redirect_part == '>') {
        is_append = 1;
        redirect_part++;
    }

    /* Trim whitespace from both parts */
    command_part = trim_whitespace_inplace(command_part);
    redirect_part = trim_whitespace_inplace(redirect_part);

    /* Validate the redirection target */
    if (!validate_safe_redirection(input)) {
        const char *error_msg = get_redirection_error_message(redirect_part);
        fprintf(stderr, "sudosh: %s\n", error_msg);
        fprintf(stderr, "sudosh: Safe redirection targets: /tmp/, /var/tmp/, or your home directory\n");
        free(input_copy);
        return -1;
    }

    /* Store redirection information */
    if (is_append) {
        cmd->redirect_type = REDIRECT_OUTPUT_APPEND;
        cmd->redirect_append = 1;
    } else if (redirect_char == '>') {
        cmd->redirect_type = REDIRECT_OUTPUT;
    } else if (redirect_char == '<') {
        cmd->redirect_type = REDIRECT_INPUT;
    }

    cmd->redirect_file = strdup(redirect_part);
    if (!cmd->redirect_file) {
        free(input_copy);
        return -1;
    }

    /* Parse the command part */
    cmd->argv = malloc(argv_size * sizeof(char *));
    if (!cmd->argv) {
        free(input_copy);
        return -1;
    }

    if (tokenize_command_line(command_part, &cmd->argv, &argc, &argv_size) != 0) {
        free_command_info(cmd);
        free(input_copy);
        return -1;
    }

    /* Set argc and store original command */
    cmd->argc = argc;
    cmd->argv[argc] = NULL;
    cmd->command = strdup(input);

    free(input_copy);
    return 0;
}

/**
 * Tokenize command line with proper shell parsing
 */
int tokenize_command_line(const char *input, char ***argv, int *argc, int *argv_size) {
    char *input_copy, *token, *saveptr;

    if (!input || !argv || !argc || !argv_size) {
        return -1;
    }

    input_copy = strdup(input);
    if (!input_copy) {
        return -1;
    }

    *argc = 0;

    /* Tokenize the command */
    token = strtok_r(input_copy, " \t\n", &saveptr);
    while (token != NULL) {
        /* Resize argv if needed */
        if (*argc >= *argv_size - 1) {
            *argv_size *= 2;
            char **new_argv = realloc(*argv, *argv_size * sizeof(char *));
            if (!new_argv) {
                free(input_copy);
                return -1;
            }
            *argv = new_argv;
        }

        /* Expand = expressions and store the token */
        (*argv)[*argc] = expand_equals_expression(token);
        if (!(*argv)[*argc]) {
            free(input_copy);
            return -1;
        }
        (*argc)++;

        token = strtok_r(NULL, " \t\n", &saveptr);
    }

    free(input_copy);
    return 0;
}

/**
 * Trim whitespace in place
 */
char *trim_whitespace_inplace(char *str) {
    char *end;

    if (!str) {
        return str;
    }

    /* Trim leading space */
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == 0) {
        return str;
    }

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    /* Write new null terminator */
    *(end + 1) = '\0';

    return str;
}
