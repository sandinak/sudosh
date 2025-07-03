#include "sudosh.h"
#define _GNU_SOURCE  /* For getresuid on Linux */
#include <ctype.h>
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
    printf("sudosh %s - Interactive sudo shell\n", SUDOSH_VERSION);
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
 * Get current working directory for prompt
 */
static char *get_prompt_cwd(void) {
    char *cwd = getcwd(NULL, 0);
    if (!cwd) {
        return strdup("unknown");
    }
    return cwd;
}

/**
 * Print the sudosh prompt with current working directory
 */
static void print_prompt(void) {
    char *cwd = get_prompt_cwd();
    printf("sudosh:%s# ", cwd);
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
            /* Ctrl-D: Delete character at cursor */
            if (pos < len) {
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

#ifdef __linux__
    uid_t suid;
    /* Get all three UIDs: real, effective, saved */
    if (getresuid(&ruid, &euid, &suid) == 0) {
        /* If we have different real and effective UIDs, we're running suid */
        if (ruid != euid) {
            return ruid;  /* Return the real UID (the invoking user) */
        }
    }
#else
    /* On macOS and other systems, use getuid() and geteuid() */
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
