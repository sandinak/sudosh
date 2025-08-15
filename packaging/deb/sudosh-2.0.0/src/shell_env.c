/**
 * shell_env.c - Environment Variable and Command Information Functions
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * This file implements environment variable management and command
 * information helpers for sudosh with security validation.
 */

#include "sudosh.h"
#include "sudosh_common.h"

/* External variables */
extern char **environ;

/**
 * List of safe environment variables that can be modified
 */
static const char *safe_env_vars[] = {
    "EDITOR", "VISUAL", "PAGER", "LESS", "MORE",
    "LANG", "LC_ALL", "LC_CTYPE", "LC_COLLATE", "LC_MESSAGES",
    "LC_MONETARY", "LC_NUMERIC", "LC_TIME",
    "TZ", "TERM", "COLUMNS", "LINES",
    "PS1", "PS2", "PS3", "PS4",
    "HISTSIZE", "HISTFILE", "HISTCONTROL",
    "MANPATH", "MANPAGER",
    NULL
};

/**
 * List of dangerous environment variables that should never be modified
 */
static const char *dangerous_env_vars[] = {
    "PATH", "LD_PRELOAD", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH",
    "SHELL", "HOME", "USER", "LOGNAME", "USERNAME",
    "SUDO_USER", "SUDO_UID", "SUDO_GID", "SUDO_COMMAND",
    "IFS", "CDPATH", "ENV", "BASH_ENV",
    "PYTHONPATH", "PERL5LIB", "RUBYLIB",
    NULL
};

/**
 * Validate environment variable name
 */
int validate_env_var_name(const char *name) {
    if (!name || strlen(name) == 0) {
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
    
    return 1;
}

/**
 * Check if environment variable is safe to modify
 */
int is_safe_env_var(const char *name) {
    if (!name) {
        return 0;
    }
    
    /* Check if it's in the dangerous list */
    for (int i = 0; dangerous_env_vars[i]; i++) {
        if (strcmp(name, dangerous_env_vars[i]) == 0) {
            return 0;
        }
    }
    
    /* Check if it's in the safe list */
    for (int i = 0; safe_env_vars[i]; i++) {
        if (strcmp(name, safe_env_vars[i]) == 0) {
            return 1;
        }
    }
    
    /* For variables not in either list, be conservative */
    return 0;
}

/**
 * Handle export command
 */
int handle_export_command(const char *command) {
    if (!command) {
        return 0;
    }
    
    /* Skip "export" and whitespace */
    const char *args = command + 6;  /* strlen("export") */
    while (isspace(*args)) {
        args++;
    }
    
    /* If no arguments, print all exported variables */
    if (*args == '\0') {
        printf("Exported variables:\n");
        for (char **env = environ; *env; env++) {
            printf("export %s\n", *env);
        }
        return 1;
    }
    
    /* Parse variable assignment */
    char *args_copy = strdup(args);
    if (!args_copy) {
        return 0;
    }
    
    char *equals = strchr(args_copy, '=');
    if (!equals) {
        /* Just export existing variable */
        char *name = trim_whitespace(args_copy);
        if (validate_env_var_name(name) && is_safe_env_var(name)) {
            char *value = getenv(name);
            if (value) {
                printf("export %s=%s\n", name, value);
            } else {
                printf("export %s\n", name);
            }
            free(args_copy);
            return 1;
        } else {
            fprintf(stderr, "export: '%s' is not a safe environment variable\n", name);
            free(args_copy);
            return 0;
        }
    }
    
    /* Variable assignment */
    *equals = '\0';
    char *name = trim_whitespace(args_copy);
    char *value = trim_whitespace(equals + 1);
    
    /* Remove quotes from value if present */
    if ((value[0] == '"' && value[strlen(value)-1] == '"') ||
        (value[0] == '\'' && value[strlen(value)-1] == '\'')) {
        value[strlen(value)-1] = '\0';
        value++;
    }
    
    if (!validate_env_var_name(name)) {
        fprintf(stderr, "export: '%s' is not a valid variable name\n", name);
        free(args_copy);
        return 0;
    }
    
    if (!is_safe_env_var(name)) {
        fprintf(stderr, "export: '%s' is not a safe environment variable\n", name);
        free(args_copy);
        return 0;
    }
    
    /* Set the environment variable */
    if (setenv(name, value, 1) == 0) {
        printf("export %s=%s\n", name, value);
        free(args_copy);
        return 1;
    } else {
        perror("export");
        free(args_copy);
        return 0;
    }
}

/**
 * Handle unset command
 */
int handle_unset_command(const char *command) {
    if (!command) {
        return 0;
    }
    
    /* Skip "unset" and whitespace */
    const char *args = command + 5;  /* strlen("unset") */
    while (isspace(*args)) {
        args++;
    }
    
    if (*args == '\0') {
        fprintf(stderr, "unset: usage: unset name [name ...]\n");
        return 0;
    }
    
    /* Parse variable names */
    char *args_copy = strdup(args);
    if (!args_copy) {
        return 0;
    }
    
    char *token = strtok(args_copy, " \t");
    int success = 1;
    
    while (token) {
        token = trim_whitespace(token);
        
        if (!validate_env_var_name(token)) {
            fprintf(stderr, "unset: '%s' is not a valid variable name\n", token);
            success = 0;
        } else if (!is_safe_env_var(token)) {
            fprintf(stderr, "unset: '%s' is not a safe environment variable\n", token);
            success = 0;
        } else {
            if (unsetenv(token) != 0) {
                perror("unset");
                success = 0;
            }
        }
        
        token = strtok(NULL, " \t");
    }
    
    free(args_copy);
    return success;
}

/**
 * Print environment variables
 */
void print_environment(void) {
    printf("Environment variables:\n");
    for (char **env = environ; *env; env++) {
        printf("%s\n", *env);
    }
}

/**
 * Handle which command
 */
int handle_which_command(const char *command) {
    if (!command) {
        return 0;
    }
    
    /* Skip "which" and whitespace */
    const char *args = command + 5;  /* strlen("which") */
    while (isspace(*args)) {
        args++;
    }
    
    if (*args == '\0') {
        fprintf(stderr, "which: usage: which command [command ...]\n");
        return 0;
    }
    
    /* Parse command names */
    char *args_copy = strdup(args);
    if (!args_copy) {
        return 0;
    }
    
    char *token = strtok(args_copy, " \t");
    int found_all = 1;
    
    while (token) {
        token = trim_whitespace(token);
        
        /* Check if it's a built-in command */
        const char *builtins[] = {
            "help", "commands", "history", "pwd", "path", "cd", "exit", "quit",
            "rules", "version", "alias", "unalias", "export", "unset", "env",
            "which", "type", "pushd", "popd", "dirs", NULL
        };
        
        int is_builtin = 0;
        for (int i = 0; builtins[i]; i++) {
            if (strcmp(token, builtins[i]) == 0) {
                printf("%s: shell builtin\n", token);
                is_builtin = 1;
                break;
            }
        }
        
        if (!is_builtin) {
            /* Find command in PATH */
            char *path = find_command_in_path(token);
            if (path) {
                printf("%s\n", path);
                free(path);
            } else {
                printf("%s: not found\n", token);
                found_all = 0;
            }
        }
        
        token = strtok(NULL, " \t");
    }
    
    free(args_copy);
    return found_all;
}

/**
 * Handle type command
 */
int handle_type_command(const char *command) {
    if (!command) {
        return 0;
    }
    
    /* Skip "type" and whitespace */
    const char *args = command + 4;  /* strlen("type") */
    while (isspace(*args)) {
        args++;
    }
    
    if (*args == '\0') {
        fprintf(stderr, "type: usage: type command [command ...]\n");
        return 0;
    }
    
    /* Parse command names */
    char *args_copy = strdup(args);
    if (!args_copy) {
        return 0;
    }
    
    char *token = strtok(args_copy, " \t");
    int found_all = 1;
    
    while (token) {
        token = trim_whitespace(token);
        
        char *type_info = find_command_type(token);
        if (type_info) {
            printf("%s\n", type_info);
            free(type_info);
        } else {
            printf("%s: not found\n", token);
            found_all = 0;
        }
        
        token = strtok(NULL, " \t");
    }
    
    free(args_copy);
    return found_all;
}

/**
 * Find command type information
 */
char *find_command_type(const char *command) {
    if (!command) {
        return NULL;
    }
    
    /* Check if it's a built-in command */
    const char *builtins[] = {
        "help", "commands", "history", "pwd", "path", "cd", "exit", "quit",
        "rules", "version", "alias", "unalias", "export", "unset", "env",
        "which", "type", "pushd", "popd", "dirs", NULL
    };
    
    for (int i = 0; builtins[i]; i++) {
        if (strcmp(command, builtins[i]) == 0) {
            char *result = malloc(strlen(command) + 20);
            if (result) {
                sprintf(result, "%s is a shell builtin", command);
            }
            return result;
        }
    }
    
    /* Check if it's an alias */
    char *alias_value = get_alias_value(command);
    if (alias_value) {
        char *result = malloc(strlen(command) + strlen(alias_value) + 30);
        if (result) {
            sprintf(result, "%s is aliased to `%s'", command, alias_value);
        }
        return result;
    }
    
    /* Find command in PATH */
    char *path = find_command_in_path(command);
    if (path) {
        char *result = malloc(strlen(command) + strlen(path) + 10);
        if (result) {
            sprintf(result, "%s is %s", command, path);
        }
        free(path);
        return result;
    }
    
    return NULL;
}
