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
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>
#include <pwd.h>
#include <termios.h>


/* Alias completion helper types and prototype */
typedef struct alias_collect_ctx { char ***matchesp; int *countp; int *capacityp; } alias_collect_ctx_t;
static int alias_collect_cb(const char *name, void *vctx);

/* Callback to collect alias names into completion matches (file-scope) */
static int alias_collect_cb(const char *name, void *vctx) {
    alias_collect_ctx_t *ctx = (alias_collect_ctx_t *)vctx;
    char **matches = *(ctx->matchesp);
    int match_count = *(ctx->countp);
    int match_capacity = *(ctx->capacityp);

    /* Check duplicates */
    for (int j = 0; j < match_count; j++) {
        if (strcmp(matches[j], name) == 0) return 0;
    }

    /* Expand capacity if needed */
    if (match_count >= match_capacity - 1) {
        int new_capacity = match_capacity * 2;
        char **new_matches = realloc(matches, new_capacity * sizeof(char *));
        if (!new_matches) return 1; /* stop on allocation failure */
        matches = new_matches;
        *(ctx->matchesp) = matches;
        *(ctx->capacityp) = new_capacity;
    }

    matches[match_count] = strdup(name);
    if (matches[match_count]) {
        match_count++;
        *(ctx->countp) = match_count;
    }
    return 0;
}

/* Track last exit status for prompt; defined here for utils (UI layer) */
int last_exit_status = 0;



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

    /* Show sudo CLI options if running in sudo compatibility mode */
    if (sudo_compat_mode_flag) {
        printf("Sudo-compatible CLI options (when invoked as 'sudo'):\n");
        printf("  -h, --help              Show help message\n");
        printf("  -V, --version           Show version information\n");
        printf("  -v                      Validate/update authentication timestamp\n");
        printf("  -k                      Invalidate cached authentication\n");
        printf("  -l, --list              List available commands showing permission sources\n");
        printf("  -n                      Non-interactive mode (no authentication prompts)\n");
        printf("  -u USER, --user USER    Run commands as target USER\n");
        printf("  -c COMMAND              Execute COMMAND and exit\n");
        printf("  -L FILE                 Log entire session to FILE\n");
        printf("  --verbose               Enable verbose output\n");
        printf("  --rc-alias-import       Enable importing aliases from shell rc files\n");
        printf("  --no-rc-alias-import    Disable importing aliases from shell rc files\n");
        printf("  --ansible-detect        Enable Ansible session detection\n");
        printf("  --no-ansible-detect     Disable Ansible session detection\n\n");
        printf("Note: Some standard sudo options (-E, -H, -i, -s, -A, -S, -b, -p) are\n");
        printf("      unsupported for security policy compliance.\n\n");
    }

    printf("Available built-in commands:\n");
    printf("  help, ?       - Show this help message\n");
    printf("  commands      - List all available commands\n");
    printf("  rules         - Show sudo rules, safe commands, and blocked commands\n");
    printf("  history       - Show command history\n");
    printf("  version       - Show version information\n");
    printf("  cd <dir>      - Change current directory\n");
    printf("  pwd           - Print current working directory\n");
    printf("  path          - Show PATH environment variable and inaccessible directories\n");
    printf("  alias [name[=value]] - Create or show aliases\n");
    printf("  unalias <name> - Remove alias\n");
    printf("  export [var[=value]] - Set or show environment variables\n");
    printf("  unset <var>   - Remove environment variable\n");
    printf("  env           - Show environment variables\n");
    printf("  which <cmd>   - Show command location\n");
    printf("  type <cmd>    - Show command type\n");
    printf("  pushd <dir>   - Push directory onto stack and change to it\n");
    printf("  popd          - Pop directory from stack and change to it\n");
    printf("  dirs          - Show directory stack\n");
    printf("  exit, quit    - Exit sudosh\n");
    printf("  <command>     - Execute command as root\n\n");
    printf("Examples:\n");
    printf("  ls -la /root\n");
    printf("  systemctl status nginx\n");
    printf("  apt update\n");
    printf("  cd /var/log\n");
    printf("  alias ll='ls -la'\n");
    printf("  export EDITOR=vim\n");
    printf("  pushd /tmp\n\n");
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
    printf("  alias         - Create or show aliases\n");
    printf("  cd            - Change current directory\n");
    printf("  commands      - List all available commands\n");
    printf("  dirs          - Show directory stack\n");
    printf("  env           - Show environment variables\n");
    printf("  exit          - Exit sudosh\n");
    printf("  export        - Set or show environment variables\n");
    printf("  help, ?       - Show help message\n");
    printf("  history       - Show command history\n");
    printf("  path          - Show PATH environment variable and inaccessible directories\n");
    printf("  popd          - Pop directory from stack\n");
    printf("  pushd         - Push directory onto stack\n");
    printf("  pwd           - Print current working directory\n");
    printf("  quit          - Exit sudosh\n");
    printf("  rules         - Show sudo rules, safe commands, and blocked commands\n");
    printf("  type          - Show command type\n");
    printf("  unalias       - Remove alias\n");
    printf("  unset         - Remove environment variable\n");
    printf("  version       - Show version information\n");
    printf("  which         - Show command location\n");
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
 * Expand tilde (~) in directory paths
 * Supports:
 * - ~ (current user's home)
 * - ~username (specified user's home)
 * Returns allocated string that must be freed, or NULL on error
 */
static char *expand_tilde_path(const char *path) {
    if (!path || path[0] != '~') {
        /* No tilde expansion needed */
        return strdup(path);
    }

    if (path[1] == '\0' || path[1] == '/') {
        /* ~ or ~/... - expand to current user's home */
        struct passwd *pwd = getpwuid(getuid());
        if (!pwd || !pwd->pw_dir) {
            return NULL;
        }

        if (path[1] == '\0') {
            /* Just ~ */
            return strdup(pwd->pw_dir);
        } else {
            /* ~/subpath */
            char *result = malloc(strlen(pwd->pw_dir) + strlen(path));
            if (result) {
                sprintf(result, "%s%s", pwd->pw_dir, path + 1);
            }
            return result;
        }
    } else {
        /* ~username or ~username/... - expand to specified user's home */
        const char *slash = strchr(path + 1, '/');
        char *username;

        if (slash) {
            /* ~username/subpath */
            size_t username_len = slash - (path + 1);
            username = malloc(username_len + 1);
            if (!username) {
                return NULL;
            }
            strncpy(username, path + 1, username_len);
            username[username_len] = '\0';
        } else {
            /* ~username */
            username = strdup(path + 1);
            if (!username) {
                return NULL;
            }
        }

        /* Look up the user */
        struct passwd *pwd = getpwnam(username);
        if (!pwd || !pwd->pw_dir) {
            free(username);
            return NULL;
        }

        char *result;
        if (slash) {
            /* ~username/subpath */
            result = malloc(strlen(pwd->pw_dir) + strlen(slash) + 1);
            if (result) {
                sprintf(result, "%s%s", pwd->pw_dir, slash);
            }
        } else {
            /* ~username */
            result = strdup(pwd->pw_dir);
        }

        free(username);
        return result;
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

/* Global color configuration */
static struct color_config *global_color_config = NULL;

/**
 * Initialize global color configuration
 */
static void init_global_color_config(void) {
    if (global_color_config) {
        return; /* Already initialized */
    }

    global_color_config = init_color_config();
}

/**
 * Print the sudosh prompt with current working directory and colors
 * Supports optional formatting via SUDOSH_PROMPT_FORMAT using tokens:
 *  %u username, %h hostname, %w cwd, %? last exit status, %# prompt marker (##)
 */
static void print_prompt(void) {
    char *cwd = get_prompt_cwd();
    char hostname[256];
    char *short_hostname;

    /* Initialize color configuration if needed */
    init_global_color_config();

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }

    /* Get short hostname (before first dot) */
    short_hostname = strtok(hostname, ".");
    if (!short_hostname) {
        short_hostname = hostname;
    }

    /* Resolve user component */
    const char *user_component = target_user ? target_user : "root";

    /* Optional prompt format */
    const char *fmt = getenv("SUDOSH_PROMPT_FORMAT");
    extern int last_exit_status;
    if (fmt && fmt[0] != '\0') {
        /* Build formatted prompt into a buffer */
        char out[1024]; out[0] = '\0';
        const char *p = fmt;
        while (*p && strlen(out) < sizeof(out) - 4) {
            if (*p == '%' && *(p+1)) {
                p++;
                char temp[512]; temp[0] = '\0';
                switch (*p) {
                    case 'u': strncat(out, user_component, sizeof(out)-strlen(out)-1); break;
                    case 'h': strncat(out, short_hostname, sizeof(out)-strlen(out)-1); break;
                    case 'w': strncat(out, cwd, sizeof(out)-strlen(out)-1); break;
                    case '?': {
                        char num[16]; snprintf(num, sizeof(num), "%d", last_exit_status);
                        strncat(out, num, sizeof(out)-strlen(out)-1);
                        break;
                    }
                    case '#': strncat(out, "##", sizeof(out)-strlen(out)-1); break;
                    case '%': strncat(out, "%", sizeof(out)-strlen(out)-1); break;
                    default: /* unknown token, include literally */
                        strncat(out, "%", sizeof(out)-strlen(out)-1);
                        char cstr[2] = {*p, '\0'}; strncat(out, cstr, sizeof(out)-strlen(out)-1);
                }
                p++;
                continue;
            }
            char c[2] = {*p++, '\0'}; strncat(out, c, sizeof(out)-strlen(out)-1);
        }
        /* Apply colors if enabled: colorize user@host:path then append space */
        if (global_color_config && global_color_config->colors_enabled) {
            /* Simple approach: print without embedding color inside tokens */
            printf("%s%s%s@%s%s%s:%s%s%s%s ",
                   global_color_config->username_color, user_component, global_color_config->reset_color,
                   global_color_config->hostname_color, short_hostname, global_color_config->reset_color,
                   global_color_config->path_color, cwd, global_color_config->reset_color,
                   global_color_config->prompt_color);
            /* Then print the formatted tail (which may include %# etc.) */
            printf("%s", out);
            printf(" %s", global_color_config->reset_color);
        } else {
            printf("%s@%s:%s %s ", user_component, short_hostname, cwd, out);
        }
        free(cwd);
        fflush(stdout);
        return;
    }

    /* Default colored prompt */
    if (global_color_config && global_color_config->colors_enabled) {
        printf("%s%s%s@%s%s%s:%s%s%s%s## %s",
               global_color_config->username_color, user_component, global_color_config->reset_color,
               global_color_config->hostname_color, short_hostname, global_color_config->reset_color,
               global_color_config->path_color, cwd, global_color_config->reset_color,
               global_color_config->prompt_color, global_color_config->reset_color);
    } else {
        /* Fallback to non-colored prompt */
        printf("%s@%s:%s## ", user_component, short_hostname, cwd);
    }

    free(cwd);
    fflush(stdout);
}

/* Static variables for tab completion cycling */
static char **tab_matches = NULL;  /* For cycling through completions */
static int tab_match_index = -1;   /* Current match index */
static char *tab_original_prefix = NULL;  /* Original prefix before cycling */
static int tab_prefix_start = -1;  /* Start position of prefix in buffer */
static char *tab_pending_completion = NULL;  /* Pending dangerous completion */

/**
 * Clean up tab completion state
 */
static void cleanup_tab_completion(void) {
    if (tab_matches) {
        for (int i = 0; tab_matches[i]; i++) {
            free(tab_matches[i]);
        }
        free(tab_matches);
        tab_matches = NULL;
    }
    if (tab_original_prefix) {
        free(tab_original_prefix);
        tab_original_prefix = NULL;
    }
    if (tab_pending_completion) {
        free(tab_pending_completion);
        tab_pending_completion = NULL;
    }
    tab_match_index = -1;
    tab_prefix_start = -1;
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

    /* Save terminal state globally for cleanup on exit */
    save_terminal_state();

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

        /* Handle SIGINT (Ctrl-C) - clear current line and start fresh */
        if (received_sigint_signal()) {
            reset_sigint_flag();

            /* Clear the current line */
            if (len > 0) {
                /* Move cursor to beginning of line and clear it */
                printf("\r\033[K");
                print_prompt();
                fflush(stdout);

                /* Reset buffer and position */
                memset(buffer, 0, sizeof(buffer));
                pos = 0;
                len = 0;
                history_index = -1;  /* Reset history navigation */
                cleanup_tab_completion();  /* Reset tab completion */
            } else {
                /* If line is empty, just print ^C and start new line */
                printf("^C\n");
                print_prompt();
                fflush(stdout);
            }

            /* Continue with fresh line */
            continue;
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
            cleanup_tab_completion();  /* Reset tab completion */
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
            cleanup_tab_completion();  /* Reset tab completion */
            if (pos > 0) {
                printf("\033[%dD", pos);
                pos = 0;
                fflush(stdout);
            }
        } else if (c == 5) {
            /* Ctrl-E: End of line */
            cleanup_tab_completion();  /* Reset tab completion */
            if (pos < len) {
                printf("\033[%dC", len - pos);
                pos = len;
                fflush(stdout);
            }
        } else if (c == 2) {
            /* Ctrl-B: Move left */
            cleanup_tab_completion();  /* Reset tab completion */
            if (pos > 0) {
                printf("\033[1D");
                pos--;
                fflush(stdout);
            }
        } else if (c == 6) {
            /* Ctrl-F: Move right */
            cleanup_tab_completion();  /* Reset tab completion */
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
            /* Tab - perform completion */
            char *prefix = find_completion_start(buffer, pos);
            if (prefix) {
                int prefix_start = pos - strlen(prefix);
                int is_empty_prefix = (strlen(prefix) == 0);

                /* Special case: if prefix ends with '/' and we're at the end of it,
                   treat it as empty prefix for that directory */
                int is_directory_end = 0;
                if (!is_empty_prefix && strlen(prefix) > 0 && prefix[strlen(prefix) - 1] == '/') {
                    /* Check if cursor is at the end of the prefix (right after the '/') */
                    if (pos == prefix_start + (int)strlen(prefix)) {
                        is_directory_end = 1;
                        is_empty_prefix = 1;  /* Treat as empty prefix */
                    }
                }

                /* Check if this is a pending dangerous completion */
                if (tab_pending_completion && tab_original_prefix &&
                    strcmp(prefix, tab_original_prefix) == 0 &&
                    prefix_start == tab_prefix_start) {
                    /* User pressed tab again - complete the dangerous match */
                    insert_completion(buffer, &pos, &len, tab_pending_completion, prefix);
                    cleanup_tab_completion();
                } else if (tab_matches && tab_original_prefix &&
                    strcmp(prefix, tab_original_prefix) == 0 &&
                    prefix_start == tab_prefix_start) {
                    /* Cycle to next match */
                    tab_match_index++;
                    if (!tab_matches[tab_match_index]) {
                        tab_match_index = 0; /* Wrap around */
                    }

                    /* Replace current completion with next match */
                    /* First, remove the current completion */
                    int current_completion_len = pos - tab_prefix_start;
                    memmove(&buffer[tab_prefix_start], &buffer[pos], len - pos);
                    len -= current_completion_len;
                    pos = tab_prefix_start;

                    /* Insert new completion */
                    insert_completion(buffer, &pos, &len, tab_matches[tab_match_index], tab_original_prefix);
                } else {
                    /* New completion - clean up previous state */
                    cleanup_tab_completion();

                    char **matches = NULL;
                    int displayed_list = 0;  /* Flag to track if we displayed a list */
                    char *completion_text = prefix;  /* Text to use for completion */
                    char *dir_context = NULL;  /* Directory context for empty prefix */

                    /* For empty prefix, check if we have a directory context */
                    if (is_empty_prefix) {
                        if (is_directory_end) {
                            /* Special case: we're at the end of a directory path ending with '/' */
                            completion_text = prefix;  /* Use the directory path itself */
                        } else {
                            /* Normal empty prefix case */
                            dir_context = get_directory_context_for_empty_prefix(buffer, pos);
                            if (dir_context) {
                                completion_text = dir_context;
                            }
                        }
                    }

                    /* Determine if we're completing a command or a path */
                    if (is_command_position(buffer, pos) && completion_text[0] != '/' && completion_text[0] != '.') {
                        /* Complete command names (but not if it's an absolute or relative path) */
                        matches = complete_command(completion_text);
                    } else if (is_command_position(buffer, pos) && (completion_text[0] == '/' || strncmp(completion_text, "./", 2) == 0)) {
                        /* Complete paths to executables only in command position for absolute or relative paths */
                        matches = complete_path(completion_text, 0, strlen(completion_text), 1, 0);
                    } else if (is_cd_command(buffer, pos)) {
                        /* Complete directories only for cd command */
                        matches = complete_path(completion_text, 0, strlen(completion_text), 0, 1);
                    } else {
                        /* Complete file/directory paths (all files for arguments) */
                        matches = complete_path(completion_text, 0, strlen(completion_text), 0, 0);
                    }

                    if (matches && matches[0]) {
                        if (is_empty_prefix && (matches[1] != NULL || is_directory_end)) {
                            /* Empty prefix with multiple matches OR directory end - display all options */
                            printf("\n");
                            display_matches_in_columns(matches);
                            printf("\n");
                            print_prompt();
                            printf("%s", buffer);
                            displayed_list = 1;

                            /* Free matches since we're just displaying them */
                            for (int i = 0; matches[i]; i++) {
                                free(matches[i]);
                            }
                            free(matches);
                        } else if (is_empty_prefix && matches[1] == NULL && dir_context) {
                            /* Single match in directory context for empty prefix - display it instead of auto-completing */
                            printf("\n");
                            display_matches_in_columns(matches);
                            printf("\n");
                            print_prompt();
                            printf("%s", buffer);
                            displayed_list = 1;

                            /* Free matches since we're just displaying them */
                            for (int i = 0; matches[i]; i++) {
                                free(matches[i]);
                            }
                            free(matches);
                        } else if (matches[1] == NULL) {
                            /* Single match - check if it's safe to auto-complete */
                            int prefix_len = strlen(prefix);
                            int match_len = strlen(matches[0]);
                            int is_dangerous_command = 0;

                            /* Check if this is a potentially dangerous command */
                            if (is_command_position(buffer, pos) == 0) {
                                /* This is an argument, check if the command is dangerous */
                                char *cmd_start = buffer;
                                while (*cmd_start == ' ' || *cmd_start == '\t') cmd_start++;
                                if (strncmp(cmd_start, "rm ", 3) == 0 ||
                                    strncmp(cmd_start, "rmdir ", 6) == 0 ||
                                    strncmp(cmd_start, "mv ", 3) == 0 ||
                                    strncmp(cmd_start, "cp ", 3) == 0) {
                                    is_dangerous_command = 1;
                                }
                            }

                            /* If the completion is much longer than prefix and it's a dangerous command,
                               show it instead of auto-completing */
                            if (is_dangerous_command && prefix_len > 0 && match_len > prefix_len * 2) {
                                printf("\n");
                                printf("Potential match: %s\n", matches[0]);
                                printf("Press Tab again to complete, or continue typing to refine.\n");
                                print_prompt();
                                printf("%s", buffer);
                                displayed_list = 1;

                                /* Store for potential second tab completion */
                                tab_pending_completion = strdup(matches[0]);
                                tab_original_prefix = strdup(prefix);
                                tab_prefix_start = prefix_start;

                                /* Free matches since we're just displaying */
                                free(matches[0]);
                                free(matches);
                            } else {
                                /* Safe to auto-complete */
                                insert_completion(buffer, &pos, &len, matches[0], prefix);

                                /* Free matches since we're not cycling */
                                free(matches[0]);
                                free(matches);
                            }
                        } else {
                            /* Multiple matches - set up for cycling through all matches */
                            tab_matches = matches;
                            tab_original_prefix = strdup(prefix);
                            tab_prefix_start = prefix_start;
                            tab_match_index = 0;

                            /* Complete with first match */
                            insert_completion(buffer, &pos, &len, matches[0], prefix);
                        }
                    } else if (is_empty_prefix) {
                        /* No matches for empty prefix - show helpful message */
                        printf("\n");
                        if (is_command_position(buffer, pos)) {
                            printf("(No commands available)\n");
                        } else if (is_cd_command(buffer, pos)) {
                            if (dir_context) {
                                printf("(No directories found in %s)\n", dir_context);
                            } else {
                                printf("(No directories found)\n");
                            }
                        } else {
                            if (dir_context) {
                                printf("(No files or directories found in %s)\n", dir_context);
                            } else {
                                printf("(No files or directories found)\n");
                            }
                        }
                        printf("\n");
                        print_prompt();
                        printf("%s", buffer);
                        displayed_list = 1;
                    }

                    /* Clean up directory context */
                    if (dir_context) {
                        free(dir_context);
                    }

                    /* Redraw line if not already done above */
                    if (!displayed_list) {
                        printf("\r\033[K");
                        print_prompt();
                        printf("%s", buffer);
                    }
                }

                /* Move cursor to correct position */
                if (pos < len) {
                    printf("\033[%dD", len - pos);
                }
                fflush(stdout);

                free(prefix);
            }
        } else if (c >= 32 && c < 127) {
            /* Printable character */
            history_index = -1;  /* Reset history navigation */
            cleanup_tab_completion();  /* Reset tab completion */
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

    /* Clean up tab completion state */
    cleanup_tab_completion();

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
 * Validate PATH for security issues
 */
int validate_path_security(const char *path_env) {
    if (!path_env) {
        return 0; /* No PATH is a security issue */
    }

    char *path_copy = strdup(path_env);
    if (!path_copy) {
        return 0;
    }

    char *dir, *saveptr;
    int issues_found = 0;

    dir = strtok_r(path_copy, ":", &saveptr);
    while (dir != NULL) {
        /* Check for current directory (.) */
        if (strcmp(dir, ".") == 0) {
            printf("⚠️  Security Issue: Current directory (.) in PATH\n");
            printf("   Risk: Commands in current directory may be executed unintentionally\n");
            issues_found++;
        }

        /* Check for empty directory (::) */
        if (strlen(dir) == 0) {
            printf("⚠️  Security Issue: Empty directory in PATH\n");
            printf("   Risk: Equivalent to current directory in PATH\n");
            issues_found++;
        }

        /* Check for relative paths */
        if (dir[0] != '/') {
            printf("⚠️  Security Issue: Relative path '%s' in PATH\n", dir);
            printf("   Risk: Path resolution depends on current directory\n");
            issues_found++;
        }

        /* Check if directory exists and is accessible */
        struct stat st;
        if (stat(dir, &st) != 0) {
            printf("⚠️  Warning: PATH directory '%s' does not exist or is not accessible\n", dir);
        } else if (!S_ISDIR(st.st_mode)) {
            printf("⚠️  Warning: PATH entry '%s' is not a directory\n", dir);
        }

        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);
    return issues_found == 0;
}

/**
 * Print PATH information with minimal, clean output
 */
void print_path_info(void) {
    const char *path_env = getenv("PATH");

    if (!path_env) {
        printf("(no PATH set)\n");
        return;
    }

    /* Print the raw PATH value */
    printf("%s\n", path_env);

    /* Check each directory and report only inaccessible ones */
    char *path_copy = strdup(path_env);
    if (!path_copy) {
        return;
    }

    char *dir, *saveptr;

    dir = strtok_r(path_copy, ":", &saveptr);
    while (dir != NULL) {
        /* Skip empty directories */
        if (strlen(dir) > 0) {
            /* Check if directory is accessible */
            if (access(dir, R_OK) != 0) {
                printf("INACCESSIBLE: %s\n", dir);
            }
        }

        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);
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

    if (strcmp(token, "help") == 0 || strcmp(token, "?") == 0) {
        print_help();
        handled = 1;
    } else if (strcmp(token, "commands") == 0) {
        print_commands();
        handled = 1;
    } else if (strcmp(token, "rules") == 0) {
        char *username = get_current_username();
        if (username) {
            execute_with_pager(list_available_commands_detailed, username);
            free(username);
        } else {
            printf("Error: Could not determine current user\n");
        }
        handled = 1;
    } else if (strcmp(token, "history") == 0) {
        print_history();
        handled = 1;
    } else if (strcmp(token, "version") == 0) {
        printf("sudosh %s\n", SUDOSH_VERSION);
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
    } else if (strcmp(token, "path") == 0) {
        print_path_info();
        handled = 1;
    } else if (strcmp(token, "cd") == 0) {
        char *dir = strtok_r(NULL, " \t", &saveptr);
        char *expanded_dir = NULL;

        if (!dir) {
            /* No argument, change to home directory */
            struct passwd *pwd = getpwuid(getuid());
            if (pwd && pwd->pw_dir) {
                expanded_dir = strdup(pwd->pw_dir);
            } else {
                expanded_dir = strdup("/");
            }
        } else {
            /* Expand tilde in the provided directory path */
            expanded_dir = expand_tilde_path(dir);
            if (!expanded_dir) {
                if (dir[0] == '~') {
                    /* Tilde expansion failed */
                    char *username_end = strchr(dir + 1, '/');
                    if (username_end) {
                        *username_end = '\0';
                        fprintf(stderr, "cd: %s: No such user\n", dir + 1);
                        *username_end = '/';
                    } else {
                        fprintf(stderr, "cd: %s: No such user\n", dir + 1);
                    }
                } else {
                    fprintf(stderr, "cd: %s: Memory allocation failed\n", dir);
                }
                handled = 1;
                goto cd_cleanup;
            }
        }

        if (chdir(expanded_dir) == 0) {
            /* Successfully changed directory - silent operation per Unix philosophy */
        } else {
            fprintf(stderr, "cd: %s: %s\n", expanded_dir, strerror(errno));
        }

cd_cleanup:
        if (expanded_dir) {
            free(expanded_dir);
        }
        handled = 1;
    } else if (strcmp(token, "alias") == 0) {
        char *args = strtok_r(NULL, "", &saveptr);
        if (!args || strlen(trim_whitespace(args)) == 0) {
            /* No arguments, print all aliases */
            print_aliases();
        } else {
            /* Parse alias definition */
            char *equals = strchr(args, '=');
            if (equals) {
                *equals = '\0';
                char *name = trim_whitespace(args);
                char *value = trim_whitespace(equals + 1);

                /* Remove quotes from value if present */
                if ((value[0] == '"' && value[strlen(value)-1] == '"') ||
                    (value[0] == '\'' && value[strlen(value)-1] == '\'')) {
                    value[strlen(value)-1] = '\0';
                    value++;
                }

                if (add_alias(name, value)) {
                    printf("alias %s='%s'\n", name, value);
                } else {
                    fprintf(stderr, "alias: invalid alias name or value\n");
                }
            } else {
                /* Show specific alias */
                char *name = trim_whitespace(args);
                char *value = get_alias_value(name);
                if (value) {
                    printf("alias %s='%s'\n", name, value);
                } else {
                    fprintf(stderr, "alias: %s: not found\n", name);
                }
            }
        }
        handled = 1;
    } else if (strcmp(token, "unalias") == 0) {
        char *name = strtok_r(NULL, " \t", &saveptr);
        if (!name) {
            fprintf(stderr, "unalias: usage: unalias name\n");
        } else {
            if (remove_alias(name)) {
                /* Silent success per Unix philosophy */
            } else {
                fprintf(stderr, "unalias: %s: not found\n", name);
            }
        }
        handled = 1;
    } else if (strcmp(token, "export") == 0) {
        handled = handle_export_command(command);
    } else if (strcmp(token, "unset") == 0) {
        handled = handle_unset_command(command);
    } else if (strcmp(token, "env") == 0) {
        print_environment();
        handled = 1;
    } else if (strcmp(token, "which") == 0) {
        handled = handle_which_command(command);
    } else if (strcmp(token, "type") == 0) {
        handled = handle_type_command(command);
    } else if (strcmp(token, "pushd") == 0) {
        char *dir = strtok_r(NULL, " \t", &saveptr);
        if (!dir) {
            fprintf(stderr, "pushd: usage: pushd directory\n");
        } else {
            char *expanded_dir = expand_tilde_path(dir);
            if (expanded_dir) {
                if (!pushd(expanded_dir)) {
                    fprintf(stderr, "pushd: %s: %s\n", expanded_dir, strerror(errno));
                }
                free(expanded_dir);
            } else {
                fprintf(stderr, "pushd: %s: No such user\n", dir);
            }
        }
        handled = 1;
    } else if (strcmp(token, "popd") == 0) {
        popd();
        handled = 1;
    } else if (strcmp(token, "dirs") == 0) {
        print_dirs();
        handled = 1;
    } else if (strcmp(token, "exit") == 0 || strcmp(token, "quit") == 0) {
        /* Silent exit per Unix philosophy */
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

/* Global terminal state for restoration */
static struct termios *saved_terminal_state = NULL;
static int terminal_state_saved = 0;

/**
 * Save current terminal state for later restoration
 */
void save_terminal_state(void) {
    if (!terminal_state_saved && isatty(STDIN_FILENO)) {
        saved_terminal_state = malloc(sizeof(struct termios));
        if (saved_terminal_state && tcgetattr(STDIN_FILENO, saved_terminal_state) == 0) {
            terminal_state_saved = 1;
        } else {
            if (saved_terminal_state) {
                free(saved_terminal_state);
                saved_terminal_state = NULL;
            }
        }
    }
}

/**
 * Restore terminal state if it was saved
 */
void restore_terminal_state(void) {
    if (terminal_state_saved && saved_terminal_state && isatty(STDIN_FILENO)) {
        tcsetattr(STDIN_FILENO, TCSANOW, saved_terminal_state);
        terminal_state_saved = 0;
        free(saved_terminal_state);
        saved_terminal_state = NULL;
    }
}

/**
 * Cleanup and exit
 */
void cleanup_and_exit(int exit_code) {
    /* Restore terminal state if needed */
    restore_terminal_state();

    /* Clean up Ansible detection info */
    if (global_ansible_info) {
        free_ansible_detection_info(global_ansible_info);
        global_ansible_info = NULL;
    }

    cleanup_color_config();
    cleanup_security();
    close_logging();
    exit(exit_code);
}

/**
 * Get terminal width, with fallback to 80 columns
 */
int get_terminal_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
    return 80; /* Default fallback */
}

/**
 * Display completion matches in clean, aligned columns
 */
void display_matches_in_columns(char **matches) {
    if (!matches || !matches[0]) {
        return;
    }

    /* Count matches and find maximum width */
    int match_count = 0;
    int max_width = 0;

    for (int i = 0; matches[i]; i++) {
        match_count++;
        int len = strlen(matches[i]);
        if (len > max_width) {
            max_width = len;
        }
    }

    if (match_count == 0) {
        return;
    }

    /* Get terminal width and calculate optimal columns */
    int terminal_width = get_terminal_width();
    int column_width = max_width + 2; /* Add 2 spaces for padding */
    int num_columns = terminal_width / column_width;

    /* Ensure at least 1 column and reasonable maximum */
    if (num_columns < 1) num_columns = 1;
    if (num_columns > 8) num_columns = 8; /* Reasonable maximum */

    /* Calculate number of rows needed */
    int num_rows = (match_count + num_columns - 1) / num_columns;

    /* Display matches in column-major order (like ls) */
    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_columns; col++) {
            int index = col * num_rows + row;
            if (index < match_count) {
                printf("%-*s", column_width, matches[index]);
            }
        }
        printf("\n");
    }
}

/**
 * Complete command names from PATH
 */
char **complete_command(const char *text) {
    char **matches = NULL;
    int match_count = 0;
    int match_capacity = 16;

    /* Allocate initial matches array */
    matches = malloc(match_capacity * sizeof(char *));
    if (!matches) {
        return NULL;
    }

    /* Get PATH environment variable */
    char *path_env = getenv("PATH");
    if (!path_env) {
        free(matches);
        return NULL;
    }

    /* Make a copy of PATH for tokenization */
    char *path_copy = strdup(path_env);
    if (!path_copy) {
        free(matches);
        return NULL;
    }

    int text_len = strlen(text);
    char *dir, *saveptr;

    /* Search each directory in PATH */
    dir = strtok_r(path_copy, ":", &saveptr);
    while (dir != NULL) {
        DIR *dirp = opendir(dir);
        if (dirp != NULL) {
            struct dirent *entry;
            while ((entry = readdir(dirp)) != NULL) {
                /* Skip . and .. */
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }

                /* Check if entry matches prefix */
                if (strncmp(entry->d_name, text, text_len) == 0) {
                    /* Build full path to check if it's executable */
                    char full_path[PATH_MAX];
                    snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);

                    struct stat st;
                    if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
                        /* Check if we already have this command */
                        int found = 0;
                        for (int i = 0; i < match_count; i++) {
                            if (strcmp(matches[i], entry->d_name) == 0) {
                                found = 1;
                                break;
                            }
                        }

                        if (!found) {
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
                                    free(path_copy);
                                    closedir(dirp);
                                    return NULL;
                                }
                                matches = new_matches;
                            }

                            /* Add command to matches */
                            matches[match_count] = strdup(entry->d_name);
                            if (matches[match_count]) {
                                match_count++;
                            }
                        }
                    }
                }
            }
            closedir(dirp);
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);

    /* Add built-in commands that match */
    const char *builtins[] = {"help", "commands", "history", "pwd", "path", "cd", "exit", "quit",
                              "alias", "unalias", "export", "unset", "env", "which", "type",
                              "pushd", "popd", "dirs", "version", "rules", NULL};
    for (int i = 0; builtins[i]; i++) {
        if (strncmp(builtins[i], text, text_len) == 0) {
            /* Check if we already have this command */
            int found = 0;
            for (int j = 0; j < match_count; j++) {
                if (strcmp(matches[j], builtins[i]) == 0) {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                /* Expand matches array if needed */
                if (match_count >= match_capacity - 1) {
                    match_capacity *= 2;
                    char **new_matches = realloc(matches, match_capacity * sizeof(char *));
                    if (!new_matches) {
                        /* Clean up on failure */
                        for (int j = 0; j < match_count; j++) {
                            free(matches[j]);
                        }
                        free(matches);
                        return NULL;
                    }
                    matches = new_matches;
                }

                matches[match_count] = strdup(builtins[i]);
                if (matches[match_count]) {
                    match_count++;
                }
            }
        }
    }

    /* Also include alias names matching the prefix */
    /* Use a small context struct to carry state into the callback */
    struct alias_collect_ctx { char ***matchesp; int *countp; int *capacityp; } acc = { &matches, &match_count, &match_capacity };
    /* Callback defined at file scope */
    alias_iterate_names_with_prefix(text, alias_collect_cb, &acc);

    /* Null-terminate the matches array */
    matches[match_count] = NULL;

    return matches;
}

/**
 * Check if we're completing the first word (command) or subsequent words (arguments)
 */
int is_command_position(const char *buffer, int pos) {
    /* Find the start of the current word */
    int start = pos;
    while (start > 0 && buffer[start - 1] != ' ' && buffer[start - 1] != '\t') {
        start--;
    }

    /* Check if there are any non-whitespace characters before this word */
    for (int i = 0; i < start; i++) {
        if (buffer[i] != ' ' && buffer[i] != '\t') {
            return 0; /* Not the first word */
        }
    }

    return 1; /* This is the first word (command position) */
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
 * Complete = expansion (like zsh)
 */
char **complete_equals_expansion(const char *text) {
    if (!text || text[0] != '=') {
        return NULL;
    }

    /* Skip the = character */
    const char *command_prefix = text + 1;
    if (strlen(command_prefix) == 0) {
        return NULL;
    }

    char **matches = NULL;
    int match_count = 0;
    int match_capacity = 16;

    /* Allocate initial matches array */
    matches = malloc(match_capacity * sizeof(char *));
    if (!matches) {
        return NULL;
    }

    /* Get PATH environment variable */
    char *path_env = getenv("PATH");
    if (!path_env) {
        free(matches);
        return NULL;
    }

    /* Make a copy of PATH for tokenization */
    char *path_copy = strdup(path_env);
    if (!path_copy) {
        free(matches);
        return NULL;
    }

    int prefix_len = strlen(command_prefix);
    char *dir, *saveptr;

    /* Search each directory in PATH */
    dir = strtok_r(path_copy, ":", &saveptr);
    while (dir != NULL) {
        DIR *dirp = opendir(dir);
        if (dirp != NULL) {
            struct dirent *entry;
            while ((entry = readdir(dirp)) != NULL) {
                /* Skip . and .. */
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }

                /* Check if entry matches prefix */
                if (strncmp(entry->d_name, command_prefix, prefix_len) == 0) {
                    /* Build full path to check if it's executable */
                    char full_path[PATH_MAX];
                    snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);

                    struct stat st;
                    if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
                        /* Check if we already have this command */
                        int found = 0;
                        for (int j = 0; j < match_count; j++) {
                            if (strcmp(matches[j], full_path) == 0) {
                                found = 1;
                                break;
                            }
                        }

                        if (!found) {
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
                                    free(path_copy);
                                    closedir(dirp);
                                    return NULL;
                                }
                                matches = new_matches;
                            }

                            /* Add full path to matches */
                            matches[match_count] = strdup(full_path);
                            if (matches[match_count]) {
                                match_count++;
                            }
                        }
                    }
                }
            }
            closedir(dirp);
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);

    /* Sort matches alphabetically */
    for (int i = 0; i < match_count - 1; i++) {
        for (int j = i + 1; j < match_count; j++) {
            if (strcmp(matches[i], matches[j]) > 0) {
                char *temp = matches[i];
                matches[i] = matches[j];
                matches[j] = temp;
            }
        }
    }

    /* Null-terminate the matches array */
    matches[match_count] = NULL;

    return matches;
}

/**
 * Extract directory context for empty prefix completion
 * Returns the directory path that should be used for completion when prefix is empty
 * For example: "ls /etc/ " should return "/etc/" for completion context
 */
char *get_directory_context_for_empty_prefix(const char *buffer, int pos) {
    if (!buffer || pos <= 0) {
        return NULL;
    }

    /* Look backwards from current position to find the last directory path */
    int end = pos;

    /* Skip any trailing whitespace */
    while (end > 0 && (buffer[end - 1] == ' ' || buffer[end - 1] == '\t')) {
        end--;
    }

    /* If we don't end with a '/', there's no directory context */
    if (end == 0 || buffer[end - 1] != '/') {
        return NULL;
    }

    /* Find the start of the path (look for space or start of buffer) */
    int start = end - 1;  /* Start from the '/' */
    while (start > 0 && buffer[start - 1] != ' ' && buffer[start - 1] != '\t') {
        start--;
    }

    /* Extract the directory path */
    int path_len = end - start;
    if (path_len <= 0) {
        return NULL;
    }

    char *dir_path = malloc(path_len + 1);
    if (!dir_path) {
        return NULL;
    }

    strncpy(dir_path, buffer + start, path_len);
    dir_path[path_len] = '\0';

    return dir_path;
}

/**
 * Check if we're completing for a cd command
 */
int is_cd_command(const char *buffer, int pos) {
    if (!buffer || pos < 2) return 0;

    /* Find the start of the current command */
    int cmd_start = 0;
    for (int i = 0; i < pos; i++) {
        if (buffer[i] != ' ' && buffer[i] != '\t') {
            cmd_start = i;
            break;
        }
    }

    /* Check if command starts with "cd" followed by whitespace or end */
    if (cmd_start + 2 <= pos &&
        buffer[cmd_start] == 'c' &&
        buffer[cmd_start + 1] == 'd' &&
        (cmd_start + 2 == pos || buffer[cmd_start + 2] == ' ' || buffer[cmd_start + 2] == '\t')) {
        return 1;
    }

    return 0;
}

/**
 * Complete usernames for tilde expansion
 */
static char **complete_usernames(const char *text) {
    char **matches = NULL;
    int match_count = 0;
    int match_capacity = 16;

    /* Allocate initial matches array */
    matches = malloc(match_capacity * sizeof(char *));
    if (!matches) {
        return NULL;
    }

    /* Extract username prefix (skip the ~) */
    const char *username_prefix = text + 1;
    int prefix_len = strlen(username_prefix);

    /* First, try to get users from getpwent() (local users) */
    struct passwd *pwd;
    setpwent();

    while ((pwd = getpwent()) != NULL) {
        /* For empty prefix (~), show regular users (UID >= 1000) and common system users */
        if (prefix_len == 0) {
            /* Show regular users and some common system users */
            if (pwd->pw_uid >= 1000 ||
                strcmp(pwd->pw_name, "root") == 0 ||
                strcmp(pwd->pw_name, "nobody") == 0 ||
                strcmp(pwd->pw_name, "www-data") == 0 ||
                strcmp(pwd->pw_name, "nginx") == 0 ||
                strcmp(pwd->pw_name, "apache") == 0) {
                /* Include this user */
            } else {
                continue;
            }
        } else {
            /* For partial usernames, check if username matches prefix */
            if (strncmp(pwd->pw_name, username_prefix, prefix_len) != 0) {
                continue;
            }
        }

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
                endpwent();
                return NULL;
            }
            matches = new_matches;
        }

        /* Create the completion match with ~ prefix */
        /* Add trailing slash for single matches to indicate it's a directory */
        char *match;
        if (prefix_len > 0 && strlen(pwd->pw_name) > (size_t)prefix_len) {
            /* Partial match - add trailing slash to indicate directory */
            match = malloc(strlen(pwd->pw_name) + 3);
            if (match) {
                sprintf(match, "~%s/", pwd->pw_name);
            }
        } else {
            /* Exact match or empty prefix - no trailing slash yet */
            match = malloc(strlen(pwd->pw_name) + 2);
            if (match) {
                sprintf(match, "~%s", pwd->pw_name);
            }
        }

        if (match) {
            matches[match_count++] = match;
        }
    }

    endpwent();

    /* If we have a specific prefix and found no matches, try to check if the
     * current user matches the prefix (for external user databases) */
    if (prefix_len > 0 && match_count == 0) {
        struct passwd *current_user = getpwuid(getuid());
        if (current_user && strncmp(current_user->pw_name, username_prefix, prefix_len) == 0) {
            /* Current user matches the prefix - add it as a completion */
            char *match;
            if (strlen(current_user->pw_name) > (size_t)prefix_len) {
                match = malloc(strlen(current_user->pw_name) + 3);
                if (match) {
                    sprintf(match, "~%s/", current_user->pw_name);
                }
            } else {
                match = malloc(strlen(current_user->pw_name) + 2);
                if (match) {
                    sprintf(match, "~%s", current_user->pw_name);
                }
            }

            if (match) {
                matches[match_count++] = match;
            }
        }

        /* Also try to check if the exact prefix is a valid user */
        if (match_count == 0) {
            struct passwd *prefix_user = getpwnam(username_prefix);
            if (prefix_user) {
                /* The prefix itself is a valid username */
                char *match = malloc(strlen(username_prefix) + 2);
                if (match) {
                    sprintf(match, "~%s", username_prefix);
                    matches[match_count++] = match;
                }
            }
        }
    }

    /* Sort matches alphabetically */
    for (int i = 0; i < match_count - 1; i++) {
        for (int j = i + 1; j < match_count; j++) {
            if (strcmp(matches[i], matches[j]) > 0) {
                char *temp = matches[i];
                matches[i] = matches[j];
                matches[j] = temp;
            }
        }
    }

    /* Null-terminate the matches array */
    matches[match_count] = NULL;

    return matches;
}

/**
 * Complete file/directory paths
 */
char **complete_path(const char *text, int start, int end, int executables_only, int directories_only) {
    (void)start; /* Suppress unused parameter warning */
    (void)end;   /* Suppress unused parameter warning */

    /* Check for = expansion first */
    if (text && text[0] == '=') {
        return complete_equals_expansion(text);
    }

    char **matches = NULL;
    int match_count = 0;
    int match_capacity = 16;

    /* Allocate initial matches array */
    matches = malloc(match_capacity * sizeof(char *));
    if (!matches) {
        return NULL;
    }

    /* Handle tilde expansion for path completion */
    char *expanded_text = NULL;
    const char *working_text = text;

    if (text && text[0] == '~') {
        /* Handle tilde expansion */
        char *last_slash = strrchr(text, '/');

        if (!last_slash) {
            /* Just ~username - complete usernames */
            /* This handles both ~ and ~partial_username cases */
            return complete_usernames(text);
        } else {
            /* ~username/path - expand the tilde part and complete the path */
            char *tilde_part = malloc(last_slash - text + 1);
            if (!tilde_part) {
                free(matches);
                return NULL;
            }
            strncpy(tilde_part, text, last_slash - text);
            tilde_part[last_slash - text] = '\0';

            char *expanded_tilde = expand_tilde_path(tilde_part);
            free(tilde_part);

            if (!expanded_tilde) {
                /* Tilde expansion failed - try username completion for the tilde part */
                free(matches);
                return complete_usernames(text);
            }

            /* Create the expanded text for completion */
            expanded_text = malloc(strlen(expanded_tilde) + strlen(last_slash) + 1);
            if (!expanded_text) {
                free(expanded_tilde);
                free(matches);
                return NULL;
            }
            sprintf(expanded_text, "%s%s", expanded_tilde, last_slash);
            free(expanded_tilde);
            working_text = expanded_text;
        }
    }

    /* Extract directory and filename parts */
    char *dir_path = NULL;
    char *filename_prefix = NULL;

    char *last_slash = strrchr(working_text, '/');
    if (last_slash) {
        /* Path contains directory separator */
        int dir_len = last_slash - working_text + 1;
        dir_path = malloc(dir_len + 1);
        if (!dir_path) {
            if (expanded_text) free(expanded_text);
            free(matches);
            return NULL;
        }
        strncpy(dir_path, working_text, dir_len);
        dir_path[dir_len] = '\0';

        filename_prefix = strdup(last_slash + 1);
    } else {
        /* No directory separator - complete in current directory */
        dir_path = strdup("./");
        filename_prefix = strdup(working_text);
    }

    if (!filename_prefix) {
        free(dir_path);
        if (expanded_text) free(expanded_text);
        free(matches);
        return NULL;
    }

    /* Open directory */
    DIR *dir = opendir(dir_path);
    if (!dir) {
        free(dir_path);
        free(filename_prefix);
        if (expanded_text) free(expanded_text);
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
                    if (expanded_text) free(expanded_text);
                    closedir(dir);
                    return NULL;
                }
                matches = new_matches;
            }

            /* Create the completion match */
            char *full_match;

            if (expanded_text && text[0] == '~') {
                /* For tilde expansion, show the original tilde format */
                char *last_slash = strrchr(text, '/');
                if (last_slash) {
                    /* ~username/path format */
                    int tilde_len = last_slash - text + 1;
                    full_match = malloc(tilde_len + strlen(entry->d_name) + 1);
                    if (full_match) {
                        strncpy(full_match, text, tilde_len);
                        full_match[tilde_len] = '\0';
                        strcat(full_match, entry->d_name);
                    }
                } else {
                    /* This shouldn't happen in this context, but handle it */
                    full_match = strdup(entry->d_name);
                }
            } else {
                /* Normal path completion */
                int full_len = strlen(dir_path) + strlen(entry->d_name) + 1;
                full_match = malloc(full_len);
                if (full_match) {
                    strcpy(full_match, dir_path);
                    /* dir_path already includes trailing slash */
                    strcat(full_match, entry->d_name);
                }
            }

            if (full_match) {
                /* Build full path for stat() check */
                char *stat_path;
                if (strcmp(dir_path, "./") == 0) {
                    stat_path = strdup(entry->d_name);
                } else {
                    int stat_len = strlen(dir_path) + strlen(entry->d_name) + 1;
                    stat_path = malloc(stat_len);
                    if (stat_path) {
                        strcpy(stat_path, dir_path);
                        strcat(stat_path, entry->d_name);
                    }
                }

                if (stat_path) {
                    struct stat st;
                    if (stat(stat_path, &st) == 0) {
                        if (S_ISDIR(st.st_mode)) {
                            /* Add trailing slash for directories */
                            char *dir_match = malloc(strlen(full_match) + 2);
                            if (dir_match) {
                                strcpy(dir_match, full_match);
                                strcat(dir_match, "/");
                                free(full_match);
                                full_match = dir_match;
                            }
                        } else if (directories_only) {
                            /* If filtering for directories only, skip non-directories */
                            free(full_match);
                            free(stat_path);
                            continue;
                        } else if (executables_only) {
                            /* If filtering for executables only, check if file is executable */
                            if (!S_ISREG(st.st_mode) || !(st.st_mode & S_IXUSR)) {
                                /* Not an executable file - skip this match */
                                free(full_match);
                                free(stat_path);
                                continue;
                            }
                        }
                    } else if (directories_only || executables_only) {
                        /* Can't stat file and we need specific type - skip */
                        free(full_match);
                        free(stat_path);
                        continue;
                    }
                    free(stat_path);
                }

                matches[match_count++] = full_match;
            }
        }
    }

    closedir(dir);
    free(dir_path);
    free(filename_prefix);
    if (expanded_text) free(expanded_text);

    /* Sort matches alphabetically */
    for (int i = 0; i < match_count - 1; i++) {
        for (int j = i + 1; j < match_count; j++) {
            if (strcmp(matches[i], matches[j]) > 0) {
                char *temp = matches[i];
                matches[i] = matches[j];
                matches[j] = temp;
            }
        }
    }

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

    /* Calculate the prefix start position */
    int prefix_start = *pos - prefix_len;

    /* Validate the prefix position */
    if (prefix_start < 0 || prefix_start + prefix_len > *len) {
        return; /* Invalid prefix position */
    }

    /* Verify that the prefix actually matches what's in the buffer */
    if (prefix_len > 0 && strncmp(&buffer[prefix_start], prefix, prefix_len) != 0) {
        return; /* Prefix mismatch */
    }

    /* Calculate how much the buffer will change */
    int size_change = completion_len - prefix_len;

    /* Check if there's enough space in buffer */
    if (*len + size_change >= 1023) { /* Leave room for null terminator */
        return;
    }

    /* Replace the prefix with the full completion */
    if (size_change != 0) {
        /* Move the text after the prefix */
        memmove(&buffer[prefix_start + completion_len],
                &buffer[prefix_start + prefix_len],
                *len - (prefix_start + prefix_len));
    }

    /* Copy the full completion */
    memcpy(&buffer[prefix_start], completion, completion_len);

    /* Update position and length */
    *pos = prefix_start + completion_len;
    *len += size_change;
    buffer[*len] = '\0';
}

/**
 * Initialize color configuration with default values
 */
struct color_config *init_color_config(void) {
    struct color_config *config = malloc(sizeof(struct color_config));
    if (!config) {
        return NULL;
    }

    /* Initialize with default colors */
    strncpy(config->username_color, ANSI_GREEN, MAX_COLOR_CODE_LENGTH - 1);
    strncpy(config->hostname_color, ANSI_BLUE, MAX_COLOR_CODE_LENGTH - 1);
    strncpy(config->path_color, ANSI_CYAN, MAX_COLOR_CODE_LENGTH - 1);
    strncpy(config->prompt_color, ANSI_WHITE, MAX_COLOR_CODE_LENGTH - 1);
    strncpy(config->reset_color, ANSI_RESET, MAX_COLOR_CODE_LENGTH - 1);

    config->username_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
    config->hostname_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
    config->path_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
    config->prompt_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
    config->reset_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';

    /* Check if terminal supports colors */
    config->colors_enabled = detect_terminal_colors();

    /* Apply environment variable overrides and shell-specific defaults */
    int colors_found = 0;

    /* Check for manual color override */
    const char *sudosh_colors = getenv("SUDOSH_COLORS");
    if (sudosh_colors && strcmp(sudosh_colors, "yellow") == 0) {
        /* Use yellow for all components to match user's shell */
        strncpy(config->username_color, ANSI_YELLOW, MAX_COLOR_CODE_LENGTH - 1);
        strncpy(config->hostname_color, ANSI_YELLOW, MAX_COLOR_CODE_LENGTH - 1);
        strncpy(config->path_color, ANSI_YELLOW, MAX_COLOR_CODE_LENGTH - 1);
        strncpy(config->prompt_color, ANSI_YELLOW, MAX_COLOR_CODE_LENGTH - 1);
        colors_found = 1;
    }

    /* Try to parse colors from PS1 environment variable (bash) */
    if (!colors_found) {
        const char *ps1 = getenv("PS1");
        if (ps1) {
            colors_found = parse_ps1_colors(ps1, config);
        }
    }

    /* Try to parse colors from PROMPT environment variable (zsh) */
    if (!colors_found) {
        const char *prompt = getenv("PROMPT");
        if (prompt) {
            colors_found = parse_zsh_prompt_colors(prompt, config);
        }
    }

    /* Check for shell-specific color detection */
    if (!colors_found) {
        const char *shell = getenv("SHELL");
        if (shell && strstr(shell, "zsh")) {
            /* For zsh without PROMPT set, try some common zsh color schemes */
            /* Many zsh themes use yellow for user@host */
            strncpy(config->username_color, ANSI_YELLOW, MAX_COLOR_CODE_LENGTH - 1);
            strncpy(config->hostname_color, ANSI_YELLOW, MAX_COLOR_CODE_LENGTH - 1);
            strncpy(config->path_color, ANSI_CYAN, MAX_COLOR_CODE_LENGTH - 1);
            strncpy(config->prompt_color, ANSI_YELLOW, MAX_COLOR_CODE_LENGTH - 1);
            colors_found = 1;
        }
    }

    return config;
}

/**
 * Free color configuration
 */
void free_color_config(struct color_config *config) {
    if (config) {
        free(config);
    }
}

/**
 * Detect if terminal supports colors
 */
int detect_terminal_colors(void) {
    const char *term = getenv("TERM");
    const char *colorterm = getenv("COLORTERM");

    /* Check if we're in a TTY */
    if (!isatty(STDOUT_FILENO)) {
        return 0;
    }

    /* Check COLORTERM environment variable */
    if (colorterm && (strcmp(colorterm, "truecolor") == 0 ||
                      strcmp(colorterm, "24bit") == 0 ||
                      strcmp(colorterm, "yes") == 0)) {
        return 1;
    }

    /* Check TERM environment variable */
    if (term) {
        /* Common color-capable terminals */
        if (strstr(term, "color") ||
            strstr(term, "xterm") ||
            strstr(term, "screen") ||
            strstr(term, "tmux") ||
            strstr(term, "rxvt") ||
            strcmp(term, "linux") == 0) {
            return 1;
        }
    }

    /* Default to no colors if we can't determine */
    return 0;
}

/**
 * Extract ANSI color code from PS1-style escape sequence
 */
static int extract_ansi_color(const char *sequence, char *output, size_t output_size) {
    const char *start = sequence;
    const char *end;
    int color_len;

    /* Look for \033[ or \e[ pattern */
    if (strncmp(start, "\\033[", 5) == 0) {
        start += 5;
    } else if (strncmp(start, "\\e[", 3) == 0) {
        start += 3;
    } else if (strncmp(start, "\033[", 2) == 0) {
        start += 2;
    } else {
        return 0;
    }

    /* Find the end of the color code (usually 'm') */
    end = strchr(start, 'm');
    if (!end) {
        return 0;
    }

    /* Calculate the length of just the color code part */
    color_len = end - start;

    /* Check if we have enough space for the full escape sequence */
    if (color_len + 4 >= (int)output_size) { /* \033[ + color + m + \0 */
        return 0;
    }

    /* Build the proper ANSI escape sequence */
    output[0] = '\033';
    output[1] = '[';
    memcpy(output + 2, start, color_len);
    output[2 + color_len] = 'm';
    output[3 + color_len] = '\0';

    return 1;
}

/**
 * Convert zsh color name to ANSI escape sequence
 */
static int zsh_color_to_ansi(const char *color_name, char *output, size_t output_size) {
    if (!color_name || !output || output_size < 8) {
        return 0;
    }

    /* Map common zsh color names to ANSI codes */
    if (strcmp(color_name, "yellow") == 0) {
        strcpy(output, ANSI_YELLOW);
        return 1;
    } else if (strcmp(color_name, "green") == 0) {
        strcpy(output, ANSI_GREEN);
        return 1;
    } else if (strcmp(color_name, "blue") == 0) {
        strcpy(output, ANSI_BLUE);
        return 1;
    } else if (strcmp(color_name, "cyan") == 0) {
        strcpy(output, ANSI_CYAN);
        return 1;
    } else if (strcmp(color_name, "red") == 0) {
        strcpy(output, ANSI_RED);
        return 1;
    } else if (strcmp(color_name, "magenta") == 0) {
        strcpy(output, ANSI_MAGENTA);
        return 1;
    } else if (strcmp(color_name, "white") == 0) {
        strcpy(output, ANSI_WHITE);
        return 1;
    } else if (strcmp(color_name, "black") == 0) {
        strcpy(output, ANSI_BLACK);
        return 1;
    }

    return 0;
}

/**
 * Parse zsh-style prompt sequences with %{ %} delimiters
 */
int parse_zsh_prompt_colors(const char *prompt, struct color_config *config) {
    if (!prompt || !config) {
        return 0;
    }

    const char *ptr = prompt;
    char temp_color[MAX_COLOR_CODE_LENGTH];
    int found_colors = 0;

    /* Look for zsh-style color sequences */
    while ((ptr = strstr(ptr, "%{")) != NULL) {
        ptr += 2; /* Skip %{ */

        /* Find the closing %} */
        const char *end = strstr(ptr, "%}");
        if (!end) {
            break;
        }

        /* Check for %F{color} format */
        if (strncmp(ptr, "%F{", 3) == 0) {
            const char *color_start = ptr + 3;
            const char *color_end = strchr(color_start, '}');
            if (color_end && color_end < end) {
                /* Extract color name */
                int color_len = color_end - color_start;
                if (color_len > 0 && color_len < 20) {
                    char color_name[21];
                    strncpy(color_name, color_start, color_len);
                    color_name[color_len] = '\0';

                    /* Convert to ANSI */
                    if (zsh_color_to_ansi(color_name, temp_color, sizeof(temp_color))) {
                        /* Simple heuristic: assign colors in order found */
                        if (found_colors == 0) {
                            strncpy(config->username_color, temp_color, MAX_COLOR_CODE_LENGTH - 1);
                            config->username_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
                        } else if (found_colors == 1) {
                            strncpy(config->hostname_color, temp_color, MAX_COLOR_CODE_LENGTH - 1);
                            config->hostname_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
                        } else if (found_colors == 2) {
                            strncpy(config->path_color, temp_color, MAX_COLOR_CODE_LENGTH - 1);
                            config->path_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
                        }
                        found_colors++;
                    }
                }
            }
        } else {
            /* Try to extract ANSI color code directly */
            if (extract_ansi_color(ptr, temp_color, sizeof(temp_color))) {
                /* Simple heuristic: assign colors in order found */
                if (found_colors == 0) {
                    strncpy(config->username_color, temp_color, MAX_COLOR_CODE_LENGTH - 1);
                    config->username_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
                } else if (found_colors == 1) {
                    strncpy(config->hostname_color, temp_color, MAX_COLOR_CODE_LENGTH - 1);
                    config->hostname_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
                } else if (found_colors == 2) {
                    strncpy(config->path_color, temp_color, MAX_COLOR_CODE_LENGTH - 1);
                    config->path_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
                }
                found_colors++;
            }
        }

        ptr = end + 2; /* Move past %} */
    }

    return found_colors > 0;
}

/**
 * Parse PS1 environment variable to extract color codes
 */
int parse_ps1_colors(const char *ps1, struct color_config *config) {
    if (!ps1 || !config) {
        return 0;
    }

    const char *ptr = ps1;
    char temp_color[MAX_COLOR_CODE_LENGTH];
    int found_colors = 0;

    /* Look for bash-style color sequences in PS1 */
    while ((ptr = strstr(ptr, "\\[")) != NULL) {
        ptr += 2; /* Skip \[ */

        /* Find the closing \] */
        const char *end = strstr(ptr, "\\]");
        if (!end) {
            break;
        }

        /* Extract the color code */
        if (extract_ansi_color(ptr, temp_color, sizeof(temp_color))) {
            /* Simple heuristic: assign colors in order found */
            if (found_colors == 0) {
                strncpy(config->username_color, temp_color, MAX_COLOR_CODE_LENGTH - 1);
                config->username_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
            } else if (found_colors == 1) {
                strncpy(config->hostname_color, temp_color, MAX_COLOR_CODE_LENGTH - 1);
                config->hostname_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
            } else if (found_colors == 2) {
                strncpy(config->path_color, temp_color, MAX_COLOR_CODE_LENGTH - 1);
                config->path_color[MAX_COLOR_CODE_LENGTH - 1] = '\0';
            }
            found_colors++;
        }

        ptr = end + 2; /* Move past \] */
    }

    return found_colors > 0;
}

/**
 * Preserve color-related environment variables from the calling shell
 */
void preserve_color_environment(void) {
    /* List of color-related environment variables to preserve */
    const char *color_vars[] = {
        "PS1",
        "TERM",
        "COLORTERM",
        "LS_COLORS",
        "GREP_COLORS",
        "CLICOLOR",
        "CLICOLOR_FORCE",
        NULL
    };

    static char *preserved_vars[8] = {NULL}; /* Static storage for preserved values */
    int i;

    /* Preserve the values before environment sanitization */
    for (i = 0; color_vars[i] && i < 7; i++) {
        const char *value = getenv(color_vars[i]);
        if (value && !preserved_vars[i]) {
            /* Store the variable=value format */
            size_t len = strlen(color_vars[i]) + strlen(value) + 2;
            preserved_vars[i] = malloc(len);
            if (preserved_vars[i]) {
                snprintf(preserved_vars[i], len, "%s=%s", color_vars[i], value);
            }
        }
    }

    /* Restore the values after sanitization (if they were removed) */
    for (i = 0; preserved_vars[i] && i < 7; i++) {
        putenv(preserved_vars[i]); /* Note: putenv takes ownership of the string */
    }
}

/**
 * Cleanup global color configuration
 */
void cleanup_color_config(void) {
    if (global_color_config) {
        free_color_config(global_color_config);
        global_color_config = NULL;
    }
}

/**
 * Get terminal height for paging
 */
int get_terminal_height(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_row;
    }
    return 24; /* Default fallback */
}

/**
 * Execute function with pager if output is too long
 */
void execute_with_pager(void (*func)(const char*), const char *arg) {
    /* For now, just execute directly - paging can be added later if needed */
    func(arg);
}
