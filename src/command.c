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
 * Parse command line input into command structure
 */
int parse_command(const char *input, struct command_info *cmd) {
    char *input_copy, *token, *saveptr;
    int argc = 0;
    int argv_size = 16;

    if (!input || !cmd) {
        return -1;
    }

    /* Initialize command structure */
    memset(cmd, 0, sizeof(struct command_info));

    /* Make a copy of input for tokenization */
    input_copy = strdup(input);
    if (!input_copy) {
        return -1;
    }

    /* Allocate initial argv array */
    cmd->argv = malloc(argv_size * sizeof(char *));
    if (!cmd->argv) {
        free(input_copy);
        return -1;
    }

    /* Tokenize the command */
    token = strtok_r(input_copy, " \t\n", &saveptr);
    while (token != NULL) {
        /* Resize argv if needed */
        if (argc >= argv_size - 1) {
            argv_size *= 2;
            char **new_argv = realloc(cmd->argv, argv_size * sizeof(char *));
            if (!new_argv) {
                free_command_info(cmd);
                free(input_copy);
                return -1;
            }
            cmd->argv = new_argv;
        }

        /* Store the token */
        cmd->argv[argc] = strdup(token);
        if (!cmd->argv[argc]) {
            free_command_info(cmd);
            free(input_copy);
            return -1;
        }
        argc++;

        token = strtok_r(NULL, " \t\n", &saveptr);
    }

    /* Null-terminate argv */
    cmd->argv[argc] = NULL;
    cmd->argc = argc;

    /* Store the full command */
    cmd->command = strdup(input);
    if (!cmd->command) {
        free_command_info(cmd);
        free(input_copy);
        return -1;
    }

    free(input_copy);
    return 0;
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
        command_path = strdup(cmd->argv[0]);
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

        /* Change to target user privileges */
        if (target_pwd) {
            /* Running as specific target user */
            if (setgid(target_pwd->pw_gid) != 0) {
                perror("setgid");
                exit(EXIT_FAILURE);
            }

            /* Set supplementary groups for target user */
            if (initgroups(target_pwd->pw_name, target_pwd->pw_gid) != 0) {
                perror("initgroups");
                exit(EXIT_FAILURE);
            }

            if (setuid(target_pwd->pw_uid) != 0) {
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
            if (setgid(0) != 0) {
                perror("setgid");
                exit(EXIT_FAILURE);
            }

            if (setuid(0) != 0) {
                perror("setuid");
                exit(EXIT_FAILURE);
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
            return -1;
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

    cmd->argc = 0;
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
