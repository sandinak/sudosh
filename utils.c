#include "sudosh.h"
#include <ctype.h>

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
    printf("Available commands:\n");
    printf("  help          - Show this help message\n");
    printf("  exit, quit    - Exit sudosh\n");
    printf("  <command>     - Execute command as root\n\n");
    printf("Examples:\n");
    printf("  ls -la /root\n");
    printf("  systemctl status nginx\n");
    printf("  apt update\n\n");
    printf("All commands are logged to syslog.\n");
}

/**
 * Read command from user with prompt
 */
char *read_command(void) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    printf("sudosh# ");
    fflush(stdout);

    read = getline(&line, &len, stdin);
    if (read == -1) {
        if (line) {
            free(line);
        }
        return NULL;
    }

    /* Remove newline */
    if (read > 0 && line[read - 1] == '\n') {
        line[read - 1] = '\0';
    }

    return line;
}

/**
 * Check if command is a built-in command
 */
int handle_builtin_command(const char *command) {
    char *trimmed = trim_whitespace(strdup(command));
    int handled = 0;

    if (!trimmed) {
        return 0;
    }

    if (strcmp(trimmed, "help") == 0) {
        print_help();
        handled = 1;
    } else if (strcmp(trimmed, "exit") == 0 || strcmp(trimmed, "quit") == 0) {
        printf("Goodbye!\n");
        free(trimmed);
        return -1; /* Signal to exit */
    }

    free(trimmed);
    return handled;
}

/**
 * Get current username
 */
char *get_current_username(void) {
    struct passwd *pwd;
    uid_t uid = getuid();

    pwd = getpwuid(uid);
    if (!pwd) {
        return NULL;
    }

    return strdup(pwd->pw_name);
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
