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
                user = get_user_info(username);  /* Use existing files-based lookup */
                break;
                
            case NSS_SOURCE_SSSD:
                user = get_user_info_sssd(username);
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
