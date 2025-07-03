#include "sudosh.h"

/* Global variables for logging */
static int logging_initialized = 0;

/* Global variables for session logging */
static FILE *session_log_file = NULL;
static int session_logging_enabled = 0;

/* Global variables for command history */
static FILE *history_file = NULL;
static int history_logging_enabled = 0;

/* Global variables for history navigation */
static char **history_buffer = NULL;
static int history_count = 0;
static int history_capacity = 0;

/**
 * Initialize syslog for sudosh
 */
void init_logging(void) {
    if (!logging_initialized) {
        openlog("sudosh", LOG_PID | LOG_CONS, LOG_AUTHPRIV);
        logging_initialized = 1;
    }
}

/**
 * Log command execution
 */
void log_command(const char *username, const char *command, int success) {
    char hostname[256];
    char *tty;
    char *pwd;
    
    if (!logging_initialized) {
        init_logging();
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }

    /* Get TTY */
    tty = ttyname(STDIN_FILENO);
    if (!tty) {
        tty = "unknown";
    } else {
        /* Remove /dev/ prefix if present */
        if (strncmp(tty, "/dev/", 5) == 0) {
            tty += 5;
        }
    }

    /* Get current working directory */
    pwd = getcwd(NULL, 0);
    if (!pwd) {
        pwd = strdup("unknown");
    }

    if (success) {
        syslog(LOG_COMMAND, 
               "%s : TTY=%s ; PWD=%s ; USER=root ; COMMAND=%s",
               username, tty, pwd, command);
    } else {
        syslog(LOG_ERROR,
               "%s : TTY=%s ; PWD=%s ; USER=root ; COMMAND=%s (FAILED)",
               username, tty, pwd, command);
    }

    if (pwd) {
        free(pwd);
    }
}

/**
 * Log authentication attempts
 */
void log_authentication(const char *username, int success) {
    char hostname[256];
    char *tty;
    
    if (!logging_initialized) {
        init_logging();
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }

    /* Get TTY */
    tty = ttyname(STDIN_FILENO);
    if (!tty) {
        tty = "unknown";
    } else {
        /* Remove /dev/ prefix if present */
        if (strncmp(tty, "/dev/", 5) == 0) {
            tty += 5;
        }
    }

    if (success) {
        syslog(LOG_AUTH_SUCCESS,
               "%s : TTY=%s ; authentication succeeded",
               username, tty);
    } else {
        syslog(LOG_AUTH_FAILURE,
               "%s : TTY=%s ; authentication failed",
               username, tty);
    }
}

/**
 * Log session start
 */
void log_session_start(const char *username) {
    char hostname[256];
    char *tty;
    
    if (!logging_initialized) {
        init_logging();
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }

    /* Get TTY */
    tty = ttyname(STDIN_FILENO);
    if (!tty) {
        tty = "unknown";
    } else {
        /* Remove /dev/ prefix if present */
        if (strncmp(tty, "/dev/", 5) == 0) {
            tty += 5;
        }
    }

    syslog(LOG_INFO,
           "%s : TTY=%s ; session opened for user root",
           username, tty);
}

/**
 * Log session end
 */
void log_session_end(const char *username) {
    char hostname[256];
    char *tty;
    
    if (!logging_initialized) {
        init_logging();
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }

    /* Get TTY */
    tty = ttyname(STDIN_FILENO);
    if (!tty) {
        tty = "unknown";
    } else {
        /* Remove /dev/ prefix if present */
        if (strncmp(tty, "/dev/", 5) == 0) {
            tty += 5;
        }
    }

    syslog(LOG_INFO,
           "%s : TTY=%s ; session closed for user root",
           username, tty);
}

/**
 * Log error messages
 */
void log_error(const char *message) {
    if (!logging_initialized) {
        init_logging();
    }

    syslog(LOG_ERROR, "error: %s", message);
}

/**
 * Log security violations
 */
void log_security_violation(const char *username, const char *violation) {
    char hostname[256];
    char *tty;
    
    if (!logging_initialized) {
        init_logging();
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }

    /* Get TTY */
    tty = ttyname(STDIN_FILENO);
    if (!tty) {
        tty = "unknown";
    } else {
        /* Remove /dev/ prefix if present */
        if (strncmp(tty, "/dev/", 5) == 0) {
            tty += 5;
        }
    }

    syslog(LOG_WARNING,
           "%s : TTY=%s ; SECURITY VIOLATION: %s",
           username, tty, violation);
}

/**
 * Close logging
 */
void close_logging(void) {
    if (logging_initialized) {
        closelog();
        logging_initialized = 0;
    }
}

/**
 * Initialize session logging to file
 */
int init_session_logging(const char *logfile) {
    time_t now;
    struct tm *tm_info;
    char timestamp[64];

    if (!logfile) {
        return -1;
    }

    session_log_file = fopen(logfile, "a");
    if (!session_log_file) {
        perror("Failed to open session log file");
        return -1;
    }

    session_logging_enabled = 1;

    /* Write session start header */
    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(session_log_file, "\n=== SUDOSH SESSION START: %s ===\n", timestamp);
    fflush(session_log_file);

    return 0;
}

/**
 * Log session input (commands typed by user)
 */
void log_session_input(const char *input) {
    time_t now;
    struct tm *tm_info;
    char timestamp[64];

    if (!session_logging_enabled || !session_log_file || !input) {
        return;
    }

    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

    fprintf(session_log_file, "[%s] INPUT: %s\n", timestamp, input);
    fflush(session_log_file);
}

/**
 * Log session output (command results)
 */
void log_session_output(const char *output) {
    time_t now;
    struct tm *tm_info;
    char timestamp[64];
    char *line, *output_copy, *saveptr;

    if (!session_logging_enabled || !session_log_file || !output) {
        return;
    }

    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

    /* Log each line of output separately for better readability */
    output_copy = strdup(output);
    if (!output_copy) {
        return;
    }

    line = strtok_r(output_copy, "\n", &saveptr);
    while (line != NULL) {
        fprintf(session_log_file, "[%s] OUTPUT: %s\n", timestamp, line);
        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(output_copy);
    fflush(session_log_file);
}

/**
 * Close session logging
 */
void close_session_logging(void) {
    time_t now;
    struct tm *tm_info;
    char timestamp[64];

    if (session_logging_enabled && session_log_file) {
        /* Write session end footer */
        time(&now);
        tm_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

        fprintf(session_log_file, "=== SUDOSH SESSION END: %s ===\n\n", timestamp);
        fclose(session_log_file);
        session_log_file = NULL;
        session_logging_enabled = 0;
    }
}

/**
 * Initialize command history logging
 */
int init_command_history(const char *username) {
    char history_path[PATH_MAX];
    struct passwd *pwd;

    if (!username) {
        return -1;
    }

    /* Get user's home directory */
    pwd = getpwnam(username);
    if (!pwd || !pwd->pw_dir) {
        return -1;
    }

    /* Create history file path */
    snprintf(history_path, sizeof(history_path), "%s/.sudosh_history", pwd->pw_dir);

    /* Open history file for appending */
    history_file = fopen(history_path, "a");
    if (!history_file) {
        /* Try to create the file with proper permissions */
        history_file = fopen(history_path, "w");
        if (!history_file) {
            return -1;
        }
        /* Set file permissions to be readable/writable by owner only */
        chmod(history_path, 0600);
        fclose(history_file);

        /* Reopen for appending */
        history_file = fopen(history_path, "a");
        if (!history_file) {
            return -1;
        }
    }

    history_logging_enabled = 1;
    return 0;
}

/**
 * Log command to history file
 */
void log_command_history(const char *command) {
    time_t now;
    struct tm *tm_info;
    char timestamp[64];

    if (!history_logging_enabled || !history_file || !command) {
        return;
    }

    /* Skip empty commands and built-in commands that don't need history */
    if (strlen(command) == 0 ||
        strncmp(command, "help", 4) == 0 ||
        strncmp(command, "commands", 8) == 0 ||
        strncmp(command, "exit", 4) == 0 ||
        strncmp(command, "quit", 4) == 0) {
        return;
    }

    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(history_file, "[%s] %s\n", timestamp, command);
    fflush(history_file);
}

/**
 * Close command history logging
 */
void close_command_history(void) {
    if (history_logging_enabled && history_file) {
        fclose(history_file);
        history_file = NULL;
        history_logging_enabled = 0;
    }
}

/**
 * Load command history into memory buffer for navigation
 */
int load_history_buffer(void) {
    char history_path[PATH_MAX];
    FILE *file;
    char line[1024];
    struct passwd *pwd;

    /* Get current user's home directory */
    pwd = getpwuid(getuid());
    if (!pwd || !pwd->pw_dir) {
        return -1;
    }

    /* Build history file path */
    snprintf(history_path, sizeof(history_path), "%s/.sudosh_history", pwd->pw_dir);

    /* Open history file */
    file = fopen(history_path, "r");
    if (!file) {
        /* No history file exists yet, that's okay */
        return 0;
    }

    /* Initialize history buffer */
    history_capacity = 100;
    history_buffer = malloc(history_capacity * sizeof(char *));
    if (!history_buffer) {
        fclose(file);
        return -1;
    }

    /* Read history entries */
    while (fgets(line, sizeof(line), file)) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        /* Skip empty lines */
        if (strlen(line) == 0) {
            continue;
        }

        /* Expand buffer if needed */
        if (history_count >= history_capacity) {
            history_capacity *= 2;
            char **new_buffer = realloc(history_buffer, history_capacity * sizeof(char *));
            if (!new_buffer) {
                break;
            }
            history_buffer = new_buffer;
        }

        /* Extract just the command part (after timestamp) */
        char *command_start = strchr(line, ']');
        if (command_start && command_start[1] == ' ') {
            command_start += 2;  /* Skip "] " */
            history_buffer[history_count] = strdup(command_start);
            if (history_buffer[history_count]) {
                history_count++;
            }
        }
    }

    fclose(file);
    return 0;
}

/**
 * Free history buffer
 */
void free_history_buffer(void) {
    if (history_buffer) {
        for (int i = 0; i < history_count; i++) {
            free(history_buffer[i]);
        }
        free(history_buffer);
        history_buffer = NULL;
        history_count = 0;
        history_capacity = 0;
    }
}

/**
 * Get history entry by index (0 = oldest, history_count-1 = newest)
 */
char *get_history_entry(int index) {
    if (index < 0 || index >= history_count || !history_buffer) {
        return NULL;
    }
    return history_buffer[index];
}

/**
 * Get total number of history entries
 */
int get_history_count(void) {
    return history_count;
}

/**
 * Expand history references in command (e.g., !1, !42)
 * Returns a newly allocated string that must be freed, or NULL on error
 */
char *expand_history(const char *command) {
    char *result = NULL;
    const char *src = command;
    size_t result_len = 0;
    size_t result_capacity = strlen(command) * 2 + 1;  /* Start with double the input size */

    if (!command) {
        return NULL;
    }

    /* Allocate initial result buffer */
    result = malloc(result_capacity);
    if (!result) {
        return NULL;
    }
    result[0] = '\0';

    while (*src) {
        if (*src == '!' && isdigit(*(src + 1))) {
            /* Found history expansion */
            src++;  /* Skip the '!' */

            /* Parse the number */
            char *endptr;
            long hist_num = strtol(src, &endptr, 10);

            if (hist_num > 0 && hist_num <= history_count) {
                /* Valid history number (1-based) */
                char *hist_cmd = get_history_entry(hist_num - 1);  /* Convert to 0-based */
                if (hist_cmd) {
                    size_t hist_len = strlen(hist_cmd);

                    /* Ensure we have enough space */
                    while (result_len + hist_len + 1 > result_capacity) {
                        result_capacity *= 2;
                        char *new_result = realloc(result, result_capacity);
                        if (!new_result) {
                            free(result);
                            return NULL;
                        }
                        result = new_result;
                    }

                    /* Append the history command */
                    strcat(result, hist_cmd);
                    result_len += hist_len;
                }
            } else {
                /* Invalid history number - append as-is */
                char temp[32];
                snprintf(temp, sizeof(temp), "!%ld", hist_num);
                size_t temp_len = strlen(temp);

                /* Ensure we have enough space */
                while (result_len + temp_len + 1 > result_capacity) {
                    result_capacity *= 2;
                    char *new_result = realloc(result, result_capacity);
                    if (!new_result) {
                        free(result);
                        return NULL;
                    }
                    result = new_result;
                }

                strcat(result, temp);
                result_len += temp_len;
            }

            /* Move source pointer past the number */
            src = endptr;
        } else {
            /* Regular character - copy it */
            /* Ensure we have enough space */
            if (result_len + 2 > result_capacity) {
                result_capacity *= 2;
                char *new_result = realloc(result, result_capacity);
                if (!new_result) {
                    free(result);
                    return NULL;
                }
                result = new_result;
            }

            result[result_len] = *src;
            result[result_len + 1] = '\0';
            result_len++;
            src++;
        }
    }

    return result;
}
