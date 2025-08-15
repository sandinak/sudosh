/**
 * filelock.c - File Locking System for Sudosh
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Implements file locking to prevent concurrent editing of the same file
 * by multiple users. Provides exclusive locks with metadata tracking.
 */

#include "sudosh.h"
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

/**
 * Initialize file locking system
 */
int init_file_locking(void) {
    struct stat st;
    
    /* Create lock directory if it doesn't exist */
    if (stat(LOCK_DIR, &st) != 0) {
        if (mkdir(LOCK_DIR, 0755) != 0) {
            if (errno != EEXIST) {
                log_error("Failed to create lock directory");
                return -1;
            }
        }
    } else if (!S_ISDIR(st.st_mode)) {
        log_error("Lock path exists but is not a directory");
        return -1;
    }
    
    /* Ensure proper permissions on lock directory */
    if (chmod(LOCK_DIR, 0755) != 0) {
        log_error("Failed to set permissions on lock directory");
        return -1;
    }
    
    /* Clean up any stale locks from previous runs */
    cleanup_stale_locks();
    
    return 0;
}

/**
 * Cleanup file locking system
 */
void cleanup_file_locking(void) {
    /* Clean up any locks owned by this process */
    cleanup_stale_locks();
}

/**
 * Resolve canonical path for a file
 */
char *resolve_canonical_path(const char *file_path) {
    char *canonical_path = NULL;
    char *resolved_path = NULL;
    
    if (!file_path) {
        return NULL;
    }
    
    /* Use realpath to resolve symlinks and relative paths */
    resolved_path = realpath(file_path, NULL);
    if (resolved_path) {
        canonical_path = strdup(resolved_path);
        free(resolved_path);
    } else {
        /* If realpath fails (file doesn't exist yet), use absolute path */
        if (file_path[0] == '/') {
            canonical_path = strdup(file_path);
        } else {
            /* Convert relative path to absolute */
            char *cwd = getcwd(NULL, 0);
            if (cwd) {
                size_t path_len = strlen(cwd) + strlen(file_path) + 2;
                canonical_path = malloc(path_len);
                if (canonical_path) {
                    snprintf(canonical_path, path_len, "%s/%s", cwd, file_path);
                }
                free(cwd);
            }
        }
    }
    
    return canonical_path;
}

/**
 * Generate lock file path from canonical file path
 */
static char *generate_lock_file_path(const char *canonical_path) {
    if (!canonical_path) {
        return NULL;
    }
    
    /* Create a safe filename by replacing / with _ */
    size_t path_len = strlen(canonical_path);
    char *safe_name = malloc(path_len + 1);
    if (!safe_name) {
        return NULL;
    }
    
    for (size_t i = 0; i < path_len; i++) {
        if (canonical_path[i] == '/') {
            safe_name[i] = '_';
        } else {
            safe_name[i] = canonical_path[i];
        }
    }
    safe_name[path_len] = '\0';
    
    /* Create full lock file path */
    size_t lock_path_len = strlen(LOCK_DIR) + strlen(safe_name) + strlen(LOCK_FILE_EXTENSION) + 2;
    char *lock_file_path = malloc(lock_path_len);
    if (lock_file_path) {
        snprintf(lock_file_path, lock_path_len, "%s/%s%s", LOCK_DIR, safe_name, LOCK_FILE_EXTENSION);
    }
    
    free(safe_name);
    return lock_file_path;
}

/**
 * Write lock metadata to lock file
 */
static int write_lock_metadata(const char *lock_file_path, const char *canonical_path, 
                              const char *username, pid_t pid) {
    FILE *lock_file = fopen(lock_file_path, "w");
    if (!lock_file) {
        return -1;
    }
    
    time_t now = time(NULL);
    
    fprintf(lock_file, "file_path=%s\n", canonical_path);
    fprintf(lock_file, "username=%s\n", username);
    fprintf(lock_file, "pid=%d\n", (int)pid);
    fprintf(lock_file, "timestamp=%ld\n", (long)now);
    
    fclose(lock_file);
    return 0;
}

/**
 * Read lock metadata from lock file
 */
static struct file_lock_info *read_lock_metadata(const char *lock_file_path) {
    FILE *lock_file = fopen(lock_file_path, "r");
    if (!lock_file) {
        return NULL;
    }
    
    struct file_lock_info *lock_info = malloc(sizeof(struct file_lock_info));
    if (!lock_info) {
        fclose(lock_file);
        return NULL;
    }
    
    memset(lock_info, 0, sizeof(struct file_lock_info));
    lock_info->lock_file_path = strdup(lock_file_path);
    
    char line[512];
    while (fgets(line, sizeof(line), lock_file)) {
        /* Remove newline */
        line[strcspn(line, "\n")] = '\0';
        
        if (strncmp(line, "file_path=", 10) == 0) {
            lock_info->file_path = strdup(line + 10);
        } else if (strncmp(line, "username=", 9) == 0) {
            lock_info->username = strdup(line + 9);
        } else if (strncmp(line, "pid=", 4) == 0) {
            lock_info->pid = (pid_t)atoi(line + 4);
        } else if (strncmp(line, "timestamp=", 10) == 0) {
            lock_info->timestamp = (time_t)atol(line + 10);
        }
    }
    
    fclose(lock_file);
    
    /* Validate that we got all required fields */
    if (!lock_info->file_path || !lock_info->username || lock_info->pid <= 0) {
        free_file_lock_info(lock_info);
        return NULL;
    }
    
    return lock_info;
}

/**
 * Check if a process is still running
 */
static int is_process_running(pid_t pid) {
    if (pid <= 0) {
        return 0;
    }
    
    /* Use kill(pid, 0) to check if process exists */
    if (kill(pid, 0) == 0) {
        return 1;  /* Process exists */
    } else if (errno == ESRCH) {
        return 0;  /* Process doesn't exist */
    } else {
        /* Permission denied or other error - assume process exists */
        return 1;
    }
}

/**
 * Check if a lock is stale (process dead or timeout exceeded)
 */
static int is_lock_stale(struct file_lock_info *lock_info) {
    if (!lock_info) {
        return 1;
    }
    
    /* Check if process is still running */
    if (!is_process_running(lock_info->pid)) {
        return 1;
    }
    
    /* Check timeout */
    time_t now = time(NULL);
    if (now - lock_info->timestamp > LOCK_TIMEOUT) {
        return 1;
    }
    
    return 0;
}

/**
 * Free file lock info structure
 */
void free_file_lock_info(struct file_lock_info *lock_info) {
    if (!lock_info) {
        return;
    }
    
    free(lock_info->file_path);
    free(lock_info->username);
    free(lock_info->lock_file_path);
    free(lock_info);
}

/**
 * Acquire exclusive lock on a file
 */
int acquire_file_lock(const char *file_path, const char *username, pid_t pid) {
    char *canonical_path = NULL;
    char *lock_file_path = NULL;
    int lock_fd = -1;
    int result = -1;
    
    if (!file_path || !username || pid <= 0) {
        return -1;
    }
    
    /* Resolve canonical path */
    canonical_path = resolve_canonical_path(file_path);
    if (!canonical_path) {
        log_error("Failed to resolve canonical path for file locking");
        return -1;
    }
    
    /* Generate lock file path */
    lock_file_path = generate_lock_file_path(canonical_path);
    if (!lock_file_path) {
        free(canonical_path);
        return -1;
    }
    
    /* Check if file is already locked */
    struct file_lock_info *existing_lock = check_file_lock(canonical_path);
    if (existing_lock) {
        if (!is_lock_stale(existing_lock)) {
            /* File is actively locked by another user/process */
            char error_msg[1024];
            char time_str[64];
            struct tm *tm_info = localtime(&existing_lock->timestamp);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
            
            snprintf(error_msg, sizeof(error_msg),
                    "Error: %s is currently being edited by user '%s' since %s. Please try again later.",
                    canonical_path, existing_lock->username, time_str);
            
            fprintf(stderr, "sudosh: %s\n", error_msg);
            log_security_violation(username, error_msg);
            
            free_file_lock_info(existing_lock);
            free(canonical_path);
            free(lock_file_path);
            return -1;
        } else {
            /* Lock is stale, remove it */
            if (existing_lock->lock_file_path) {
                unlink(existing_lock->lock_file_path);
            }
            free_file_lock_info(existing_lock);
        }
    }
    
    /* Create lock file with exclusive access */
    lock_fd = open(lock_file_path, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (lock_fd == -1) {
        if (errno == EEXIST) {
            /* Race condition - another process created the lock */
            fprintf(stderr, "sudosh: File is being edited by another user\n");
        } else {
            perror("Failed to create lock file");
        }
        free(canonical_path);
        free(lock_file_path);
        return -1;
    }
    
    /* Apply advisory lock */
    if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
        close(lock_fd);
        unlink(lock_file_path);
        fprintf(stderr, "sudosh: Failed to acquire file lock\n");
        free(canonical_path);
        free(lock_file_path);
        return -1;
    }
    
    /* Write metadata to lock file */
    close(lock_fd);  /* Close fd, but keep file */
    
    if (write_lock_metadata(lock_file_path, canonical_path, username, pid) == 0) {
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "acquired file lock: %s", canonical_path);
        log_security_violation(username, log_msg);
        result = 0;
    } else {
        unlink(lock_file_path);
        result = -1;
    }
    
    free(canonical_path);
    free(lock_file_path);
    return result;
}

/**
 * Release file lock
 */
int release_file_lock(const char *file_path, const char *username, pid_t pid) {
    char *canonical_path = NULL;
    char *lock_file_path = NULL;
    int result = -1;

    if (!file_path || !username || pid <= 0) {
        return -1;
    }

    /* Resolve canonical path */
    canonical_path = resolve_canonical_path(file_path);
    if (!canonical_path) {
        return -1;
    }

    /* Generate lock file path */
    lock_file_path = generate_lock_file_path(canonical_path);
    if (!lock_file_path) {
        free(canonical_path);
        return -1;
    }

    /* Read lock metadata to verify ownership */
    struct file_lock_info *lock_info = read_lock_metadata(lock_file_path);
    if (lock_info) {
        /* Verify that this user/process owns the lock */
        if (strcmp(lock_info->username, username) == 0 && lock_info->pid == pid) {
            /* Remove lock file */
            if (unlink(lock_file_path) == 0) {
                char log_msg[512];
                snprintf(log_msg, sizeof(log_msg), "released file lock: %s", canonical_path);
                log_security_violation(username, log_msg);
                result = 0;
            }
        }
        free_file_lock_info(lock_info);
    }

    free(canonical_path);
    free(lock_file_path);
    return result;
}

/**
 * Check if file is locked
 */
struct file_lock_info *check_file_lock(const char *file_path) {
    char *canonical_path = NULL;
    char *lock_file_path = NULL;
    struct file_lock_info *lock_info = NULL;

    if (!file_path) {
        return NULL;
    }

    /* Resolve canonical path */
    canonical_path = resolve_canonical_path(file_path);
    if (!canonical_path) {
        return NULL;
    }

    /* Generate lock file path */
    lock_file_path = generate_lock_file_path(canonical_path);
    if (!lock_file_path) {
        free(canonical_path);
        return NULL;
    }

    /* Check if lock file exists */
    if (access(lock_file_path, F_OK) == 0) {
        lock_info = read_lock_metadata(lock_file_path);
        if (lock_info && is_lock_stale(lock_info)) {
            /* Lock is stale, remove it */
            unlink(lock_file_path);
            free_file_lock_info(lock_info);
            lock_info = NULL;
        }
    }

    free(canonical_path);
    free(lock_file_path);
    return lock_info;
}

/**
 * Clean up stale locks
 */
int cleanup_stale_locks(void) {
    DIR *lock_dir = opendir(LOCK_DIR);
    if (!lock_dir) {
        return -1;
    }

    struct dirent *entry;
    int cleaned_count = 0;

    while ((entry = readdir(lock_dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Check if it's a lock file */
        if (strstr(entry->d_name, LOCK_FILE_EXTENSION)) {
            char lock_file_path[MAX_LOCK_PATH_LENGTH];
            snprintf(lock_file_path, sizeof(lock_file_path), "%s/%s", LOCK_DIR, entry->d_name);

            struct file_lock_info *lock_info = read_lock_metadata(lock_file_path);
            if (lock_info) {
                if (is_lock_stale(lock_info)) {
                    if (unlink(lock_file_path) == 0) {
                        char log_msg[512];
                        snprintf(log_msg, sizeof(log_msg),
                                "cleaned up stale lock: %s (user: %s, pid: %d)",
                                lock_info->file_path, lock_info->username, lock_info->pid);
                        log_security_violation("system", log_msg);
                        cleaned_count++;
                    }
                }
                free_file_lock_info(lock_info);
            }
        }
    }

    closedir(lock_dir);
    return cleaned_count;
}

/**
 * Check if command is an editing command that requires file locking
 */
int is_editing_command(const char *command) {
    if (!command) return 0;

    /* Check if it's a secure editor */
    if (is_secure_editor(command)) {
        return 1;
    }

    /* Create a copy for parsing */
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;

    /* Get the first token (command name) */
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }

    /* List of additional editors that can modify files */
    const char *editors[] = {
        "emacs", "/bin/emacs", "/usr/bin/emacs", "/usr/local/bin/emacs",
        "joe", "/bin/joe", "/usr/bin/joe", "/usr/local/bin/joe",
        "mcedit", "/bin/mcedit", "/usr/bin/mcedit", "/usr/local/bin/mcedit",
        "ed", "/bin/ed", "/usr/bin/ed",
        "ex", "/bin/ex", "/usr/bin/ex",
        "sed", "/bin/sed", "/usr/bin/sed",  /* sed -i can modify files */
        NULL
    };

    /* Check against editor list */
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
 * Extract file argument from editing command
 */
char *extract_file_argument(const char *command) {
    if (!command) return NULL;

    char *cmd_copy = strdup(command);
    if (!cmd_copy) return NULL;

    char *token, *saveptr;
    char *file_arg = NULL;
    int arg_count = 0;
    int skip_next = 0;

    /* Tokenize command */
    token = strtok_r(cmd_copy, " \t", &saveptr);
    while (token != NULL) {
        arg_count++;

        /* Skip the command name */
        if (arg_count == 1) {
            token = strtok_r(NULL, " \t", &saveptr);
            continue;
        }

        /* Handle options that take arguments */
        if (skip_next) {
            skip_next = 0;
            token = strtok_r(NULL, " \t", &saveptr);
            continue;
        }

        /* Check for options that take arguments */
        if (strcmp(token, "-i") == 0 || strcmp(token, "-e") == 0 ||
            strcmp(token, "-f") == 0 || strcmp(token, "-n") == 0) {
            /* These options might take arguments, but for sed -i, the next token might be the file */
            if (strcmp(token, "-i") == 0) {
                /* For sed -i, check if next token starts with a quote or is a file */
                char *next_token = strtok_r(NULL, " \t", &saveptr);
                if (next_token && next_token[0] != '\'' && next_token[0] != '"' && next_token[0] != '-') {
                    /* This looks like a file, not an -i argument */
                    file_arg = strdup(next_token);
                    break;
                }
                /* Continue to look for file argument */
                token = next_token;
                continue;
            } else {
                skip_next = 1;
            }
        }
        /* Skip other options */
        else if (token[0] == '-') {
            /* Skip this option */
        }
        /* This looks like a file argument */
        else {
            file_arg = strdup(token);
            break;
        }

        token = strtok_r(NULL, " \t", &saveptr);
    }

    free(cmd_copy);
    return file_arg;
}
