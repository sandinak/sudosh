/**
 * sudoers.c - Sudoers File Parsing and Validation
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Handles parsing and validation of sudoers files including
 * #includedir directives and permission checking.
 */

#include "sudosh.h"
#include "sudosh_common.h"

/* System includes */
#include <dirent.h>
#include <fcntl.h>

/* Default sudoers file path */
#define SUDOERS_PATH "/etc/sudoers"
#define SUDOERS_DIR "/etc/sudoers.d"

/**
 * Create a new userspec entry
 */
static struct sudoers_userspec *create_userspec(void) {
    struct sudoers_userspec *spec = malloc(sizeof(struct sudoers_userspec));
    if (!spec) {
        return NULL;
    }

    spec->users = NULL;
    spec->hosts = NULL;
    spec->commands = NULL;
    spec->nopasswd = 0;
    spec->runas_user = strdup("root");  /* Default runas user */
    spec->source_file = NULL;
    spec->next = NULL;

    return spec;
}

/**
 * Free a userspec entry
 */
static void free_userspec(struct sudoers_userspec *spec) {
    if (!spec) {
        return;
    }
    
    /* Free string arrays */
    if (spec->users) {
        for (int i = 0; spec->users[i]; i++) {
            free(spec->users[i]);
        }
        free(spec->users);
    }
    
    if (spec->hosts) {
        for (int i = 0; spec->hosts[i]; i++) {
            free(spec->hosts[i]);
        }
        free(spec->hosts);
    }
    
    if (spec->commands) {
        for (int i = 0; spec->commands[i]; i++) {
            free(spec->commands[i]);
        }
        free(spec->commands);
    }
    
    free(spec->runas_user);
    free(spec->source_file);
    free(spec);
}

/**
 * Parse a comma-separated list into string array
 */
static char **parse_list(const char *list_str) {
    if (!list_str) {
        return NULL;
    }
    
    char *str_copy = strdup(list_str);
    if (!str_copy) {
        return NULL;
    }
    
    /* Count items */
    int count = 1;
    for (char *p = str_copy; *p; p++) {
        if (*p == ',') {
            count++;
        }
    }
    
    /* Allocate array */
    char **array = malloc((count + 1) * sizeof(char *));
    if (!array) {
        free(str_copy);
        return NULL;
    }
    
    /* Parse items */
    char *token;
    char *saveptr;
    int i = 0;
    
    token = strtok_r(str_copy, ",", &saveptr);
    while (token && i < count) {
        /* Trim whitespace */
        while (*token && isspace(*token)) {
            token++;
        }
        char *end = token + strlen(token) - 1;
        while (end > token && isspace(*end)) {
            *end-- = '\0';
        }
        
        array[i] = strdup(token);
        if (!array[i]) {
            /* Cleanup on failure */
            for (int j = 0; j < i; j++) {
                free(array[j]);
            }
            free(array);
            free(str_copy);
            return NULL;
        }
        
        i++;
        token = strtok_r(NULL, ",", &saveptr);
    }
    
    array[i] = NULL;  /* Null terminate */
    free(str_copy);
    return array;
}

/**
 * Parse a simple sudoers line
 * Format: user host = (runas_user) command
 * Example: %wheel ALL = (ALL) ALL
 */
static struct sudoers_userspec *parse_sudoers_line(const char *line, const char *source_file) {
    char *line_copy = strdup(line);
    if (!line_copy) {
        return NULL;
    }
    
    /* Skip leading whitespace */
    char *p = line_copy;
    while (*p && isspace(*p)) {
        p++;
    }
    
    /* Skip comments and empty lines */
    if (*p == '#' || *p == '\0') {
        free(line_copy);
        return NULL;
    }
    
    /* Skip Defaults lines for now */
    if (strncmp(p, "Defaults", 8) == 0) {
        free(line_copy);
        return NULL;
    }
    
    struct sudoers_userspec *spec = create_userspec();
    if (!spec) {
        free(line_copy);
        return NULL;
    }
    
    /* Find the '=' separator */
    char *equals = strchr(p, '=');
    if (!equals) {
        free_userspec(spec);
        free(line_copy);
        return NULL;
    }
    
    *equals = '\0';
    char *left_side = p;
    char *right_side = equals + 1;
    
    /* Parse left side: user host */
    /* Trim whitespace from left side first */
    while (*left_side && isspace(*left_side)) {
        left_side++;
    }
    char *end = left_side + strlen(left_side) - 1;
    while (end > left_side && isspace(*end)) {
        *end-- = '\0';
    }

    /* Find the first whitespace to separate user from host */
    char *whitespace = NULL;
    for (char *p = left_side; *p; p++) {
        if (isspace(*p)) {
            whitespace = p;
            break;  /* Use first whitespace, not last */
        }
    }

    if (whitespace) {
        *whitespace = '\0';
        spec->users = parse_list(left_side);

        /* Skip multiple whitespace characters */
        char *host_start = whitespace + 1;
        while (*host_start && isspace(*host_start)) {
            host_start++;
        }
        spec->hosts = parse_list(host_start);
    } else {
        /* No whitespace found, assume user only */
        spec->users = parse_list(left_side);
        spec->hosts = parse_list("ALL");
    }
    
    /* Parse right side: (runas_user) command */
    while (*right_side && isspace(*right_side)) {
        right_side++;
    }

    /* Parse runas user first */
    if (*right_side == '(') {
        char *close_paren = strchr(right_side, ')');
        if (close_paren) {
            *close_paren = '\0';
            free(spec->runas_user);
            spec->runas_user = strdup(right_side + 1);
            right_side = close_paren + 1;
            while (*right_side && isspace(*right_side)) {
                right_side++;
            }
        }
    }

    /* Check for NOPASSWD: after runas user */
    if (strncmp(right_side, "NOPASSWD:", 9) == 0) {
        spec->nopasswd = 1;
        right_side += 9;
        while (*right_side && isspace(*right_side)) {
            right_side++;
        }
    }

    /* Parse commands */
    spec->commands = parse_list(right_side);

    /* Set source file */
    if (source_file) {
        spec->source_file = strdup(source_file);
    }

    free(line_copy);
    return spec;
}

/**
 * Temporarily escalate privileges to read sudoers file
 * Returns 1 if escalation was needed and successful, 0 if not needed, -1 on error
 */
static int escalate_for_sudoers_read(uid_t *saved_euid) {
    uid_t real_uid = get_real_uid();
    uid_t effective_uid = geteuid();

    /* If we're already running as root, no escalation needed */
    if (effective_uid == 0) {
        return 0;
    }

    /* If we're not setuid root, we can't escalate */
    if (real_uid == effective_uid) {
        return -1;
    }

    /* Save current effective UID and escalate to root */
    *saved_euid = effective_uid;
    if (seteuid(0) != 0) {
        return -1;
    }

    return 1;
}

/**
 * Drop privileges back after reading sudoers file
 */
static void drop_after_sudoers_read(int escalated, uid_t saved_euid) {
    if (escalated == 1) {
        /* Restore original effective UID */
        seteuid(saved_euid);
    }
}

/**
 * Check if filename is valid for sudoers include
 * Valid files must not contain '.' or '~' and must be regular files
 */
static int is_valid_sudoers_file(const char *filename) {
    /* Skip files with dots (except at start for hidden files) */
    if (strchr(filename, '.') != NULL) {
        return 0;
    }

    /* Skip files with tildes (backup files) */
    if (strchr(filename, '~') != NULL) {
        return 0;
    }

    /* Skip files starting with '#' (comments) */
    if (filename[0] == '#') {
        return 0;
    }

    return 1;
}

/**
 * Parse all files in an include directory
 */
static void parse_sudoers_directory(const char *dirname, struct sudoers_config *config, struct sudoers_userspec **last_spec) {
    DIR *dir;
    struct dirent *entry;
    char filepath[PATH_MAX];
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    uid_t saved_euid = geteuid();  /* Initialize to current euid */
    int escalated;

    if (!dirname || !config) {
        return;
    }

    /* Temporarily escalate privileges to read sudoers directory */
    escalated = escalate_for_sudoers_read(&saved_euid);

    dir = opendir(dirname);
    if (!dir) {
        /* Drop privileges and return */
        drop_after_sudoers_read(escalated, saved_euid);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Check if filename is valid for sudoers include */
        if (!is_valid_sudoers_file(entry->d_name)) {
            continue;
        }

        /* Build full file path */
        snprintf(filepath, sizeof(filepath), "%s/%s", dirname, entry->d_name);

        /* Try to open the file */
        fp = fopen(filepath, "r");
        if (!fp) {
            continue;  /* Skip files we can't read */
        }

        /* Parse the file line by line */
        while ((read = getline(&line, &len, fp)) != -1) {
            /* Remove newline */
            if (read > 0 && line[read - 1] == '\n') {
                line[read - 1] = '\0';
            }

            struct sudoers_userspec *spec = parse_sudoers_line(line, filepath);
            if (spec) {
                if (!config->userspecs) {
                    config->userspecs = spec;
                    *last_spec = spec;
                } else {
                    (*last_spec)->next = spec;
                    *last_spec = spec;
                }
            }
        }

        fclose(fp);
    }

    if (line) {
        free(line);
    }
    closedir(dir);

    /* Drop privileges back to original level */
    drop_after_sudoers_read(escalated, saved_euid);
}



/**
 * Parse sudoers file
 */
struct sudoers_config *parse_sudoers_file(const char *filename) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    struct sudoers_config *config;
    struct sudoers_userspec *last_spec = NULL;
    uid_t saved_euid;
    int escalated;

    if (!filename) {
        filename = SUDOERS_PATH;
    }

    config = malloc(sizeof(struct sudoers_config));
    if (!config) {
        return NULL;
    }

    config->userspecs = NULL;
    config->includedir = strdup(SUDOERS_DIR);

    /* Also parse the default sudoers.d directory if it exists */
    parse_sudoers_directory(SUDOERS_DIR, config, &last_spec);

    /* Temporarily escalate privileges to read sudoers file */
    escalated = escalate_for_sudoers_read(&saved_euid);

    fp = fopen(filename, "r");
    if (!fp) {
        /* Drop privileges before returning */
        drop_after_sudoers_read(escalated, saved_euid);
        /* If we can't read sudoers, return empty config */
        return config;
    }
    
    while ((read = getline(&line, &len, fp)) != -1) {
        /* Remove newline */
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }

        /* Check for #includedir directive */
        char *trimmed = line;
        while (*trimmed && isspace(*trimmed)) {
            trimmed++;
        }

        if (strncmp(trimmed, "#includedir", 11) == 0) {
            /* Parse includedir directive */
            char *dir_path = trimmed + 11;
            while (*dir_path && isspace(*dir_path)) {
                dir_path++;
            }

            if (*dir_path) {
                /* Update the includedir in config and parse the directory */
                free(config->includedir);
                config->includedir = strdup(dir_path);
                parse_sudoers_directory(dir_path, config, &last_spec);
            }
            continue;
        }

        struct sudoers_userspec *spec = parse_sudoers_line(line, filename);
        if (spec) {
            if (!config->userspecs) {
                config->userspecs = spec;
                last_spec = spec;
            } else {
                last_spec->next = spec;
                last_spec = spec;
            }
        }
    }
    
    free(line);
    fclose(fp);

    /* Drop privileges back to original level */
    drop_after_sudoers_read(escalated, saved_euid);

    return config;
}

/**
 * Free sudoers configuration
 */
void free_sudoers_config(struct sudoers_config *config) {
    if (!config) {
        return;
    }
    
    struct sudoers_userspec *spec = config->userspecs;
    while (spec) {
        struct sudoers_userspec *next = spec->next;
        free_userspec(spec);
        spec = next;
    }
    
    free(config->includedir);
    free(config);
}

/**
 * Check if string matches pattern (simple wildcard support)
 */
static int match_pattern(const char *pattern, const char *string) {
    if (!pattern || !string) {
        return 0;
    }
    
    if (strcmp(pattern, "ALL") == 0) {
        return 1;
    }
    
    return strcmp(pattern, string) == 0;
}

/**
 * Check if user matches userspec
 */
static int user_matches_spec(const char *username, struct sudoers_userspec *spec) {
    if (!username || !spec || !spec->users) {
        return 0;
    }
    
    for (int i = 0; spec->users[i]; i++) {
        if (match_pattern(spec->users[i], username)) {
            return 1;
        }
        
        /* Check for group membership (groups start with %) */
        if (spec->users[i][0] == '%') {
            struct group *grp = getgrnam(spec->users[i] + 1);
            if (grp && grp->gr_mem) {
                for (char **member = grp->gr_mem; *member; member++) {
                    if (strcmp(*member, username) == 0) {
                        return 1;
                    }
                }
            }
        }
    }
    
    return 0;
}

/**
 * Check sudoers privileges for user
 */
int check_sudoers_privileges(const char *username, const char *hostname, struct sudoers_config *sudoers) {
    if (!username || !sudoers) {
        return 0;
    }

    if (!hostname) {
        hostname = "localhost";  /* Default hostname */
    }

    struct sudoers_userspec *spec = sudoers->userspecs;
    while (spec) {
        if (user_matches_spec(username, spec)) {
            /* Check hostname match */
            if (spec->hosts) {
                for (int i = 0; spec->hosts[i]; i++) {
                    if (match_pattern(spec->hosts[i], hostname)) {
                        return 1;  /* User has privileges */
                    }
                }
            }
        }
        spec = spec->next;
    }

    return 0;
}

/**
 * Check if user has NOPASSWD privileges
 */
int check_sudoers_nopasswd(const char *username, const char *hostname, struct sudoers_config *sudoers) {
    if (!username || !sudoers) {
        return 0;
    }

    if (!hostname) {
        hostname = "localhost";  /* Default hostname */
    }

    struct sudoers_userspec *spec = sudoers->userspecs;
    while (spec) {
        if (user_matches_spec(username, spec)) {
            /* Check hostname match */
            if (spec->hosts) {
                for (int i = 0; spec->hosts[i]; i++) {
                    if (match_pattern(spec->hosts[i], hostname)) {
                        /* If this rule has NOPASSWD, return YES immediately */
                        if (spec->nopasswd) {
                            return 1;
                        }
                        /* If this rule doesn't have NOPASSWD, continue checking other rules */
                        /* Don't return 0 here - there might be other rules with NOPASSWD */
                    }
                }
            }
        }
        spec = spec->next;
    }

    return 0;  /* No matching rule with NOPASSWD found */
}

/**
 * List available commands for a user from sudoers configuration
 * Shows each permission source separately with detailed attribution
 */
void list_available_commands(const char *username) {
    struct sudoers_config *sudoers_config = NULL;
    char hostname[256];
    int found_any_rules = 0;
    int found_direct_sudoers = 0;
    int found_group_privileges = 0;

    if (!username) {
        printf("Error: No username provided\n");
        return;
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }

    printf("Sudo privileges for %s on %s:\n", username, hostname);
    printf("=====================================\n\n");

    /* First show Defaults entries using sudo -l */
    char command[256];
    snprintf(command, sizeof(command), "sudo -l -U %s 2>/dev/null", username);

    FILE *fp = popen(command, "r");
    if (fp) {
        char buffer[1024];
        int in_defaults = 0;
        int found_defaults = 0;

        while (fgets(buffer, sizeof(buffer), fp)) {
            /* Check if we're in the Defaults section */
            if (strstr(buffer, "Matching Defaults entries")) {
                in_defaults = 1;
                found_defaults = 1;
                printf("Defaults Configuration:\n");
                continue;
            }

            /* Stop at commands section */
            if (strstr(buffer, "may run the following commands")) {
                break;
            }

            /* Print Defaults entries */
            if (in_defaults) {
                /* Skip empty lines */
                if (buffer[0] != '\n' && buffer[0] != '\r') {
                    printf("    %s", buffer);
                }
            }
        }
        pclose(fp);

        if (found_defaults) {
            printf("\n");
        }
    }

    /* Show LDAP/SSSD-based rules from sudo -l output */
    printf("LDAP/SSSD-Based Rules (from sudo -l):\n");
    fp = popen(command, "r");
    if (fp) {
        char buffer[1024];
        int in_commands = 0;
        int found_ldap_rules = 0;

        while (fgets(buffer, sizeof(buffer), fp)) {
            /* Check if we're in the commands section */
            if (strstr(buffer, "may run the following commands")) {
                in_commands = 1;
                continue;
            }

            /* Parse command entries */
            if (in_commands) {
                char *trimmed = buffer;
                while (*trimmed && isspace(*trimmed)) trimmed++;

                if (*trimmed == '(' && strchr(trimmed, ')')) {
                    /* This is a sudo rule entry */
                    found_ldap_rules = 1;
                    found_any_rules = 1;

                    /* Remove newline */
                    size_t len = strlen(buffer);
                    if (len > 0 && buffer[len-1] == '\n') {
                        buffer[len-1] = '\0';
                    }

                    printf("    %s  [Source: LDAP/SSSD]\n", trimmed);
                }
            }
        }
        pclose(fp);

        if (!found_ldap_rules) {
            printf("    No LDAP/SSSD-based rules found\n");
        }
    } else {
        printf("    Could not query LDAP/SSSD rules\n");
    }
    printf("\n");

    /* Show direct sudoers rules */
    printf("Direct Sudoers Rules (from /etc/sudoers):\n");
    sudoers_config = parse_sudoers_file(NULL);
    if (sudoers_config) {
        struct sudoers_userspec *spec = sudoers_config->userspecs;
        int found_direct_rules = 0;

        while (spec) {
            /* Check if this rule applies to the user */
            if (user_matches_spec(username, spec)) {
                found_direct_rules = 1;
                found_direct_sudoers = 1;
                found_any_rules = 1;

                /* Print the rule with source indication */
                printf("    ");

                /* Print hosts */
                if (spec->hosts) {
                    for (int i = 0; spec->hosts && spec->hosts[i]; i++) {
                        if (i > 0) printf(", ");
                        printf("%s", spec->hosts[i]);
                    }
                }

                printf(" = ");

                /* Print runas user */
                if (spec->runas_user) {
                    printf("(%s) ", spec->runas_user);
                } else {
                    printf("(root) ");
                }

                /* Print NOPASSWD if applicable */
                if (spec->nopasswd) {
                    printf("NOPASSWD: ");
                }

                /* Print commands */
                if (spec->commands) {
                    for (int i = 0; spec->commands && spec->commands[i]; i++) {
                        if (i > 0) printf(", ");
                        printf("%s", spec->commands[i]);
                    }
                }

                printf("  [Source: %s]\n", spec->source_file ? spec->source_file : "sudoers file");
            }
            spec = spec->next;
        }

        if (!found_direct_rules) {
            printf("    No direct sudoers rules found for user %s\n", username);
        }
    } else {
        printf("    Error: Could not parse sudoers configuration\n");
    }
    printf("\n");

    /* Show group-based privileges */
    printf("Group-Based Privileges:\n");
    const char *admin_groups[] = {"wheel", "sudo", "admin", NULL};

    for (int i = 0; admin_groups[i]; i++) {
        struct group *grp = getgrnam(admin_groups[i]);
        if (grp && grp->gr_mem) {
            int is_member = 0;
            for (char **member = grp->gr_mem; member && *member; member++) {
                if (*member && strcmp(*member, username) == 0) {
                    is_member = 1;
                    found_group_privileges = 1;
                    found_any_rules = 1;
                    break;
                }
            }

            if (is_member) {
                printf("    Group '%s': (ALL) ALL  [Source: group membership]\n", admin_groups[i]);
            }
        }
    }

    if (!found_group_privileges) {
        printf("    User %s is not a member of any admin groups (wheel, sudo, admin)\n", username);
    }
    printf("\n");

    /* Show system-wide sudoers rules that might apply through groups */
    printf("System-Wide Group Rules:\n");
    if (sudoers_config) {
        struct sudoers_userspec *spec = sudoers_config->userspecs;
        int found_group_rules = 0;

        while (spec) {
            /* Check if this rule applies to groups the user is in */
            if (spec->users) {
                for (int i = 0; spec->users && spec->users[i]; i++) {
                    if (spec->users[i] && spec->users[i][0] == '%') {
                        /* This is a group rule */
                        char *group_name = spec->users[i] + 1;
                        if (group_name && *group_name) {
                            struct group *grp = getgrnam(group_name);
                            if (grp && grp->gr_mem) {
                                for (char **member = grp->gr_mem; member && *member; member++) {
                                    if (*member && strcmp(*member, username) == 0) {
                                        found_group_rules = 1;
                                        found_any_rules = 1;

                                        printf("    ");

                                        /* Print hosts */
                                        if (spec->hosts) {
                                            for (int j = 0; spec->hosts[j]; j++) {
                                                if (j > 0) printf(", ");
                                                printf("%s", spec->hosts[j]);
                                            }
                                        }

                                        printf(" = ");

                                        /* Print runas user */
                                        if (spec->runas_user) {
                                            printf("(%s) ", spec->runas_user);
                                        } else {
                                            printf("(root) ");
                                        }

                                        /* Print NOPASSWD if applicable */
                                        if (spec->nopasswd) {
                                            printf("NOPASSWD: ");
                                        }

                                        /* Print commands */
                                        if (spec->commands) {
                                            for (int j = 0; spec->commands && spec->commands[j]; j++) {
                                                if (j > 0) printf(", ");
                                                printf("%s", spec->commands[j]);
                                            }
                                        }

                                        printf("  [Source: %%%s group rule in %s]\n", group_name,
                                               spec->source_file ? spec->source_file : "sudoers file");
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            spec = spec->next;
        }

        if (!found_group_rules) {
            printf("    No system-wide group rules apply to user %s\n", username);
        }
    } else {
        printf("    Error: Could not check system-wide group rules\n");
    }
    printf("\n");

    /* Summary */
    if (found_any_rules) {
        printf("Summary:\n");
        if (found_direct_sudoers) {
            printf("✓ User has direct sudoers rules\n");
        }
        if (found_group_privileges) {
            printf("✓ User has privileges through group membership\n");
        }
        printf("User %s is authorized to run sudo commands on %s\n", username, hostname);
    } else {
        printf("Summary:\n");
        printf("✗ User %s has no sudo privileges on %s\n", username, hostname);
        printf("User is not in any admin groups and has no explicit sudoers rules\n");
    }

    /* Clean up */
    if (sudoers_config) {
        free_sudoers_config(sudoers_config);
    }
}
