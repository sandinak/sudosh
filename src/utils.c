/**
 * utils.c - Utility Functions and Interactive Interface
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Provides utility functions, interactive command reading, history
 * navigation, tab completion, and user interface components.
 */

#include "sudosh.h"
#include <ctype.h>

/* Global target user for -u option */
char *target_user = NULL;
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <pwd.h>
#include <termios.h>

/**
 * Get user information by username
 */
struct user_info *get_user_info(const char *username) {
    struct passwd *pwd;
    struct user_info *user;

    if (!username) {
        return NULL;
    }

    pwd = getpwnam(username);
    if (!pwd) {
        return NULL;
    }

    user = malloc(sizeof(struct user_info));
    if (!user) {
        return NULL;
    }

    user->uid = pwd->pw_uid;
    user->gid = pwd->pw_gid;
    user->username = strdup(pwd->pw_name);
    user->home_dir = strdup(pwd->pw_dir);
    user->shell = strdup(pwd->pw_shell);

    if (!user->username || !user->home_dir || !user->shell) {
        free_user_info(user);
        return NULL;
    }

    return user;
}

/**
 * Free user_info structure
 */
void free_user_info(struct user_info *user) {
    if (!user) {
        return;
    }

    if (user->username) {
        free(user->username);
    }
    if (user->home_dir) {
        free(user->home_dir);
    }
    if (user->shell) {
        free(user->shell);
    }

    free(user);
}

/**
 * Trim whitespace from string
 */
char *trim_whitespace(char *str) {
    char *end;

    if (!str) {
        return NULL;
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
    end[1] = '\0';

    return str;
}

/**
 * Print banner
 */
void print_banner(void) {
    printf("Type 'help' for available commands, 'exit' to quit.\n\n");
}

/**
 * Print help message
 */
void print_help(void) {
    printf("sudosh - Interactive sudo shell\n\n");
    printf("Available built-in commands:\n");
    printf("  help          - Show this help message\n");
    printf("  commands      - List all available commands\n");
    printf("  history       - Show command history\n");
    printf("  cd <dir>      - Change current directory\n");
    printf("  pwd           - Print current working directory\n");
    printf("  exit, quit    - Exit sudosh\n");
    printf("  <command>     - Execute command as root\n\n");
    printf("Examples:\n");
    printf("  ls -la /root\n");
    printf("  systemctl status nginx\n");
    printf("  apt update\n");
    printf("  cd /var/log\n");
    printf("  pwd\n\n");
    printf("All commands are logged to syslog.\n");
}

/**
 * Print available commands
 */
void print_commands(void) {
    char *path_env, *path_copy, *dir, *saveptr;
    DIR *dirp;
    struct dirent *entry;
    char **commands = NULL;
    int command_count = 0;
    int command_capacity = 100;
    int i, j;

    printf("sudosh - Available Commands\n\n");

    /* Print built-in commands first */
    printf("Built-in commands:\n");
    printf("  cd            - Change current directory\n");
    printf("  commands      - List all available commands\n");
    printf("  exit          - Exit sudosh\n");
    printf("  help          - Show help message\n");
    printf("  pwd           - Print current working directory\n");
    printf("  quit          - Exit sudosh\n");
    printf("\n");

    /* Allocate array for system commands */
    commands = malloc(command_capacity * sizeof(char *));
    if (!commands) {
        printf("Error: Unable to allocate memory for command list\n");
        return;
    }

    /* Get PATH environment variable */
    path_env = getenv("PATH");
    if (!path_env) {
        printf("System commands: (PATH not set)\n");
        free(commands);
        return;
    }

    /* Make a copy of PATH for tokenization */
    path_copy = strdup(path_env);
    if (!path_copy) {
        printf("Error: Unable to copy PATH\n");
        free(commands);
        return;
    }

    /* Iterate through each directory in PATH */
    dir = strtok_r(path_copy, ":", &saveptr);
    while (dir != NULL) {
        dirp = opendir(dir);
        if (dirp != NULL) {
            while ((entry = readdir(dirp)) != NULL) {
                char full_path[PATH_MAX];
                struct stat st;

                /* Skip . and .. */
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }

                /* Build full path */
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);

                /* Check if it's an executable file */
                if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
                    /* Check if we already have this command */
                    int found = 0;
                    for (i = 0; i < command_count; i++) {
                        if (strcmp(commands[i], entry->d_name) == 0) {
                            found = 1;
                            break;
                        }
                    }

                    if (!found) {
                        /* Expand array if needed */
                        if (command_count >= command_capacity) {
                            command_capacity *= 2;
                            char **new_commands = realloc(commands, command_capacity * sizeof(char *));
                            if (!new_commands) {
                                break;
                            }
                            commands = new_commands;
                        }

                        /* Add command to list */
                        commands[command_count] = strdup(entry->d_name);
                        if (commands[command_count]) {
                            command_count++;
                        }
                    }
                }
            }
            closedir(dirp);
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }

    /* Sort commands alphabetically */
    for (i = 0; i < command_count - 1; i++) {
        for (j = i + 1; j < command_count; j++) {
            if (strcmp(commands[i], commands[j]) > 0) {
                char *temp = commands[i];
                commands[i] = commands[j];
                commands[j] = temp;
            }
        }
    }

    /* Print system commands */
    printf("System commands (%d found):\n", command_count);
    if (command_count > 0) {
        int cols = 4;
        int col_width = 20;

        for (i = 0; i < command_count; i++) {
            printf("  %-*s", col_width, commands[i]);
            if ((i + 1) % cols == 0) {
                printf("\n");
            }
        }
        if (command_count % cols != 0) {
            printf("\n");
        }
    } else {
        printf("  (No executable commands found in PATH)\n");
    }

    printf("\nUse 'help' for more information about built-in commands.\n");

    /* Clean up */
    for (i = 0; i < command_count; i++) {
        free(commands[i]);
    }
    free(commands);
    free(path_copy);
}

/**
 * Print command history from ~/.sudosh_history
 */
void print_history(void) {
    char history_path[PATH_MAX];
    FILE *history_file;
    char line[1024];
    int line_number = 1;
    struct passwd *pwd;

    /* Get current user's home directory */
    pwd = getpwuid(getuid());
    if (!pwd || !pwd->pw_dir) {
        printf("Error: Unable to determine home directory\n");
        return;
    }

    /* Build history file path */
    snprintf(history_path, sizeof(history_path), "%s/.sudosh_history", pwd->pw_dir);

    /* Open history file */
    history_file = fopen(history_path, "r");
    if (!history_file) {
        printf("No command history found (file: %s)\n", history_path);
        return;
    }

    printf("Command History:\n");
    printf("================\n");

    /* Read and display each line with line numbers */
    while (fgets(line, sizeof(line), history_file)) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        printf("%4d  %s\n", line_number++, line);
    }

    fclose(history_file);

    if (line_number == 1) {
        printf("(No commands in history)\n");
    } else {
        printf("\nTotal: %d commands\n", line_number - 1);
        printf("Use !<number> to execute a command from history\n");
    }
}

/**
 * Get current working directory for prompt with ~user expansion
 */
static char *get_prompt_cwd(void) {
    char *cwd = getcwd(NULL, 0);
    char *result;
    struct passwd *pwd;
    const char *effective_user;

    if (!cwd) {
        return strdup("unknown");
    }

    /* Determine which user's home directory to check */
    if (target_user) {
        effective_user = target_user;
        pwd = getpwnam(target_user);
    } else {
        /* For root operations, check root's home directory */
        effective_user = "root";
        pwd = getpwnam("root");
    }

    /* If we can get the user's home directory, check if cwd is within it */
    if (pwd && pwd->pw_dir) {
        size_t home_len = strlen(pwd->pw_dir);

        /* Check if current directory is exactly the home directory */
        if (strcmp(cwd, pwd->pw_dir) == 0) {
            result = malloc(strlen(effective_user) + 3); /* ~user + null */
            if (result) {
                sprintf(result, "~%s", effective_user);
                free(cwd);
                return result;
            }
        }
        /* Check if current directory is a subdirectory of home */
        else if (strncmp(cwd, pwd->pw_dir, home_len) == 0 && cwd[home_len] == '/') {
            /* Replace home directory path with ~user */
            const char *subpath = cwd + home_len; /* Points to the '/' after home dir */
            result = malloc(strlen(effective_user) + strlen(subpath) + 2); /* ~user + subpath + null */
            if (result) {
                sprintf(result, "~%s%s", effective_user, subpath);
                free(cwd);
                return result;
            }
        }
    }

    /* If not in home directory or couldn't process, return full path */
    return cwd;
}

/**
 * Print the sudosh prompt with current working directory
 */
static void print_prompt(void) {
    char *cwd = get_prompt_cwd();
    char hostname[256];
    char *short_hostname;

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }

    /* Get short hostname (before first dot) */
    short_hostname = strtok(hostname, ".");
    if (!short_hostname) {
        short_hostname = hostname;
    }

    if (target_user) {
        printf("%s@%s:%s## ", target_user, short_hostname, cwd);
    } else {
        printf("root@%s:%s## ", short_hostname, cwd);
    }

    free(cwd);
    fflush(stdout);
}

/**
 * Read command from user with prompt and basic line editing
 */
char *read_command(void) {
    char buffer[1024];
    char *line = NULL;
    int pos = 0;
    int len = 0;
    int c;
    struct termios old_termios, new_termios;
    static int history_index = -1;  /* -1 means not navigating history */

    /* Get current terminal settings */
    if (tcgetattr(STDIN_FILENO, &old_termios) != 0) {
        /* Fall back to simple getline if terminal control fails */
        size_t line_len = 0;
        ssize_t read_len;

        print_prompt();

        read_len = getline(&line, &line_len, stdin);
        if (read_len == -1) {
            if (line) {
                free(line);
            }
            return NULL;
        }

        /* Remove newline */
        if (read_len > 0 && line[read_len - 1] == '\n') {
            line[read_len - 1] = '\0';
        }

        return line;
    }

    /* Set up raw mode for character-by-character input */
    new_termios = old_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_termios) != 0) {
        /* Fall back to simple getline */
        size_t line_len = 0;
        ssize_t read_len;

        print_prompt();

        read_len = getline(&line, &line_len, stdin);
        if (read_len == -1) {
            if (line) {
                free(line);
            }
            return NULL;
        }

        /* Remove newline */
        if (read_len > 0 && line[read_len - 1] == '\n') {
            line[read_len - 1] = '\0';
        }

        return line;
    }

    print_prompt();

    memset(buffer, 0, sizeof(buffer));

    while (1) {
        /* Check if interrupted by SIGTERM/SIGQUIT (but not SIGINT) */
        if (is_interrupted()) {
            tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
            return NULL;
        }

        /* Reset SIGINT flag if it was received - we continue on Ctrl-C */
        if (received_sigint_signal()) {
            reset_sigint_flag();
            /* Continue processing - don't exit on Ctrl-C */
        }

        /* Use select() to implement timeout */
        fd_set readfds;
        struct timeval timeout;
        int select_result;

        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        timeout.tv_sec = INACTIVITY_TIMEOUT;
        timeout.tv_usec = 0;

        select_result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);

        if (select_result == 0) {
            /* Timeout occurred */
            tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
            printf("\nSession timeout after %d seconds of inactivity. Exiting.\n", INACTIVITY_TIMEOUT);
            return NULL;
        } else if (select_result < 0) {
            /* Error in select */
            if (errno == EINTR) {
                continue;  /* Interrupted by signal, try again */
            }
            tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
            return NULL;
        }

        /* Input is available, read it */
        c = getchar();

        if (c == EOF) {
            tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
            printf("\n");  /* Move to next line for clean exit */
            return NULL;
        }

        if (c == '\n' || c == '\r') {
            /* Enter pressed */
            printf("\n");
            break;
        } else if (c == 127 || c == '\b') {
            /* Backspace */
            if (pos > 0) {
                pos--;
                len--;
                memmove(&buffer[pos], &buffer[pos + 1], len - pos);
                buffer[len] = '\0';

                /* Redraw line */
                printf("\r\033[K");  /* Clear line */
                print_prompt();
                printf("%s", buffer);

                /* Move cursor to correct position */
                if (pos < len) {
                    printf("\033[%dD", len - pos);
                }
                fflush(stdout);
            }
        } else if (c == 1) {
            /* Ctrl-A: Beginning of line */
            if (pos > 0) {
                printf("\033[%dD", pos);
                pos = 0;
                fflush(stdout);
            }
        } else if (c == 5) {
            /* Ctrl-E: End of line */
            if (pos < len) {
                printf("\033[%dC", len - pos);
                pos = len;
                fflush(stdout);
            }
        } else if (c == 2) {
            /* Ctrl-B: Move left */
            if (pos > 0) {
                printf("\033[1D");
                pos--;
                fflush(stdout);
            }
        } else if (c == 6) {
            /* Ctrl-F: Move right */
            if (pos < len) {
                printf("\033[1C");
                pos++;
                fflush(stdout);
            }
        } else if (c == 4) {
            /* Ctrl-D: Delete character at cursor or exit if line is empty */
            if (len == 0) {
                /* Empty line - exit gracefully */
                tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
                printf("\n");  /* Move to next line for clean exit */
                return NULL;
            } else if (pos < len) {
                /* Delete character at cursor */
                memmove(&buffer[pos], &buffer[pos + 1], len - pos);
                len--;
                buffer[len] = '\0';

                /* Redraw from cursor position */
                printf("%s ", &buffer[pos]);
                printf("\033[%dD", len - pos + 1);
                fflush(stdout);
            }
        } else if (c == 11) {
            /* Ctrl-K: Kill to end of line */
            if (pos < len) {
                buffer[pos] = '\0';
                len = pos;

                /* Clear to end of line */
                printf("\033[K");
                fflush(stdout);
            }
        } else if (c == 21) {
            /* Ctrl-U: Kill entire line */
            pos = 0;
            len = 0;
            buffer[0] = '\0';

            /* Clear line and redraw prompt */
            printf("\r\033[K");
            print_prompt();
        } else if (c == 27) {
            /* Escape sequence - might be arrow keys */
            int c2 = getchar();
            if (c2 == '[') {
                int c3 = getchar();
                if (c3 == 'A') {
                    /* Up arrow - previous command in history */
                    int total_history = get_history_count();
                    if (total_history > 0) {
                        if (history_index == -1) {
                            history_index = total_history - 1;
                        } else if (history_index > 0) {
                            history_index--;
                        }

                        char *hist_cmd = get_history_entry(history_index);
                        if (hist_cmd) {
                            /* Clear current line */
                            printf("\r\033[K");
                            print_prompt();

                            /* Copy history command to buffer */
                            strncpy(buffer, hist_cmd, sizeof(buffer) - 1);
                            buffer[sizeof(buffer) - 1] = '\0';
                            len = strlen(buffer);
                            pos = len;

                            /* Display the command */
                            printf("%s", buffer);
                            fflush(stdout);
                        }
                    }
                } else if (c3 == 'B') {
                    /* Down arrow - next command in history */
                    int total_history = get_history_count();
                    if (total_history > 0 && history_index != -1) {
                        if (history_index < total_history - 1) {
                            history_index++;
                            char *hist_cmd = get_history_entry(history_index);
                            if (hist_cmd) {
                                /* Clear current line */
                                printf("\r\033[K");
                                print_prompt();

                                /* Copy history command to buffer */
                                strncpy(buffer, hist_cmd, sizeof(buffer) - 1);
                                buffer[sizeof(buffer) - 1] = '\0';
                                len = strlen(buffer);
                                pos = len;

                                /* Display the command */
                                printf("%s", buffer);
                                fflush(stdout);
                            }
                        } else {
                            /* Past the end - clear line */
                            history_index = -1;
                            printf("\r\033[K");
                            print_prompt();
                            buffer[0] = '\0';
                            len = 0;
                            pos = 0;
                        }
                    }
                }
                /* Ignore other escape sequences */
            }
        } else if (c == '\t') {
            /* Tab - perform path completion */
            char *prefix = find_completion_start(buffer, pos);
            if (prefix) {
                char **matches = complete_path(prefix, 0, strlen(prefix));
                if (matches && matches[0]) {
                    if (matches[1] == NULL) {
                        /* Single match - complete it */
                        insert_completion(buffer, &pos, &len, matches[0], prefix);

                        /* Redraw line */
                        printf("\r\033[K");
                        print_prompt();
                        printf("%s", buffer);

                        /* Move cursor to correct position */
                        if (pos < len) {
                            printf("\033[%dD", len - pos);
                        }
                        fflush(stdout);
                    } else {
                        /* Multiple matches - show them */
                        printf("\n");
                        for (int i = 0; matches[i]; i++) {
                            printf("%s  ", matches[i]);
                            if ((i + 1) % 4 == 0) {
                                printf("\n");
                            }
                        }
                        if (matches[0] && strlen(matches[0]) > 0) {
                            /* Find common prefix among matches */
                            char common[256] = {0};
                            int common_len = 0;
                            int max_common = strlen(matches[0]);

                            for (int i = 1; matches[i] && common_len < max_common; i++) {
                                for (int j = 0; j < max_common && j < (int)strlen(matches[i]); j++) {
                                    if (matches[0][j] != matches[i][j]) {
                                        max_common = j;
                                        break;
                                    }
                                }
                            }

                            if (max_common > (int)strlen(prefix)) {
                                strncpy(common, matches[0], max_common);
                                common[max_common] = '\0';
                                insert_completion(buffer, &pos, &len, common, prefix);
                            }
                        }
                        printf("\n");

                        /* Redraw prompt and line */
                        print_prompt();
                        printf("%s", buffer);

                        /* Move cursor to correct position */
                        if (pos < len) {
                            printf("\033[%dD", len - pos);
                        }
                        fflush(stdout);
                    }

                    /* Free matches */
                    for (int i = 0; matches[i]; i++) {
                        free(matches[i]);
                    }
                    free(matches);
                }
                free(prefix);
            }
        } else if (c >= 32 && c < 127) {
            /* Printable character */
            history_index = -1;  /* Reset history navigation */
            if (len < (int)sizeof(buffer) - 1) {
                /* Insert character at current position */
                memmove(&buffer[pos + 1], &buffer[pos], len - pos);
                buffer[pos] = c;
                pos++;
                len++;
                buffer[len] = '\0';

                /* Redraw from cursor position */
                printf("%s", &buffer[pos - 1]);
                if (pos < len) {
                    printf("\033[%dD", len - pos);
                }
                fflush(stdout);
            }
        }
        /* Ignore other control characters */
    }

    /* Restore terminal settings */
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);

    /* Return a copy of the buffer */
    if (len > 0) {
        line = malloc(len + 1);
        if (line) {
            strcpy(line, buffer);
        }
    } else {
        line = malloc(1);
        if (line) {
            line[0] = '\0';
        }
    }

    return line;
}

/**
 * Check if command is a built-in command
 */
int handle_builtin_command(const char *command) {
    char *trimmed = trim_whitespace(strdup(command));
    char *token, *saveptr;
    int handled = 0;

    if (!trimmed) {
        return 0;
    }

    /* Parse command and arguments */
    token = strtok_r(trimmed, " \t", &saveptr);
    if (!token) {
        free(trimmed);
        return 0;
    }

    if (strcmp(token, "help") == 0) {
        print_help();
        handled = 1;
    } else if (strcmp(token, "commands") == 0) {
        print_commands();
        handled = 1;
    } else if (strcmp(token, "history") == 0) {
        print_history();
        handled = 1;
    } else if (strcmp(token, "pwd") == 0) {
        char *cwd = getcwd(NULL, 0);
        if (cwd) {
            printf("%s\n", cwd);
            free(cwd);
        } else {
            perror("pwd");
        }
        handled = 1;
    } else if (strcmp(token, "cd") == 0) {
        char *dir = strtok_r(NULL, " \t", &saveptr);
        if (!dir) {
            /* No argument, change to home directory */
            struct passwd *pwd = getpwuid(getuid());
            if (pwd && pwd->pw_dir) {
                dir = pwd->pw_dir;
            } else {
                dir = "/";
            }
        }

        if (chdir(dir) == 0) {
            /* Successfully changed directory */
            char *new_cwd = getcwd(NULL, 0);
            if (new_cwd) {
                printf("Changed to: %s\n", new_cwd);
                free(new_cwd);
            }
        } else {
            fprintf(stderr, "cd: %s: %s\n", dir, strerror(errno));
        }
        handled = 1;
    } else if (strcmp(token, "exit") == 0 || strcmp(token, "quit") == 0) {
        printf("Goodbye!\n");
        free(trimmed);
        return -1; /* Signal to exit */
    }

    free(trimmed);
    return handled;
}

/**
 * Get real user ID (the user who invoked sudosh)
 * This mimics how sudo determines the real user
 */
uid_t get_real_uid(void) {
    uid_t ruid, euid;

#if defined(__linux__) || defined(__GLIBC__)
    uid_t suid;
    /* Get all three UIDs: real, effective, saved */
    if (getresuid(&ruid, &euid, &suid) == 0) {
        /* If we have different real and effective UIDs, we're running suid */
        if (ruid != euid) {
            return ruid;  /* Return the real UID (the invoking user) */
        }
    }
#else
    /* On macOS, BSD, and other systems, use getuid() and geteuid() */
    ruid = getuid();
    euid = geteuid();

    /* If we have different real and effective UIDs, we're running suid */
    if (ruid != euid) {
        return ruid;  /* Return the real UID (the invoking user) */
    }
#endif

    /* Fallback to getuid() if UIDs are the same or functions fail */
    return getuid();
}

/**
 * Get current username (the real user who invoked sudosh)
 */
char *get_current_username(void) {
    struct passwd *pwd;
    uid_t uid = get_real_uid();

    pwd = getpwuid(uid);
    if (!pwd) {
        return NULL;
    }

    return strdup(pwd->pw_name);
}

/**
 * Get real user info (the user who invoked sudosh)
 */
struct user_info *get_real_user_info(void) {
    uid_t real_uid = get_real_uid();
    struct passwd *pwd = getpwuid(real_uid);

    if (!pwd) {
        return NULL;
    }

    return get_user_info(pwd->pw_name);
}

/**
 * Check if string contains only whitespace
 */
int is_whitespace_only(const char *str) {
    if (!str) {
        return 1;
    }

    while (*str) {
        if (!isspace((unsigned char)*str)) {
            return 0;
        }
        str++;
    }

    return 1;
}

/**
 * Safe string copy
 */
char *safe_strdup(const char *str) {
    char *copy;
    
    if (!str) {
        return NULL;
    }

    copy = malloc(strlen(str) + 1);
    if (!copy) {
        return NULL;
    }

    strcpy(copy, str);
    return copy;
}

/**
 * Cleanup and exit
 */
void cleanup_and_exit(int exit_code) {
    cleanup_security();
    close_logging();
    exit(exit_code);
}

/**
 * Find the start of the word to complete at the given position
 */
char *find_completion_start(const char *buffer, int pos) {
    int start = pos;

    /* Move backwards to find the start of the current word */
    while (start > 0 && buffer[start - 1] != ' ' && buffer[start - 1] != '\t') {
        start--;
    }

    /* Return a copy of the prefix to complete */
    int prefix_len = pos - start;
    if (prefix_len <= 0) {
        return strdup("");
    }

    char *prefix = malloc(prefix_len + 1);
    if (!prefix) {
        return NULL;
    }

    strncpy(prefix, buffer + start, prefix_len);
    prefix[prefix_len] = '\0';

    return prefix;
}

/**
 * Complete file/directory paths
 */
char **complete_path(const char *text, int start, int end) {
    (void)start; /* Suppress unused parameter warning */
    (void)end;   /* Suppress unused parameter warning */

    char **matches = NULL;
    int match_count = 0;
    int match_capacity = 16;

    /* Allocate initial matches array */
    matches = malloc(match_capacity * sizeof(char *));
    if (!matches) {
        return NULL;
    }

    /* Extract directory and filename parts */
    char *dir_path = NULL;
    char *filename_prefix = NULL;

    char *last_slash = strrchr(text, '/');
    if (last_slash) {
        /* Path contains directory separator */
        int dir_len = last_slash - text + 1;
        dir_path = malloc(dir_len + 1);
        if (!dir_path) {
            free(matches);
            return NULL;
        }
        strncpy(dir_path, text, dir_len);
        dir_path[dir_len] = '\0';

        filename_prefix = strdup(last_slash + 1);
    } else {
        /* No directory separator - complete in current directory */
        dir_path = strdup("./");
        filename_prefix = strdup(text);
    }

    if (!filename_prefix) {
        free(dir_path);
        free(matches);
        return NULL;
    }

    /* Open directory */
    DIR *dir = opendir(dir_path);
    if (!dir) {
        free(dir_path);
        free(filename_prefix);
        free(matches);
        return NULL;
    }

    /* Read directory entries */
    struct dirent *entry;
    int prefix_len = strlen(filename_prefix);

    while ((entry = readdir(dir)) != NULL) {
        /* Skip hidden files unless prefix starts with '.' */
        if (entry->d_name[0] == '.' && (prefix_len == 0 || filename_prefix[0] != '.')) {
            continue;
        }

        /* Check if entry matches prefix */
        if (strncmp(entry->d_name, filename_prefix, prefix_len) == 0) {
            /* Expand matches array if needed */
            if (match_count >= match_capacity - 1) {
                match_capacity *= 2;
                char **new_matches = realloc(matches, match_capacity * sizeof(char *));
                if (!new_matches) {
                    /* Clean up on failure */
                    for (int i = 0; i < match_count; i++) {
                        free(matches[i]);
                    }
                    free(matches);
                    free(dir_path);
                    free(filename_prefix);
                    closedir(dir);
                    return NULL;
                }
                matches = new_matches;
            }

            /* Create full path for the match */
            char *full_match;
            if (strcmp(dir_path, "./") == 0) {
                full_match = strdup(entry->d_name);
            } else {
                int full_len = strlen(dir_path) + strlen(entry->d_name) + 1;
                full_match = malloc(full_len);
                if (full_match) {
                    strcpy(full_match, dir_path);
                    strcat(full_match, entry->d_name);
                }
            }

            if (full_match) {
                /* Check if it's a directory and add trailing slash */
                struct stat st;
                if (stat(full_match, &st) == 0 && S_ISDIR(st.st_mode)) {
                    char *dir_match = malloc(strlen(full_match) + 2);
                    if (dir_match) {
                        strcpy(dir_match, full_match);
                        strcat(dir_match, "/");
                        free(full_match);
                        full_match = dir_match;
                    }
                }

                matches[match_count++] = full_match;
            }
        }
    }

    closedir(dir);
    free(dir_path);
    free(filename_prefix);

    /* Null-terminate the matches array */
    matches[match_count] = NULL;

    return matches;
}

/**
 * Insert completion into buffer at current position
 */
void insert_completion(char *buffer, int *pos, int *len, const char *completion, const char *prefix) {
    int prefix_len = strlen(prefix);
    int completion_len = strlen(completion);
    int insert_len = completion_len - prefix_len;

    if (insert_len <= 0) {
        return; /* Nothing to insert */
    }

    /* Check if there's enough space in buffer */
    if (*len + insert_len >= 1023) { /* Leave room for null terminator */
        return;
    }

    /* Move existing text to make room */
    memmove(&buffer[*pos + insert_len], &buffer[*pos], *len - *pos);

    /* Insert the new text */
    memcpy(&buffer[*pos], completion + prefix_len, insert_len);

    /* Update position and length */
    *pos += insert_len;
    *len += insert_len;
    buffer[*len] = '\0';
}
