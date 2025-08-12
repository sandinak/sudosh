/**
 * shell_enhancements.c - Shell Enhancement Features
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * This file implements bash/zsh-like shell enhancements for sudosh
 * including alias management, directory stack, environment variables,
 * and command information helpers - all with security validation.
 */

#include "sudosh.h"

/* Global variables for shell enhancements */
static struct alias_entry *alias_list = NULL;
static struct dir_stack_entry *dir_stack = NULL;
static int alias_system_initialized = 0;
static int dir_stack_initialized = 0;

/* External variables */
extern char *current_username;

/**
 * Initialize the alias system
 */
int init_alias_system(void) {
    if (alias_system_initialized) {
        return 1;
    }

    alias_list = NULL;
    alias_system_initialized = 1;
    /* Respect configuration flag if available */
    extern int rc_alias_import_enabled;


    /* Load aliases from file */
    load_aliases_from_file();

    /* Load aliases from user shell rc files if enabled */
    if (rc_alias_import_enabled) {
        load_aliases_from_shell_rc_files();
    }

    return 1;
}


/**
 * Cleanup the alias system
 */
void cleanup_alias_system(void) {
    if (!alias_system_initialized) {
        return;
    }

    /* Save aliases to file before cleanup */
    save_aliases_to_file();

    /* Free all aliases */
    struct alias_entry *current = alias_list;
    while (current) {
        struct alias_entry *next = current->next;
        free(current->name);
        free(current->value);
        free(current);
        current = next;
    }

    alias_list = NULL;
    alias_system_initialized = 0;
}

/**
 * Validate alias name for security
 */
int validate_alias_name(const char *name) {
    if (!name || strlen(name) == 0) {
        return 0;
    }

    /* Check length */
    if (strlen(name) >= MAX_ALIAS_NAME_LENGTH) {
        return 0;
    }

    /* Must start with letter or underscore */
    if (!isalpha(name[0]) && name[0] != '_') {
        return 0;
    }

    /* Can only contain alphanumeric characters and underscores */
    for (size_t i = 0; i < strlen(name); i++) {
        if (!isalnum(name[i]) && name[i] != '_') {
            return 0;
        }
    }

    /* Don't allow overriding built-in commands */
    const char *builtins[] = {
        "help", "commands", "history", "pwd", "path", "cd", "exit", "quit",
        "rules", "version", "alias", "unalias", "export", "unset", "env",
        "which", "type", "pushd", "popd", "dirs", NULL
    };

    for (int i = 0; builtins[i]; i++) {
        if (strcmp(name, builtins[i]) == 0) {
            return 0;
        }
    }

    return 1;
}

/**
 * Validate alias value for security
 */
int validate_alias_value(const char *value) {
    if (!value) {
        return 0;
    }

    /* Check length */
    if (strlen(value) >= MAX_ALIAS_VALUE_LENGTH) {
        return 0;
    }

    /* Check for dangerous patterns */
    const char *dangerous_patterns[] = {
        "$(", "`", "${", "\\x", "\\u", "\\U", "\\N",
        "LD_PRELOAD=", "PATH=", "SHELL=", "HOME=",
        ";", "&&", "||", "|", "&", ">", "<", ">>",
        NULL
    };

    for (int i = 0; dangerous_patterns[i]; i++) {
        if (strstr(value, dangerous_patterns[i])) {
            return 0;
        }
    }

    /* Check for null bytes */
    for (size_t i = 0; i < strlen(value); i++) {
        if (value[i] == '\0') {
            return 0;
        }
    }

    return 1;
}

/**
 * Add or update an alias
 */
int add_alias(const char *name, const char *value) {
    if (!alias_system_initialized) {
        init_alias_system();
    }

    if (!validate_alias_name(name) || !validate_alias_value(value)) {
        return 0;
    }
    /* Evaluate the alias value with the global security validator to ensure
       it cannot be used to bypass policy (e.g., shells, ssh, sudoedit, etc.) */
    if (!validate_command(value)) {
        return 0;
    }
    /* Explicitly block dangerous system operations as aliases */
    if (is_dangerous_system_operation(value) || is_dangerous_command(value)) {
        return 0;
    }

    /* Check if alias already exists */
    struct alias_entry *current = alias_list;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            /* Update existing alias */
            free(current->value);
            current->value = strdup(value);
            return current->value ? 1 : 0;
        }
        current = current->next;
    }

    /* Create new alias */
    struct alias_entry *new_alias = malloc(sizeof(struct alias_entry));
    if (!new_alias) {
        return 0;
    }

    new_alias->name = strdup(name);
    new_alias->value = strdup(value);
    new_alias->next = alias_list;

    if (!new_alias->name || !new_alias->value) {
        free(new_alias->name);
        free(new_alias->value);
        free(new_alias);
        return 0;
    }

    alias_list = new_alias;
    return 1;
}

/**
 * Remove an alias
 */
int remove_alias(const char *name) {
    if (!alias_system_initialized || !name) {
        return 0;
    }

    struct alias_entry *current = alias_list;
    struct alias_entry *prev = NULL;

    while (current) {
        if (strcmp(current->name, name) == 0) {
            /* Found the alias to remove */
            if (prev) {
                prev->next = current->next;
            } else {
                alias_list = current->next;
            }

            free(current->name);
            free(current->value);
            free(current);
            return 1;
        }
        prev = current;
        current = current->next;
    }

    return 0;  /* Alias not found */
}

/**
 * Get alias value
 */
char *get_alias_value(const char *name) {
    if (!alias_system_initialized || !name) {
        return NULL;
    }

    struct alias_entry *current = alias_list;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current->value;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Print all aliases
 */
void print_aliases(void) {
    if (!alias_system_initialized) {
        init_alias_system();
    }

    if (!alias_list) {
        printf("No aliases defined.\n");
        return;
    }

    printf("Defined aliases:\n");
    struct alias_entry *current = alias_list;
    while (current) {
        printf("alias %s='%s'\n", current->name, current->value);
        current = current->next;
    }
}

/* Iterate alias names matching a prefix; returns count found */
int alias_iterate_names_with_prefix(const char *prefix, int (*cb)(const char *name, void *), void *ctx) {
    if (!alias_system_initialized) {
        init_alias_system();
    }
    if (!cb) return 0;
    int count = 0;
    size_t plen = prefix ? strlen(prefix) : 0;
    for (struct alias_entry *cur = alias_list; cur; cur = cur->next) {
        if (!prefix || strncmp(cur->name, prefix, plen) == 0) {
            if (cb(cur->name, ctx)) {
                /* callback can signal to stop early by returning non-zero */
                return count;
            }
            count++;
        }
    }
    return count;
}

/**
 * Load aliases from file
 */
int load_aliases_from_file(void) {
    char *username = get_current_username();
    if (!username) {
        return 0;
    }

    struct passwd *pwd = getpwnam(username);
    if (!pwd) {
        free(username);
        return 0;
    }

    char alias_file_path[PATH_MAX];
    snprintf(alias_file_path, sizeof(alias_file_path), "%s/%s",
             pwd->pw_dir, ALIAS_FILE_NAME);

    FILE *file = fopen(alias_file_path, "r");
    if (!file) {
        free(username);
        return 0;  /* File doesn't exist, that's OK */
    }

    char line[MAX_ALIAS_VALUE_LENGTH + MAX_ALIAS_NAME_LENGTH + 10];
    while (fgets(line, sizeof(line), file)) {
        /* Remove newline */
        line[strcspn(line, "\n")] = '\0';

        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        /* Parse alias line: name=value */
        char *equals = strchr(line, '=');
        if (!equals) {
            continue;
        }

        *equals = '\0';
        char *name = line;
        char *value = equals + 1;

        /* Trim whitespace */
        name = trim_whitespace(name);
        value = trim_whitespace(value);

        if (validate_alias_name(name) && validate_alias_value(value)) {
            add_alias(name, value);
        }
    }

    fclose(file);
    free(username);
    return 1;
}

/**
 * Load aliases from shell rc files (.zshrc, .zshenv, .bashrc, .bash_profile)
 * Only simple alias lines like: alias ll='ls -la' or alias gs="git status" are accepted.
 * The files must be regular files owned by the user and not group/world writable.
 */
int load_aliases_from_shell_rc_files(void) {
    char *username = get_current_username();
    if (!username) {
        return 0;
    }
    struct passwd *pwd = getpwnam(username);
    if (!pwd) {
        free(username);
        return 0;
    }

    /* Determine base directory for rc files: in test mode, prefer $HOME */
    const char *base_dir = NULL;
    char *test_env = getenv("SUDOSH_TEST_MODE");
    if (test_env && strcmp(test_env, "1") == 0) {
        char *home_env = getenv("HOME");
        if (home_env && home_env[0] == '/') {
            base_dir = home_env;
        }
    }
    if (!base_dir) {
        base_dir = pwd->pw_dir;
    }

    const char *rc_files[] = {
        ".zshrc",
        ".zshenv",
        ".zprofile",
        ".bashrc",
        ".bash_profile",
        ".profile",
        NULL
    };

    int loaded_any = 0;

    for (int i = 0; rc_files[i]; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", base_dir, rc_files[i]);

        struct stat st;
        if (stat(path, &st) != 0) {
            continue; /* file not present */
        }

        /* Security checks: regular file, owned by user, not group/world writable */
        if (!S_ISREG(st.st_mode)) {
            continue;
        }
        if (st.st_uid != pwd->pw_uid) {
            continue;
        }
        if ((st.st_mode & S_IWGRP) || (st.st_mode & S_IWOTH)) {
            /* Skip insecure files */
            continue;
        }

        FILE *f = fopen(path, "r");
        if (!f) {
            continue;
        }

        char line[MAX_ALIAS_VALUE_LENGTH + MAX_ALIAS_NAME_LENGTH + 64];
        while (fgets(line, sizeof(line), f)) {
            /* Trim leading whitespace */
            char *p = line;
            while (*p && isspace((unsigned char)*p)) p++;

            /* Skip comments and empty */
            if (*p == '\0' || *p == '\n' || *p == '#') {
                continue;
            }

            /* Only accept lines starting with 'alias ' */
            if (strncmp(p, "alias ", 6) != 0) {
                continue;
            }
            p += 6;

            /* Now expect name='value' or name="value" (no embedded quotes of same type) */
            /* Extract name up to '=' */
            char *eq = strchr(p, '=');
            if (!eq) {
                continue;
            }
            *eq = '\0';
            char *name = trim_whitespace(p);

            char *val = eq + 1;
            val = trim_whitespace(val);

            /* Require quoted value to avoid parsing complex code */
            size_t vlen = strlen(val);
            if (vlen < 2) {
                continue;
            }
            char quote = val[0];
            if (!(quote == '"' || quote == '\'')) {
                continue; /* Only quoted values allowed */
            }
            if (val[vlen-1] != quote) {
                continue; /* mismatched quote */
            }
            /* Strip surrounding quotes */
            val[vlen-1] = '\0';
            val++;

            /* Additional sanitization using existing validators */
            if (validate_alias_name(name) && validate_alias_value(val)) {
                /* Ensure the value does not contain shell control operators */
                if (strstr(val, "$(") || strstr(val, "`")) {
                    continue;
                }
                /* Reject aliases that are dangerous or contain redirections/pipes */
                if (is_dangerous_command(val) || is_dangerous_system_operation(val) ||
                    strstr(val, ">") || strstr(val, "|") || strstr(val, "&&") || strstr(val, ";")) {
                    continue;
                }
                /* Add alias */
                if (add_alias(name, val)) {
                    loaded_any = 1;
                }
            }
        }
        fclose(f);
    }

    free(username);
    return loaded_any ? 1 : 0;
}

/**
 * Save aliases to file
 */
int save_aliases_to_file(void) {
    if (!alias_system_initialized || !alias_list) {
        return 1;  /* Nothing to save */
    }

    char *username = get_current_username();
    if (!username) {
        return 0;
    }

    struct passwd *pwd = getpwnam(username);
    if (!pwd) {
        free(username);
        return 0;
    }

    char alias_file_path[PATH_MAX];
    snprintf(alias_file_path, sizeof(alias_file_path), "%s/%s",
             pwd->pw_dir, ALIAS_FILE_NAME);

    FILE *file = fopen(alias_file_path, "w");
    if (!file) {
        free(username);
        return 0;
    }

    fprintf(file, "# Sudosh aliases - automatically generated\n");
    fprintf(file, "# Format: name=value\n\n");

    struct alias_entry *current = alias_list;
    while (current) {
        fprintf(file, "%s=%s\n", current->name, current->value);
        current = current->next;
    }

    fclose(file);
    free(username);
    return 1;
}

/**
 * Expand aliases in a command
 */
char *expand_aliases(const char *command) {
    if (!alias_system_initialized || !command) {
        return NULL;
    }

    /* Create a copy of the command to work with */
    char *command_copy = strdup(command);
    if (!command_copy) {
        return NULL;
    }

    /* Trim whitespace */
    command_copy = trim_whitespace(command_copy);

    /* Extract the first word (command name) */
    char *space = strchr(command_copy, ' ');
    char *first_word;
    char *rest_of_command = "";

    if (space) {
        *space = '\0';
        first_word = command_copy;
        rest_of_command = space + 1;
    } else {
        first_word = command_copy;
    }

    /* Check if first word is an alias */
    char *alias_value = get_alias_value(first_word);
    if (!alias_value) {
        /* No alias found, return original command */
        free(command_copy);
        return strdup(command);
    }

    /* Construct expanded command */
    size_t expanded_len = strlen(alias_value) + strlen(rest_of_command) + 2;
    char *expanded = malloc(expanded_len);
    if (!expanded) {
        free(command_copy);
        return NULL;
    }

    if (strlen(rest_of_command) > 0) {
        snprintf(expanded, expanded_len, "%s %s", alias_value, rest_of_command);
    } else {
        strcpy(expanded, alias_value);
    }

    free(command_copy);
    return expanded;
}

/**
 * Initialize directory stack
 */
int init_directory_stack(void) {
    if (dir_stack_initialized) {
        return 1;
    }

    dir_stack = NULL;
    dir_stack_initialized = 1;
    return 1;
}

/**
 * Cleanup directory stack
 */
void cleanup_directory_stack(void) {
    if (!dir_stack_initialized) {
        return;
    }

    struct dir_stack_entry *current = dir_stack;
    while (current) {
        struct dir_stack_entry *next = current->next;
        free(current->path);
        free(current);
        current = next;
    }

    dir_stack = NULL;
    dir_stack_initialized = 0;
}

/**
 * Push directory onto stack
 */
int pushd(const char *dir) {
    if (!dir_stack_initialized) {
        init_directory_stack();
    }

    /* Get current directory */
    char *current_dir = getcwd(NULL, 0);
    if (!current_dir) {
        return 0;
    }

    /* Validate and change to new directory */
    if (chdir(dir) != 0) {
        free(current_dir);
        return 0;
    }

    /* Count current stack depth */
    int depth = 0;
    struct dir_stack_entry *current = dir_stack;
    while (current) {
        depth++;
        current = current->next;
    }

    if (depth >= MAX_DIR_STACK_DEPTH) {
        /* Stack is full, remove oldest entry */
        struct dir_stack_entry *prev = NULL;
        current = dir_stack;
        while (current->next) {
            prev = current;
            current = current->next;
        }

        if (prev) {
            prev->next = NULL;
        } else {
            dir_stack = NULL;
        }

        free(current->path);
        free(current);
    }

    /* Add current directory to stack */
    struct dir_stack_entry *new_entry = malloc(sizeof(struct dir_stack_entry));
    if (!new_entry) {
        free(current_dir);
        return 0;
    }

    new_entry->path = current_dir;
    new_entry->next = dir_stack;
    dir_stack = new_entry;

    /* Print new directory */
    char *new_dir = getcwd(NULL, 0);
    if (new_dir) {
        printf("%s\n", new_dir);
        free(new_dir);
    }

    return 1;
}

/**
 * Pop directory from stack
 */
int popd(void) {
    if (!dir_stack_initialized || !dir_stack) {
        fprintf(stderr, "popd: directory stack empty\n");
        return 0;
    }

    struct dir_stack_entry *top = dir_stack;
    dir_stack = top->next;

    /* Change to the popped directory */
    if (chdir(top->path) != 0) {
        perror("popd");
        free(top->path);
        free(top);
        return 0;
    }

    printf("%s\n", top->path);

    free(top->path);
    free(top);
    return 1;
}

/**
 * Print directory stack
 */
void print_dirs(void) {
    if (!dir_stack_initialized) {
        init_directory_stack();
    }

    /* Print current directory first */
    char *current_dir = getcwd(NULL, 0);
    if (current_dir) {
        printf("%s", current_dir);
        free(current_dir);
    }

    /* Print stack entries */
    struct dir_stack_entry *current = dir_stack;
    while (current) {
        printf(" %s", current->path);
        current = current->next;
    }
    printf("\n");
}
