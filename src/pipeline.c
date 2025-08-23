/**
 * pipeline.c - Secure Pipeline Execution for Sudosh
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * This file implements secure pipeline execution that allows chaining
 * whitelisted commands while maintaining security isolation.
 */

#include "sudosh.h"

/* Whitelist of commands allowed in pipelines */
static const char *whitelisted_pipe_commands[] = {
    /* Text processing utilities */
    "awk", "sed", "grep", "egrep", "fgrep", "cut", "sort", "uniq", 
    "head", "tail", "tr", "wc", "nl", "cat", "tac", "rev",
    
    /* System information commands */
    "ps", "ls", "df", "du", "who", "w", "id", "whoami", "date",
    "uptime", "uname", "hostname", "pwd", "env", "printenv", "echo",
    
    /* Pagers and viewers */
    "less", "more", "cat",
    
    /* Network utilities (read-only) */
    "ping", "traceroute", "nslookup", "dig", "host",
    
    /* File utilities (read-only) */
    "file", "stat", "find", "locate", "which", "whereis", "type",
    
    NULL
};

/* Commands that require secure environment setup */
static const char *secure_pager_commands[] = {
    "less", "more", NULL
};

/**
 * Check if a command is whitelisted for pipeline use
 */
int is_whitelisted_pipe_command(const char *command) {
    if (!command) {
        return 0;
    }
    
    /* Extract just the command name (first word) */
    char *cmd_copy = strdup(command);
    if (!cmd_copy) {
        return 0;
    }
    
    const char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }
    
    /* Remove path if present - check basename */
    char *basename_cmd = strrchr(cmd_name, '/');
    if (basename_cmd) {
        cmd_name = basename_cmd + 1;
    }
    
    /* Check against whitelist */
    for (int i = 0; whitelisted_pipe_commands[i]; i++) {
        if (strcmp(cmd_name, whitelisted_pipe_commands[i]) == 0) {
            free(cmd_copy);
            return 1;
        }
    }
    
    free(cmd_copy);
    return 0;
}

/**
 * Check if input contains a pipeline
 */
int is_pipeline_command(const char *input) {
    if (!input) {
        return 0;
    }

    /* Look for pipe character not in quotes or command substitutions */
    int in_quotes = 0;
    int in_substitution = 0;
    char quote_char = 0;
    int pipe_count = 0;

    for (const char *p = input; *p; p++) {
        if (!in_quotes && !in_substitution && (*p == '"' || *p == '\'')) {
            in_quotes = 1;
            quote_char = *p;
        } else if (in_quotes && *p == quote_char) {
            in_quotes = 0;
            quote_char = 0;
        } else if (!in_quotes && !in_substitution && (*p == '$' && (*(p+1) == '(' || *(p+1) == '{'))) {
            /* Start of command substitution */
            in_substitution = 1;
        } else if (!in_quotes && !in_substitution && *p == '`') {
            /* Backtick command substitution */
            in_substitution = 1;
        } else if (in_substitution && (*p == ')' || *p == '}' || *p == '`')) {
            /* End of command substitution */
            in_substitution = 0;
        } else if (!in_quotes && !in_substitution && *p == '|') {
            /* Check for || (logical OR) */
            if (*(p+1) == '|') {
                /* This is ||, not a pipe */
                p++; /* Skip the next | */
                continue;
            }
            pipe_count++;
        }
    }

    /* Must have at least one pipe and not be malformed */
    if (pipe_count == 0) {
        return 0;
    }

    /* Check for malformed pipes (leading, trailing, or consecutive) */
    const char *trimmed = input;
    while (*trimmed && isspace(*trimmed)) trimmed++;

    if (*trimmed == '|') {
        /* Leading pipe */
        return 0;
    }

    /* Find last non-whitespace character */
    const char *end = input + strlen(input) - 1;
    while (end > input && isspace(*end)) end--;

    if (*end == '|') {
        /* Trailing pipe */
        return 0;
    }

    return 1;
}

/**
 * Parse pipeline command into individual commands
 */
int parse_pipeline(const char *input, struct pipeline_info *pipeline) {
    if (!input || !pipeline) {
        return -1;
    }

    /* Check for empty input */
    if (strlen(input) == 0) {
        return -1;
    }

    /* Validate that this is actually a pipeline command */
    if (!is_pipeline_command(input)) {
        return -1;
    }

    /* Initialize pipeline structure */
    memset(pipeline, 0, sizeof(struct pipeline_info));
    
    /* Count number of commands (pipes + 1) */
    int pipe_count = 0;
    int in_quotes = 0;
    char quote_char = 0;
    
    for (const char *p = input; *p; p++) {
        if (!in_quotes && (*p == '"' || *p == '\'')) {
            in_quotes = 1;
            quote_char = *p;
        } else if (in_quotes && *p == quote_char) {
            in_quotes = 0;
            quote_char = 0;
        } else if (!in_quotes && *p == '|') {
            pipe_count++;
        }
    }
    
    pipeline->num_commands = pipe_count + 1;
    pipeline->num_pipes = pipe_count;
    
    /* Allocate memory for commands */
    pipeline->commands = calloc(pipeline->num_commands, sizeof(struct pipeline_command));
    if (!pipeline->commands) {
        return -1;
    }
    
    /* Allocate memory for pipe file descriptors */
    if (pipeline->num_pipes > 0) {
        pipeline->pipe_fds = calloc(pipeline->num_pipes * 2, sizeof(int));
        if (!pipeline->pipe_fds) {
            free(pipeline->commands);
            return -1;
        }
    }
    
    /* Parse individual commands */
    char *input_copy = strdup(input);
    if (!input_copy) {
        free_pipeline_info(pipeline);
        return -1;
    }
    
    char *cmd_start = input_copy;
    int cmd_index = 0;
    in_quotes = 0;
    quote_char = 0;
    
    for (char *p = input_copy; *p || cmd_index < pipeline->num_commands; p++) {
        if (*p && !in_quotes && (*p == '"' || *p == '\'')) {
            in_quotes = 1;
            quote_char = *p;
        } else if (*p && in_quotes && *p == quote_char) {
            in_quotes = 0;
            quote_char = 0;
        } else if ((!*p || (!in_quotes && *p == '|')) && cmd_index < pipeline->num_commands) {
            /* End of command found */
            if (*p == '|') {
                *p = '\0';
            }
            
            /* Trim whitespace */
            while (isspace(*cmd_start)) cmd_start++;
            char *cmd_end = cmd_start + strlen(cmd_start) - 1;
            while (cmd_end > cmd_start && isspace(*cmd_end)) {
                *cmd_end = '\0';
                cmd_end--;
            }
            
            /* Parse this command */
            if (parse_command(cmd_start, &pipeline->commands[cmd_index].cmd) != 0) {
                free(input_copy);
                free_pipeline_info(pipeline);
                return -1;
            }
            
            cmd_index++;
            cmd_start = p + 1;
        }
    }
    
    free(input_copy);
    return 0;
}

/**
 * Validate pipeline security
 */
int validate_pipeline_security(struct pipeline_info *pipeline) {
    if (!pipeline || !pipeline->commands) {
        return 0;
    }
    
    /* Check each command in the pipeline */
    for (int i = 0; i < pipeline->num_commands; i++) {
        struct command_info *cmd = &pipeline->commands[i].cmd;
        
        if (!cmd->argv || !cmd->argv[0]) {
            return 0;
        }
        
        /* Check if command is whitelisted */
        if (!is_whitelisted_pipe_command(cmd->argv[0])) {
            fprintf(stderr, "sudosh: command '%s' is not allowed in pipelines\n", cmd->argv[0]);
            return 0;
        }
        
        /* Additional security checks for specific commands */
        if (strstr(cmd->argv[0], "find")) {
            /* Check for dangerous find options */
            for (int j = 1; j < cmd->argc; j++) {
                if (cmd->argv[j] && (strcmp(cmd->argv[j], "-exec") == 0 || 
                                   strcmp(cmd->argv[j], "-execdir") == 0 ||
                                   strcmp(cmd->argv[j], "-delete") == 0)) {
                    fprintf(stderr, "sudosh: dangerous find option '%s' not allowed in pipelines\n", cmd->argv[j]);
                    return 0;
                }
            }
        }
    }
    
    return 1;
}

/**
 * Check if command is a secure pager
 */
int is_secure_pager_command(const char *command) {
    if (!command) {
        return 0;
    }

    char *cmd_copy = strdup(command);
    if (!cmd_copy) {
        return 0;
    }

    const char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }

    /* Remove path if present */
    char *basename_cmd = strrchr(cmd_name, '/');
    if (basename_cmd) {
        cmd_name = basename_cmd + 1;
    }

    for (int i = 0; secure_pager_commands[i]; i++) {
        if (strcmp(cmd_name, secure_pager_commands[i]) == 0) {
            free(cmd_copy);
            return 1;
        }
    }

    free(cmd_copy);
    return 0;
}



/**
 * Execute pipeline with security isolation and comprehensive audit logging
 */
int execute_pipeline(struct pipeline_info *pipeline, struct user_info *user) {
    (void)user;  /* Suppress unused parameter warning */

    if (!pipeline || !pipeline->commands || pipeline->num_commands == 0) {
        return -1;
    }

    /* Validate pipeline security first */
    if (!validate_pipeline_security(pipeline)) {
        return -1;
    }

    /* Log the start of pipeline execution */
    log_pipeline_start(pipeline);

    /* Create pipes */
    for (int i = 0; i < pipeline->num_pipes; i++) {
        if (pipe(&pipeline->pipe_fds[i * 2]) == -1) {
            perror("pipe");
            return -1;
        }
    }

    /* Execute each command in the pipeline */
    for (int i = 0; i < pipeline->num_commands; i++) {
        struct pipeline_command *pcmd = &pipeline->commands[i];
        struct command_info *cmd = &pcmd->cmd;

        /* Find command path */
        char *command_path;
        if (cmd->argv[0][0] != '/') {
            command_path = find_command_in_path(cmd->argv[0]);
            if (!command_path) {
                fprintf(stderr, "sudosh: %s: command not found\n", cmd->argv[0]);
                return -1;
            }
        } else {
            command_path = strdup(cmd->argv[0]);
            if (!command_path) {
                return -1;
            }
        }

        /* Fork process for this command */
        pcmd->pid = fork();
        if (pcmd->pid == -1) {
            perror("fork");
            free(command_path);
            return -1;
        } else if (pcmd->pid == 0) {
            /* Child process */

            /* Set up input redirection */
            if (i > 0) {
                /* Not the first command - read from previous pipe */
                dup2(pipeline->pipe_fds[(i-1) * 2], STDIN_FILENO);
            }

            /* Set up output redirection */
            if (i < pipeline->num_commands - 1) {
                /* Not the last command - write to next pipe */
                dup2(pipeline->pipe_fds[i * 2 + 1], STDOUT_FILENO);
            }

            /* Close all pipe file descriptors */
            for (int j = 0; j < pipeline->num_pipes * 2; j++) {
                close(pipeline->pipe_fds[j]);
            }

            /* Handle file redirection for this command */
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

            /* Setup secure environment for pagers */
            if (is_secure_pager_command(cmd->argv[0])) {
                setup_secure_pager_environment();
            }

            /* Execute the command */
            execv(command_path, cmd->argv);

            /* If we get here, exec failed */
            perror("execv");
            exit(EXIT_FAILURE);
        } else {
            /* Parent process */
            free(command_path);
        }
    }

    /* Close all pipe file descriptors in parent */
    for (int i = 0; i < pipeline->num_pipes * 2; i++) {
        close(pipeline->pipe_fds[i]);
    }

    /* Wait for all child processes */
    int final_status = 0;
    for (int i = 0; i < pipeline->num_commands; i++) {
        int status;
        waitpid(pipeline->commands[i].pid, &status, 0);
        if (i == pipeline->num_commands - 1) {
            /* Use exit status of last command */
            final_status = WEXITSTATUS(status);
        }
    }

    /* Log pipeline completion */
    log_pipeline_completion(pipeline, final_status);

    return final_status;
}

/**
 * Free pipeline information structure
 */
void free_pipeline_info(struct pipeline_info *pipeline) {
    if (!pipeline) {
        return;
    }

    if (pipeline->commands) {
        for (int i = 0; i < pipeline->num_commands; i++) {
            free_command_info(&pipeline->commands[i].cmd);
        }
        free(pipeline->commands);
    }

    if (pipeline->pipe_fds) {
        free(pipeline->pipe_fds);
    }

    memset(pipeline, 0, sizeof(struct pipeline_info));
}

/**
 * Log the start of pipeline execution
 */
void log_pipeline_start(struct pipeline_info *pipeline) {
    if (!pipeline) {
        return;
    }

    char *username = get_current_username();
    if (!username) {
        username = "unknown";
    }

    /* Log overall pipeline start */
    syslog(LOG_INFO, "PIPELINE_START: user=%s commands=%d", username, pipeline->num_commands);

    /* Log each individual command in the pipeline */
    for (int i = 0; i < pipeline->num_commands; i++) {
        struct command_info *cmd = &pipeline->commands[i].cmd;
        if (cmd->command) {
            syslog(LOG_INFO, "PIPELINE_CMD[%d]: user=%s command=%s", i, username, cmd->command);

            /* Also log to command history if available */
            log_command_with_ansible_context(username, cmd->command, 1);
        }
    }
}

/**
 * Log the completion of pipeline execution
 */
void log_pipeline_completion(struct pipeline_info *pipeline, int exit_code) {
    if (!pipeline) {
        return;
    }

    char *username = get_current_username();
    if (!username) {
        username = "unknown";
    }

    /* Log pipeline completion with exit code */
    syslog(LOG_INFO, "PIPELINE_COMPLETE: user=%s commands=%d exit_code=%d",
           username, pipeline->num_commands, exit_code);

    /* Log individual command completions */
    for (int i = 0; i < pipeline->num_commands; i++) {
        struct command_info *cmd = &pipeline->commands[i].cmd;
        if (cmd->command) {
            syslog(LOG_INFO, "PIPELINE_CMD_COMPLETE[%d]: user=%s command=%s",
                   i, username, cmd->command);
        }
    }
}

/**
 * Enhanced pipeline validation with individual command permission checking
 */
int validate_pipeline_with_permissions(struct pipeline_info *pipeline, const char *username) {
    if (!pipeline || !pipeline->commands || !username) {
        return 0;
    }

    /* First do basic pipeline security validation */
    if (!validate_pipeline_security(pipeline)) {
        return 0;
    }

    /* Then check each command against user permissions */
    for (int i = 0; i < pipeline->num_commands; i++) {
        struct command_info *cmd = &pipeline->commands[i].cmd;

        if (!cmd->argv || !cmd->argv[0] || !cmd->command) {
            fprintf(stderr, "sudosh: invalid command in pipeline at position %d\n", i);
            return 0;
        }

        /* Check if user has permission to run this specific command */
        if (!check_command_permission(username, cmd->command)) {
            fprintf(stderr, "sudosh: %s is not allowed to run '%s' in pipeline\n",
                    username, cmd->command);
            log_security_violation(username, "unauthorized command in pipeline");
            return 0;
        }

        /* Log the permission check */
        syslog(LOG_INFO, "PIPELINE_PERMISSION_CHECK: user=%s command=%s result=allowed",
               username, cmd->command);
    }

    return 1;
}
