/**
 * nss.c - Name Service Switch (NSS) Integration
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Handles NSS configuration parsing and integration with system
 * name resolution services for user and group lookups.
 */

#include "sudosh.h"
#include <fcntl.h>

/* Default NSS configuration paths */
#define NSS_CONF_PATH "/etc/nsswitch.conf"
#define NSS_CONF_PATH_ALT "/etc/netsvc.conf"  /* AIX */

/**
 * Parse NSS source type from string
 */
static enum nss_source_type parse_nss_source_type(const char *source) {
    if (!source) {
        return NSS_SOURCE_UNKNOWN;
    }
    
    if (strcmp(source, "files") == 0) {
        return NSS_SOURCE_FILES;
    } else if (strcmp(source, "sssd") == 0) {
        return NSS_SOURCE_SSSD;
    } else if (strcmp(source, "ldap") == 0) {
        return NSS_SOURCE_LDAP;
    }
    
    return NSS_SOURCE_UNKNOWN;
}

/**
 * Create a new NSS source entry
 */
struct nss_source *create_nss_source(const char *name) {
    struct nss_source *source = malloc(sizeof(struct nss_source));
    if (!source) {
        return NULL;
    }
    
    source->name = strdup(name);
    source->type = parse_nss_source_type(name);
    source->next = NULL;
    
    if (!source->name) {
        free(source);
        return NULL;
    }
    
    return source;
}

/**
 * Free NSS source list
 */
static void free_nss_source_list(struct nss_source *sources) {
    struct nss_source *current = sources;
    struct nss_source *next;
    
    while (current) {
        next = current->next;
        free(current->name);
        free(current);
        current = next;
    }
}

/**
 * Parse NSS configuration line
 */
static struct nss_source *parse_nss_line(const char *line, const char *service) {
    char *line_copy = strdup(line);
    char *token;
    char *saveptr;
    struct nss_source *sources = NULL;
    struct nss_source *last = NULL;
    
    if (!line_copy) {
        return NULL;
    }
    
    /* Skip service name */
    token = strtok_r(line_copy, " \t:", &saveptr);
    if (!token || strcmp(token, service) != 0) {
        free(line_copy);
        return NULL;
    }
    
    /* Parse sources */
    while ((token = strtok_r(NULL, " \t", &saveptr)) != NULL) {
        /* Skip action specifiers like [NOTFOUND=return] */
        if (token[0] == '[') {
            continue;
        }
        
        struct nss_source *source = create_nss_source(token);
        if (source) {
            if (!sources) {
                sources = source;
                last = source;
            } else {
                last->next = source;
                last = source;
            }
        }
    }
    
    free(line_copy);
    return sources;
}

/**
 * Read NSS configuration from file
 */
struct nss_config *read_nss_config(void) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    struct nss_config *config;
    
    config = malloc(sizeof(struct nss_config));
    if (!config) {
        return NULL;
    }
    
    config->passwd_sources = NULL;
    config->sudoers_sources = NULL;
    
    /* Try primary NSS configuration file */
    fp = fopen(NSS_CONF_PATH, "r");
    if (!fp) {
        /* Try alternative path (AIX) */
        fp = fopen(NSS_CONF_PATH_ALT, "r");
        if (!fp) {
            /* Use default configuration */
            config->passwd_sources = create_nss_source("files");
            config->sudoers_sources = create_nss_source("files");
            return config;
        }
    }
    
    while ((read = getline(&line, &len, fp)) != -1) {
        /* Remove newline */
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        
        /* Skip comments and empty lines */
        char *trimmed = line;
        while (*trimmed && isspace(*trimmed)) {
            trimmed++;
        }
        if (*trimmed == '#' || *trimmed == '\0') {
            continue;
        }
        
        /* Parse passwd line */
        if (strncmp(trimmed, "passwd:", 7) == 0) {
            if (!config->passwd_sources) {
                config->passwd_sources = parse_nss_line(trimmed, "passwd");
            }
        }
        
        /* Parse sudoers line */
        if (strncmp(trimmed, "sudoers:", 8) == 0) {
            if (!config->sudoers_sources) {
                config->sudoers_sources = parse_nss_line(trimmed, "sudoers");
            }
        }
    }
    
    free(line);
    fclose(fp);
    
    /* Set defaults if not found */
    if (!config->passwd_sources) {
        config->passwd_sources = create_nss_source("files");
    }
    if (!config->sudoers_sources) {
        config->sudoers_sources = create_nss_source("files");
    }
    
    return config;
}

/**
 * Free NSS configuration
 */
void free_nss_config(struct nss_config *config) {
    if (!config) {
        return;
    }
    
    free_nss_source_list(config->passwd_sources);
    free_nss_source_list(config->sudoers_sources);
    free(config);
}

/**
 * Get user information using NSS configuration
 */
struct user_info *get_user_info_nss(const char *username, struct nss_config *nss_config) {
    struct nss_source *source;
    struct user_info *user = NULL;

    if (!username || !nss_config) {
        return NULL;
    }

    /* Try each NSS source in order */
    for (source = nss_config->passwd_sources; source; source = source->next) {
        switch (source->type) {
            case NSS_SOURCE_FILES:
                user = get_user_info_files(username);
                break;

            case NSS_SOURCE_SSSD:
                user = get_user_info_sssd_direct(username);
                break;

            case NSS_SOURCE_LDAP:
                /* LDAP support would go here */
                break;

            default:
                continue;
        }

        if (user) {
            break;  /* Found user, stop searching */
        }
    }

    return user;
}

/**
 * Get user information directly from files (/etc/passwd)
 */
struct user_info *get_user_info_files(const char *username) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    struct user_info *user = NULL;

    if (!username) {
        return NULL;
    }

    fp = fopen("/etc/passwd", "r");
    if (!fp) {
        /* Fall back to getpwnam if we can't read /etc/passwd directly */
        return get_user_info(username);
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        /* Remove newline */
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }

        /* Parse passwd line: username:password:uid:gid:gecos:home:shell */
        char *fields[7];
        char *token, *saveptr;
        int field_count = 0;

        char *line_copy = strdup(line);
        if (!line_copy) {
            continue;
        }

        token = strtok_r(line_copy, ":", &saveptr);
        while (token && field_count < 7) {
            fields[field_count++] = token;
            token = strtok_r(NULL, ":", &saveptr);
        }

        if (field_count >= 6 && strcmp(fields[0], username) == 0) {
            /* Found the user */
            user = malloc(sizeof(struct user_info));
            if (user) {
                user->uid = (uid_t)atoi(fields[2]);
                user->gid = (gid_t)atoi(fields[3]);
                user->username = strdup(fields[0]);
                user->home_dir = strdup(fields[5]);
                user->shell = field_count >= 7 ? strdup(fields[6]) : strdup("/bin/sh");

                if (!user->username || !user->home_dir || !user->shell) {
                    free_user_info(user);
                    user = NULL;
                }
            }
            free(line_copy);
            break;
        }

        free(line_copy);
    }

    if (line) {
        free(line);
    }
    fclose(fp);

    return user;
}

/**
 * Get user information directly from SSSD without sudo dependency
 */
struct user_info *get_user_info_sssd_direct(const char *username) {
    if (!username) {
        return NULL;
    }

    /* First try standard getpwnam which should work with SSSD via NSS */
    struct user_info *user = get_user_info(username);
    if (user) {
        return user;
    }

    /* If that fails, try direct SSSD socket communication */
    return get_user_info_sssd_socket(username);
}

/**
 * Get user information via SSSD socket communication
 */
struct user_info *get_user_info_sssd_socket(const char *username) {
    /* This is a placeholder for direct SSSD socket communication
     * In a full implementation, this would use the SSSD client libraries
     * or communicate directly with SSSD via D-Bus or Unix sockets */

    if (!username) {
        return NULL;
    }

    /* For now, return NULL to indicate SSSD socket communication not implemented */
    return NULL;
}

/**
 * Check if user is in admin groups using direct file parsing
 */
int check_admin_groups_files(const char *username) {
    const char *admin_groups[] = {"wheel", "sudo", "admin", NULL};
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int is_admin = 0;

    if (!username) {
        return 0;
    }

    fp = fopen("/etc/group", "r");
    if (!fp) {
        /* Fall back to getgrnam if we can't read /etc/group directly */
        return check_admin_groups_getgrnam(username);
    }

    while ((read = getline(&line, &len, fp)) != -1 && !is_admin) {
        /* Remove newline */
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }

        /* Parse group line: groupname:password:gid:members */
        char *fields[4];
        char *token, *saveptr;
        int field_count = 0;

        char *line_copy = strdup(line);
        if (!line_copy) {
            continue;
        }

        token = strtok_r(line_copy, ":", &saveptr);
        while (token && field_count < 4) {
            fields[field_count++] = token;
            token = strtok_r(NULL, ":", &saveptr);
        }

        if (field_count >= 4) {
            /* Check if this is an admin group */
            for (int i = 0; admin_groups[i]; i++) {
                if (strcmp(fields[0], admin_groups[i]) == 0) {
                    /* Check if username is in the members list */
                    char *members = fields[3];
                    char *member_token, *member_saveptr;

                    member_token = strtok_r(members, ",", &member_saveptr);
                    while (member_token) {
                        /* Trim whitespace */
                        while (*member_token && isspace(*member_token)) {
                            member_token++;
                        }
                        char *end = member_token + strlen(member_token) - 1;
                        while (end > member_token && isspace(*end)) {
                            *end = '\0';
                            end--;
                        }

                        if (strcmp(member_token, username) == 0) {
                            is_admin = 1;
                            break;
                        }

                        member_token = strtok_r(NULL, ",", &member_saveptr);
                    }
                    break;
                }
            }
        }

        free(line_copy);
    }

    if (line) {
        free(line);
    }
    fclose(fp);

    return is_admin;
}

/**
 * Check admin groups using getgrnam (fallback)
 */
int check_admin_groups_getgrnam(const char *username) {
    const char *admin_groups[] = {"wheel", "sudo", "admin", NULL};

    if (!username) {
        return 0;
    }

    for (int i = 0; admin_groups[i]; i++) {
        struct group *grp = getgrnam(admin_groups[i]);
        if (grp && grp->gr_mem) {
            for (char **member = grp->gr_mem; *member; member++) {
                if (strcmp(*member, username) == 0) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

/**
 * Check sudo privileges using direct NSS parsing (no sudo dependency)
 */
int check_sudo_privileges_nss(const char *username) {
    struct nss_config *nss_config;
    struct sudoers_config *sudoers_config;
    char hostname[256];
    int has_privileges = 0;

    if (!username) {
        return 0;
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }

    /* First try direct sudoers file parsing */
    sudoers_config = parse_sudoers_file(NULL);
    if (sudoers_config) {
        has_privileges = check_sudoers_privileges(username, hostname, sudoers_config);
        free_sudoers_config(sudoers_config);

        if (has_privileges) {
            return 1;
        }
    }

    /* Try NSS-based group checking */
    nss_config = read_nss_config();
    if (nss_config) {
        struct nss_source *source;

        for (source = nss_config->passwd_sources; source && !has_privileges; source = source->next) {
            switch (source->type) {
                case NSS_SOURCE_FILES:
                    has_privileges = check_admin_groups_files(username);
                    break;

                case NSS_SOURCE_SSSD:
                    has_privileges = check_admin_groups_sssd_direct(username);
                    break;

                case NSS_SOURCE_LDAP:
                    /* LDAP support would go here */
                    break;

                default:
                    continue;
            }
        }

        free_nss_config(nss_config);
    }

    return has_privileges;
}

/**
 * Check admin groups via SSSD without sudo dependency
 */
int check_admin_groups_sssd_direct(const char *username) {
    if (!username) {
        return 0;
    }

    /* First try standard getgrnam which should work with SSSD via NSS */
    if (check_admin_groups_getgrnam(username)) {
        return 1;
    }

    /* Try direct SSSD communication if available */
    return check_sssd_groups_socket(username);
}

/**
 * Check SSSD groups via socket communication
 */
int check_sssd_groups_socket(const char *username) {
    /* This is a placeholder for direct SSSD socket communication
     * In a full implementation, this would use the SSSD client libraries */

    if (!username) {
        return 0;
    }

    /* For now, return 0 to indicate SSSD socket communication not implemented */
    return 0;
}

/**
 * Check specific command permission using direct sudoers parsing
 */
int check_command_permission_nss(const char *username, const char *command) {
    struct sudoers_config *sudoers_config;
    char hostname[256];
    int is_allowed = 0;

    if (!username || !command) {
        return 0;
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }

    /* Parse sudoers configuration */
    sudoers_config = parse_sudoers_file(NULL);
    if (sudoers_config) {
        is_allowed = check_sudoers_command_permission(username, hostname, command, sudoers_config);
        free_sudoers_config(sudoers_config);
    }

    return is_allowed;
}
