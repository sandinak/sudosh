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
 * Check for dangerous alias patterns that could be used for privilege escalation
 */
int check_dangerous_alias_patterns(const char *alias_name, const char *alias_value) {
    if (!alias_name || !alias_value) {
        return 1; /* Dangerous - invalid input */
    }

    /* Check for aliases that try to override critical commands */
    const char *critical_commands[] = {
        "sudo", "su", "passwd", "chown", "chmod", "chgrp",
        "mount", "umount", "systemctl", "service", "init",
        "reboot", "shutdown", "halt", "poweroff",
        "iptables", "firewall-cmd", "ufw",
        "crontab", "at", "batch",
        "ssh", "scp", "rsync", "wget", "curl",
        NULL
    };

    for (int i = 0; critical_commands[i]; i++) {
        if (strcmp(alias_name, critical_commands[i]) == 0) {
            return 1; /* Dangerous - trying to override critical command */
        }
    }

    /* Check for aliases that try to execute privileged commands */
    for (int i = 0; critical_commands[i]; i++) {
        if (strstr(alias_value, critical_commands[i])) {
            /* Check if it's a word boundary */
            char *cmd_in_value = strstr(alias_value, critical_commands[i]);
            if ((cmd_in_value == alias_value || !isalnum(*(cmd_in_value - 1))) &&
                (!isalnum(*(cmd_in_value + strlen(critical_commands[i]))))) {
                return 1; /* Dangerous - contains privileged command */
            }
        }
    }

    /* Check for aliases that try to modify the environment in dangerous ways */
    const char *dangerous_env_patterns[] = {
        "PATH=", "LD_PRELOAD=", "LD_LIBRARY_PATH=", "SHELL=",
        "HOME=", "USER=", "LOGNAME=", "SUDO_",
        "export PATH", "export LD_PRELOAD", "export LD_LIBRARY_PATH",
        NULL
    };

    for (int i = 0; dangerous_env_patterns[i]; i++) {
        if (strstr(alias_value, dangerous_env_patterns[i])) {
            return 1; /* Dangerous - environment manipulation */
        }
    }

    /* Check for aliases that try to execute code from unusual locations */
    if (strstr(alias_value, "/tmp/") || strstr(alias_value, "/var/tmp/") ||
        strstr(alias_value, "/dev/shm/") || strstr(alias_value, "~/.")) {
        return 1; /* Dangerous - execution from temporary or hidden locations */
    }

    return 0; /* Safe */
}

/**
 * Validate alias expansion safety during alias creation
 */
int validate_alias_expansion_safety(const char *alias_name, const char *alias_value) {
    if (!alias_name || !alias_value) {
        return 0;
    }

    /* Check for self-referential aliases */
    if (strcmp(alias_name, alias_value) == 0) {
        return 0;
    }

    /* Check for recursive references in the alias value */
    if (strstr(alias_value, alias_name)) {
        /* The alias value contains the alias name - check if it's a word boundary */
        char *alias_in_value = strstr(alias_value, alias_name);

        if ((alias_in_value == alias_value || !isalnum(*(alias_in_value - 1))) &&
            (!isalnum(*(alias_in_value + strlen(alias_name))))) {
            /* This would create a recursive alias */
            return 0;
        }
    }

    /* Test expansion with a dummy argument to see if it would be safe */
    char test_command[MAX_ALIAS_VALUE_LENGTH + 64];
    snprintf(test_command, sizeof(test_command), "%s test_arg", alias_name);

    /* Temporarily add the alias to test expansion */
    struct alias_entry temp_alias;
    temp_alias.name = (char *)alias_name;
    temp_alias.value = (char *)alias_value;
    temp_alias.next = alias_list;

    /* Temporarily modify the alias list */
    struct alias_entry *old_head = alias_list;
    alias_list = &temp_alias;

    /* Test the expansion */
    char *expanded = expand_aliases_internal(test_command);

    /* Restore the original alias list */
    alias_list = old_head;

    if (!expanded) {
        return 0;
    }

    /* Validate the expanded result */
    int is_safe = validate_command(expanded);

    /* Additional checks for the expanded command */
    if (is_safe) {
        /* Check that expansion doesn't introduce dangerous patterns */
        if (strchr(expanded, ';') || strchr(expanded, '&') ||
            strstr(expanded, "&&") || strstr(expanded, "||") ||
            strchr(expanded, '`') || strstr(expanded, "$(")) {
            is_safe = 0;
        }

        /* Check for dangerous commands in the expansion */
        char *expanded_copy = strdup(expanded);
        if (expanded_copy) {
            char *first_cmd = strtok(expanded_copy, " \t");
            if (first_cmd && (is_dangerous_command(first_cmd) || is_dangerous_system_operation(first_cmd))) {
                is_safe = 0;
            }
            free(expanded_copy);
        }
    }

    free(expanded);
    return is_safe;
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

    /* SECURITY ENHANCEMENT: Check for dangerous alias patterns */
    if (check_dangerous_alias_patterns(name, value)) {
        return 0;
    }

    /* SECURITY ENHANCEMENT: Test the alias expansion to ensure it's safe */
    if (!validate_alias_expansion_safety(name, value)) {
        return 0;
    }

    /* Check if alias already exists */
    struct alias_entry *current = alias_list;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            /* Update existing alias */
            free(current->value);
            current->value = safe_strdup(value);
            return current->value ? 1 : 0;
        }
        current = current->next;
    }

    /* Create new alias */
    struct alias_entry *new_alias = malloc(sizeof(struct alias_entry));
    if (!new_alias) {
        return 0;
    }

    new_alias->name = safe_strdup(name);
    new_alias->value = safe_strdup(value);
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
                /* SECURITY ENHANCEMENT: Check for dangerous patterns */
                if (check_dangerous_alias_patterns(name, val)) {
                    continue;
                }
                /* SECURITY ENHANCEMENT: Additional validation for expansion safety */
                if (!validate_alias_expansion_safety(name, val)) {
                    continue;
                }
                /* Respect first-seen precedence: if alias already exists, skip override */
                if (get_alias_value(name) != NULL) {
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
 * Internal alias expansion without security validation (for testing during alias creation)
 */
char *expand_aliases_internal(const char *command) {
    if (!alias_system_initialized || !command) {
        return NULL;
    }

    /* Create a copy of the command to work with */
    char *command_copy = safe_strdup(command);
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
        return safe_strdup(command);
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
        snprintf(expanded, expanded_len, "%s", alias_value);
    }

    free(command_copy);
    return expanded;
}

/**
 * Expand aliases in a command with security validation
 */
char *expand_aliases(const char *command) {
    if (!alias_system_initialized || !command) {
        return NULL;
    }

    /* Create a copy of the command to work with */
    char *command_copy = safe_strdup(command);
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
        return safe_strdup(command);
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
        snprintf(expanded, expanded_len, "%s", alias_value);
    }

    /* SECURITY ENHANCEMENT: Validate the expanded command before returning it */
    if (!validate_expanded_alias_command(expanded, first_word, alias_value)) {
        fprintf(stderr, "sudosh: alias expansion '%s' -> '%s' failed security validation\n",
                first_word, expanded);

        /* Log the security violation with details */
        char violation_msg[512];
        char *username = get_current_username();
        snprintf(violation_msg, sizeof(violation_msg),
                "dangerous alias expansion blocked: %s -> %s", first_word, expanded);
        log_security_violation(username, violation_msg);
        free(username);

        free(expanded);
        free(command_copy);
        return NULL;
    }

    /* Log successful alias expansion for audit trail */
    if (strcmp(command, expanded) != 0) {
        char audit_msg[512];
        char *username = get_current_username();
        snprintf(audit_msg, sizeof(audit_msg),
                "alias expanded: %s -> %s", first_word, alias_value);
        syslog(LOG_INFO, "ALIAS_EXPANSION: user=%s %s",
               username ? username : "unknown", audit_msg);
        free(username);
    }

    free(command_copy);
    return expanded;
}

/**
 * Validate expanded alias command for security
 */
int validate_expanded_alias_command(const char *expanded_command, const char *alias_name, const char *alias_value) {
    if (!expanded_command || !alias_name || !alias_value) {
        return 0;
    }

    /* First, validate the expanded command using the standard security validator */
    if (!validate_command(expanded_command)) {
        return 0;
    }

    /* Check for alias expansion loops (alias pointing to itself) */
    if (strcmp(alias_name, alias_value) == 0) {
        return 0;
    }

    /* Check for recursive alias expansion that could lead to infinite loops */
    if (strstr(alias_value, alias_name)) {
        /* The alias value contains the alias name - potential recursion */
        char *alias_in_value = strstr(alias_value, alias_name);

        /* Check if it's a word boundary (not part of another word) */
        if ((alias_in_value == alias_value || !isalnum(*(alias_in_value - 1))) &&
            (!isalnum(*(alias_in_value + strlen(alias_name))))) {
            return 0;
        }
    }

    /* Extract the first command from the expanded result for additional validation */
    char *expanded_copy = safe_strdup(expanded_command);
    if (!expanded_copy) {
        return 0;
    }

    char *first_cmd = strtok(expanded_copy, " \t");
    if (first_cmd) {
        /* Check if the expanded command is trying to execute dangerous commands */
        if (is_dangerous_command(first_cmd) || is_dangerous_system_operation(first_cmd)) {
            free(expanded_copy);
            return 0;
        }

        /* Check if the expanded command contains shell metacharacters that weren't in the original alias */
        if (strchr(expanded_command, ';') || strchr(expanded_command, '&') ||
            strstr(expanded_command, "&&") || strstr(expanded_command, "||") ||
            strchr(expanded_command, '`') || strstr(expanded_command, "$(")) {
            free(expanded_copy);
            return 0;
        }

        /* Check for redirection that wasn't validated during alias creation */
        if (strchr(expanded_command, '>') || strchr(expanded_command, '<')) {
            if (!validate_safe_redirection(expanded_command)) {
                free(expanded_copy);
                return 0;
            }
        }

        /* Check for pipeline commands */
        if (strchr(expanded_command, '|')) {
            if (!validate_secure_pipeline(expanded_command)) {
                free(expanded_copy);
                return 0;
            }
        }
    }

    free(expanded_copy);
    return 1;
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
