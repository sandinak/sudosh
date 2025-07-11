/**
 * auth.c - Authentication and Authorization
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Handles user authentication, privilege checking, sudoers parsing,
 * and target user validation for sudosh.
 */

#include "sudosh.h"

/* Weak symbol for verbose_mode - can be overridden by main.c or test files */
__attribute__((weak)) int verbose_mode = 0;

#ifdef MOCK_AUTH
/* Mock authentication for systems without PAM */
static int mock_authenticate(const char *username) {
    char *password;

    printf("Mock authentication for user: %s\n", username);
    password = get_password("Password: ");

    if (!password) {
        return 0;
    }

    /* Simple mock - accept any non-empty password */
    int result = (strlen(password) > 0);

    /* Clear password from memory */
    memset(password, 0, strlen(password));
    free(password);

    return result;
}
#endif

#ifndef MOCK_AUTH
/**
 * PAM conversation function
 * Handles password prompts and other PAM messages
 */
int pam_conversation(int num_msg, const struct pam_message **msg,
                    struct pam_response **resp, void *appdata_ptr) {
    (void)appdata_ptr;  /* Suppress unused parameter warning */
    struct pam_response *responses;
    int i;

    if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG) {
        return PAM_CONV_ERR;
    }

    responses = calloc(num_msg, sizeof(struct pam_response));
    if (!responses) {
        return PAM_BUF_ERR;
    }

    for (i = 0; i < num_msg; i++) {
        switch (msg[i]->msg_style) {
            case PAM_PROMPT_ECHO_OFF:
                /* Password prompt */
                responses[i].resp = get_password(msg[i]->msg);
                if (!responses[i].resp) {
                    goto cleanup_error;
                }
                responses[i].resp_retcode = 0;
                break;

            case PAM_PROMPT_ECHO_ON:
                /* Username or other visible prompt */
                printf("%s", msg[i]->msg);
                fflush(stdout);
                responses[i].resp = malloc(MAX_USERNAME_LENGTH);
                if (!responses[i].resp || !fgets(responses[i].resp, MAX_USERNAME_LENGTH, stdin)) {
                    goto cleanup_error;
                }
                /* Remove newline */
                responses[i].resp[strcspn(responses[i].resp, "\n")] = '\0';
                responses[i].resp_retcode = 0;
                break;

            case PAM_ERROR_MSG:
                fprintf(stderr, "Error: %s\n", msg[i]->msg);
                responses[i].resp = NULL;
                responses[i].resp_retcode = 0;
                break;

            case PAM_TEXT_INFO:
                printf("Info: %s\n", msg[i]->msg);
                responses[i].resp = NULL;
                responses[i].resp_retcode = 0;
                break;

            default:
                goto cleanup_error;
        }
    }

    *resp = responses;
    return PAM_SUCCESS;

cleanup_error:
    for (i = 0; i < num_msg; i++) {
        if (responses[i].resp) {
            memset(responses[i].resp, 0, strlen(responses[i].resp));
            free(responses[i].resp);
        }
    }
    free(responses);
    return PAM_CONV_ERR;
}
#endif /* MOCK_AUTH */

/**
 * Get password from user with echo disabled
 */
char *get_password(const char *prompt) {
    struct termios old_termios, new_termios;
    char *password;
    size_t len;

    password = malloc(MAX_PASSWORD_LENGTH);
    if (!password) {
        return NULL;
    }

    printf("%s", prompt);
    fflush(stdout);

    /* Disable echo */
    if (tcgetattr(STDIN_FILENO, &old_termios) != 0) {
        free(password);
        return NULL;
    }

    new_termios = old_termios;
    new_termios.c_lflag &= ~ECHO;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios) != 0) {
        free(password);
        return NULL;
    }

    /* Read password */
    if (!fgets(password, MAX_PASSWORD_LENGTH, stdin)) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
        free(password);
        return NULL;
    }

    /* Restore echo */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
    printf("\n");

    /* Remove newline */
    len = strlen(password);
    if (len > 0 && password[len - 1] == '\n') {
        password[len - 1] = '\0';
    }

    return password;
}

/**
 * Create authentication cache directory if it doesn't exist
 */
int create_auth_cache_dir(void) {
    struct stat st;

    /* Check if directory exists */
    if (stat(AUTH_CACHE_DIR, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            /* Directory exists, check permissions */
            if ((st.st_mode & 0777) != 0700) {
                /* Fix permissions */
                if (chmod(AUTH_CACHE_DIR, 0700) != 0) {
                    return 0;
                }
            }
            return 1;
        } else {
            /* Path exists but is not a directory */
            return 0;
        }
    }

    /* Create directory with secure permissions */
    if (mkdir(AUTH_CACHE_DIR, 0700) != 0) {
        return 0;
    }

    return 1;
}

/**
 * Get authentication cache file path for a user
 */
char *get_auth_cache_path(const char *username) {
    char *cache_path;
    char *tty;
    char tty_safe[64];
    int i;

    if (!username) {
        return NULL;
    }

    cache_path = malloc(MAX_CACHE_PATH_LENGTH);
    if (!cache_path) {
        return NULL;
    }

    /* Get TTY and make it filesystem-safe */
    tty = ttyname(STDIN_FILENO);
    if (tty) {
        /* Remove /dev/ prefix and replace / with _ */
        if (strncmp(tty, "/dev/", 5) == 0) {
            tty += 5;
        }
        strncpy(tty_safe, tty, sizeof(tty_safe) - 1);
        tty_safe[sizeof(tty_safe) - 1] = '\0';

        /* Replace / with _ for filesystem safety */
        for (i = 0; tty_safe[i]; i++) {
            if (tty_safe[i] == '/') {
                tty_safe[i] = '_';
            }
        }
    } else {
        strcpy(tty_safe, "unknown");
    }

    /* Create cache file path: /var/run/sudosh/auth_cache_username_tty */
    snprintf(cache_path, MAX_CACHE_PATH_LENGTH, "%s/%s%s_%s",
             AUTH_CACHE_DIR, AUTH_CACHE_FILE_PREFIX, username, tty_safe);

    return cache_path;
}

/**
 * Check if authentication is cached and still valid
 */
int check_auth_cache(const char *username) {
    char *cache_path;
    FILE *cache_file;
    struct auth_cache cache_data;
    time_t current_time;
    struct stat st;

    if (!username) {
        return 0;
    }

    cache_path = get_auth_cache_path(username);
    if (!cache_path) {
        return 0;
    }

    /* Check if cache file exists and get its stats */
    if (stat(cache_path, &st) != 0) {
        free(cache_path);
        return 0;
    }

    /* Check file permissions for security */
    if ((st.st_mode & 0777) != 0600) {
        /* Invalid permissions, remove file */
        unlink(cache_path);
        free(cache_path);
        return 0;
    }

    /* Check if file is owned by root */
    if (st.st_uid != 0) {
        /* Invalid ownership, remove file */
        unlink(cache_path);
        free(cache_path);
        return 0;
    }

    cache_file = fopen(cache_path, "rb");
    if (!cache_file) {
        free(cache_path);
        return 0;
    }

    /* Read cache data */
    if (fread(&cache_data, sizeof(cache_data), 1, cache_file) != 1) {
        fclose(cache_file);
        free(cache_path);
        return 0;
    }

    fclose(cache_file);
    free(cache_path);

    /* Verify username matches */
    if (strcmp(cache_data.username, username) != 0) {
        return 0;
    }

    /* Check if cache has expired */
    current_time = time(NULL);
    if (current_time - cache_data.timestamp > AUTH_CACHE_TIMEOUT) {
        /* Cache expired, remove it */
        cache_path = get_auth_cache_path(username);
        if (cache_path) {
            unlink(cache_path);
            free(cache_path);
        }
        return 0;
    }

    /* Cache is valid */
    return 1;
}

/**
 * Update authentication cache with current session info
 */
int update_auth_cache(const char *username) {
    char *cache_path;
    FILE *cache_file;
    struct auth_cache cache_data;
    char *tty;

    if (!username) {
        return 0;
    }

    /* Create cache directory if needed */
    if (!create_auth_cache_dir()) {
        return 0;
    }

    cache_path = get_auth_cache_path(username);
    if (!cache_path) {
        return 0;
    }

    /* Prepare cache data */
    memset(&cache_data, 0, sizeof(cache_data));
    strncpy(cache_data.username, username, sizeof(cache_data.username) - 1);
    cache_data.timestamp = time(NULL);
    cache_data.session_id = getsid(0);
    cache_data.uid = getuid();
    cache_data.gid = getgid();

    /* Get TTY */
    tty = ttyname(STDIN_FILENO);
    if (tty) {
        if (strncmp(tty, "/dev/", 5) == 0) {
            tty += 5;
        }
        strncpy(cache_data.tty, tty, sizeof(cache_data.tty) - 1);
    } else {
        strcpy(cache_data.tty, "unknown");
    }

    /* Get hostname */
    if (gethostname(cache_data.hostname, sizeof(cache_data.hostname)) != 0) {
        strcpy(cache_data.hostname, "localhost");
    }

    /* Write cache file with secure permissions */
    cache_file = fopen(cache_path, "wb");
    if (!cache_file) {
        free(cache_path);
        return 0;
    }

    /* Set secure permissions before writing */
    if (chmod(cache_path, 0600) != 0) {
        fclose(cache_file);
        unlink(cache_path);
        free(cache_path);
        return 0;
    }

    /* Write cache data */
    if (fwrite(&cache_data, sizeof(cache_data), 1, cache_file) != 1) {
        fclose(cache_file);
        unlink(cache_path);
        free(cache_path);
        return 0;
    }

    fclose(cache_file);
    free(cache_path);

    return 1;
}

/**
 * Clear authentication cache for a user
 */
void clear_auth_cache(const char *username) {
    char *cache_path;

    if (!username) {
        return;
    }

    cache_path = get_auth_cache_path(username);
    if (cache_path) {
        unlink(cache_path);
        free(cache_path);
    }
}

/**
 * Cleanup old authentication cache files
 */
void cleanup_auth_cache(void) {
    DIR *cache_dir;
    struct dirent *entry;
    char file_path[MAX_CACHE_PATH_LENGTH];
    struct stat st;
    time_t current_time;

    cache_dir = opendir(AUTH_CACHE_DIR);
    if (!cache_dir) {
        return;
    }

    current_time = time(NULL);

    while ((entry = readdir(cache_dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Only process our cache files */
        if (strncmp(entry->d_name, AUTH_CACHE_FILE_PREFIX, strlen(AUTH_CACHE_FILE_PREFIX)) != 0) {
            continue;
        }

        snprintf(file_path, sizeof(file_path), "%s/%s", AUTH_CACHE_DIR, entry->d_name);

        if (stat(file_path, &st) == 0) {
            /* Remove files older than cache timeout */
            if (current_time - st.st_mtime > AUTH_CACHE_TIMEOUT) {
                unlink(file_path);
            }
        }
    }

    closedir(cache_dir);
}

/**
 * Authenticate user with caching support
 */
int authenticate_user_cached(const char *username) {
    /* Check for NULL username */
    if (!username) {
        return 0;
    }

    /* First check if authentication is cached */
    if (check_auth_cache(username)) {
        if (verbose_mode) {
            printf("sudosh: using cached authentication for %s\n", username);
        }
        log_authentication(username, 1);
        return 1;
    }

    /* Cache miss or expired, perform full authentication */
    if (authenticate_user(username)) {
        /* Authentication successful, update cache */
        update_auth_cache(username);
        return 1;
    }

    /* Authentication failed, clear any existing cache */
    clear_auth_cache(username);
    return 0;
}

/**
 * Authenticate user using PAM or mock authentication
 */
int authenticate_user(const char *username) {
#ifdef MOCK_AUTH
    /* Use mock authentication for systems without PAM */
    int result = mock_authenticate(username);
    log_authentication(username, result);
    return result;
#else
    pam_handle_t *pamh = NULL;
    struct pam_conv conv = {
        .conv = pam_conversation,
        .appdata_ptr = NULL
    };
    int retval;

    /* Initialize PAM */
    retval = pam_start("sudo", username, &conv, &pamh);
    if (retval != PAM_SUCCESS) {
        log_error("PAM initialization failed");
        return 0;
    }

    /* Authenticate */
    retval = pam_authenticate(pamh, 0);
    if (retval != PAM_SUCCESS) {
        log_authentication(username, 0);
        pam_end(pamh, retval);
        return 0;
    }

    /* Check account validity */
    retval = pam_acct_mgmt(pamh, 0);
    if (retval != PAM_SUCCESS) {
        log_authentication(username, 0);
        pam_end(pamh, retval);
        return 0;
    }

    /* Clean up */
    pam_end(pamh, PAM_SUCCESS);
    log_authentication(username, 1);
    return 1;
#endif
}

/**
 * Enhanced sudo privilege checking using NSS, sudoers parsing, and SSSD
 */
int check_sudo_privileges_enhanced(const char *username) {
    struct nss_config *nss_config = NULL;
    struct sudoers_config *sudoers_config = NULL;
    struct nss_source *source;
    int has_privileges = 0;
    char hostname[256];

    /* Check for NULL or empty username */
    if (!username || *username == '\0') {
        return 0;
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }

    /* Read NSS configuration */
    nss_config = read_nss_config();
    if (!nss_config) {
        return check_sudo_privileges_fallback(username);
    }

    /* Check each sudoers source according to NSS configuration */
    for (source = nss_config->sudoers_sources; source && !has_privileges; source = source->next) {
        switch (source->type) {
            case NSS_SOURCE_FILES:
                /* Parse and check sudoers file with privilege escalation */
                sudoers_config = parse_sudoers_file(NULL);
                if (sudoers_config) {
                    has_privileges = check_sudoers_privileges(username, hostname, sudoers_config);
                    free_sudoers_config(sudoers_config);
                    /* If we successfully parsed sudoers, don't fall back to sudo -l */
                    if (has_privileges) {
                        free_nss_config(nss_config);
                        return has_privileges;
                    }
                }
                break;

            case NSS_SOURCE_SSSD:
                /* Check SSSD for sudo privileges */
                has_privileges = check_sssd_privileges(username);
                break;

            case NSS_SOURCE_LDAP:
                /* LDAP support would go here */
                break;

            default:
                continue;
        }
    }

    free_nss_config(nss_config);

    /* If no privileges found via NSS sources, try direct sudoers parsing */
    if (!has_privileges) {
        /* Try to parse sudoers file directly (will escalate privileges if needed) */
        struct sudoers_config *sudoers_config = parse_sudoers_file(NULL);
        if (sudoers_config) {
            char hostname[256];
            if (gethostname(hostname, sizeof(hostname)) != 0) {
                strcpy(hostname, "localhost");
            }
            has_privileges = check_sudoers_privileges(username, hostname, sudoers_config);
            free_sudoers_config(sudoers_config);
        }

        /* If still no privileges, fall back to sudo -l method */
        if (!has_privileges) {
            has_privileges = check_sudo_privileges(username);
        }
    }

    return has_privileges;
}

/**
 * Validate that the target user exists and is valid
 */
int validate_target_user(const char *target_user) {
    struct passwd *pwd;

    if (!target_user || *target_user == '\0') {
        return 0;
    }

    /* Check if target user exists */
    pwd = getpwnam(target_user);
    if (!pwd) {
        return 0;
    }

    /* Additional validation - ensure it's not a system account we shouldn't use */
    /* Allow root and regular users, but be cautious about system accounts */
    if (pwd->pw_uid == 0) {
        /* Root is always valid */
        return 1;
    }

    /* Check for reasonable UID range (typically > 100 or > 1000 depending on system) */
    if (pwd->pw_uid < 100) {
        /* System account - be more restrictive */
        /* Allow common service accounts that might be legitimate targets */
        const char *allowed_system_users[] = {
            "www-data", "apache", "nginx", "mysql", "postgres", "redis",
            "mongodb", "elasticsearch", "jenkins", "docker", "nobody",
            NULL
        };

        for (int i = 0; allowed_system_users[i]; i++) {
            if (strcmp(target_user, allowed_system_users[i]) == 0) {
                return 1;
            }
        }

        /* Other system accounts require explicit approval */
        printf("Warning: '%s' appears to be a system account (UID %d)\n", target_user, pwd->pw_uid);
        return prompt_user_confirmation("", "Running as system account");
    }

    return 1; /* Regular user account */
}

/**
 * Get user info for target user
 */
struct user_info *get_target_user_info(const char *target_user) {
    if (!target_user) {
        return NULL;
    }

    return get_user_info(target_user);
}

/**
 * Check if user has permission to run commands as target user
 * This checks sudoers rules for runas permissions
 */
int check_runas_permissions(const char *username, const char *target_user) {
    char command[512];
    FILE *fp;
    char buffer[1024];
    int has_permission = 0;

    if (!username || !target_user) {
        return 0;
    }

    /* First validate that target user exists */
    if (!validate_target_user(target_user)) {
        return 0;
    }

    /* Special case: if target user is the same as current user, allow it */
    if (strcmp(username, target_user) == 0) {
        return 1;
    }

    /* Check using sudo -l -U username to see what the user can run */
    snprintf(command, sizeof(command), "sudo -l -U %s 2>/dev/null", username);

    fp = popen(command, "r");
    if (!fp) {
        return 0;
    }

    /* Parse sudo -l output to check for runas permissions */
    while (fgets(buffer, sizeof(buffer), fp)) {
        /* Look for runas specifications */
        if (strstr(buffer, "(ALL)") || strstr(buffer, "(ALL : ALL)")) {
            /* User can run as any user */
            has_permission = 1;
            break;
        }

        /* Look for specific user in runas specification */
        char pattern[256];
        snprintf(pattern, sizeof(pattern), "(%s)", target_user);
        if (strstr(buffer, pattern)) {
            has_permission = 1;
            break;
        }

        /* Look for user in runas list like (user1, user2, user3) */
        if (strchr(buffer, '(') && strchr(buffer, ')')) {
            char *start = strchr(buffer, '(');
            char *end = strchr(start, ')');
            if (start && end) {
                *end = '\0';
                start++; /* Skip opening parenthesis */

                /* Split by commas and check each user */
                char *token = strtok(start, ", ");
                while (token) {
                    /* Trim whitespace */
                    while (*token == ' ' || *token == '\t') token++;
                    char *token_end = token + strlen(token) - 1;
                    while (token_end > token && (*token_end == ' ' || *token_end == '\t')) {
                        *token_end = '\0';
                        token_end--;
                    }

                    if (strcmp(token, target_user) == 0) {
                        has_permission = 1;
                        break;
                    }
                    token = strtok(NULL, ", ");
                }
                if (has_permission) break;
            }
        }

        /* Look for root permission which typically allows running as any user */
        if (strstr(buffer, "(root)") && strcmp(target_user, "root") == 0) {
            has_permission = 1;
            break;
        }
    }

    pclose(fp);

    /* If no explicit permission found, try a more direct approach */
    if (!has_permission) {
        /* Try to check if user can run a simple command as target user */
        snprintf(command, sizeof(command),
                "sudo -l -U %s | grep -E '\\(%s\\)|\\(ALL\\)' >/dev/null 2>&1",
                username, target_user);

        if (system(command) == 0) {
            has_permission = 1;
        }
    }

    return has_permission;
}

/**
 * Check if user has NOPASSWD privileges using enhanced checking
 */
int check_nopasswd_privileges_enhanced(const char *username) {
    struct nss_config *nss_config = NULL;
    struct sudoers_config *sudoers_config = NULL;
    struct nss_source *source;
    int has_nopasswd = 0;
    char hostname[256];

    /* Check for NULL or empty username */
    if (!username || *username == '\0') {
        return 0;
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }

    /* Read NSS configuration */
    nss_config = read_nss_config();
    if (!nss_config) {
        return 0;
    }

    /* Check each sudoers source according to NSS configuration */
    for (source = nss_config->sudoers_sources; source && !has_nopasswd; source = source->next) {
        switch (source->type) {
            case NSS_SOURCE_FILES:
                /* Parse and check sudoers file for NOPASSWD with privilege escalation */
                sudoers_config = parse_sudoers_file(NULL);
                if (sudoers_config) {
                    has_nopasswd = check_sudoers_nopasswd(username, hostname, sudoers_config);
                    free_sudoers_config(sudoers_config);
                    /* If we successfully parsed sudoers, don't fall back to sudo -l */
                    if (has_nopasswd) {
                        free_nss_config(nss_config);
                        return has_nopasswd;
                    }
                }
                break;

            case NSS_SOURCE_SSSD:
                /* SSSD NOPASSWD checking would go here */
                /* For now, assume SSSD doesn't provide NOPASSWD info */
                break;

            case NSS_SOURCE_LDAP:
                /* LDAP NOPASSWD checking would go here */
                break;

            default:
                continue;
        }
    }

    free_nss_config(nss_config);

    /* If no NOPASSWD found via NSS sources, try direct sudoers parsing */
    if (!has_nopasswd) {
        /* Try to parse sudoers file directly (will escalate privileges if needed) */
        struct sudoers_config *sudoers_config = parse_sudoers_file(NULL);
        if (sudoers_config) {
            char hostname[256];
            if (gethostname(hostname, sizeof(hostname)) != 0) {
                strcpy(hostname, "localhost");
            }
            has_nopasswd = check_sudoers_nopasswd(username, hostname, sudoers_config);
            free_sudoers_config(sudoers_config);
        }

        /* If still no NOPASSWD found, fall back to sudo -l method */
        if (!has_nopasswd) {
            has_nopasswd = check_nopasswd_sudo_l(username);
        }
    }

    return has_nopasswd;
}

/**
 * Check if user has NOPASSWD privileges using sudo -l
 */
int check_nopasswd_sudo_l(const char *username) {
    char command[256];
    FILE *fp;
    char buffer[1024];
    int has_nopasswd = 0;

    /* Check for NULL or empty username */
    if (!username || *username == '\0') {
        return 0;
    }

    /* Build sudo -l command for the user */
    snprintf(command, sizeof(command), "sudo -l -U %s 2>/dev/null", username);

    /* Execute sudo -l to check privileges */
    fp = popen(command, "r");
    if (!fp) {
        return 0;
    }

    /* Read output and look for NOPASSWD indicators */
    while (fgets(buffer, sizeof(buffer), fp)) {
        /* Look for NOPASSWD in the output */
        if (strstr(buffer, "NOPASSWD:")) {
            has_nopasswd = 1;
            break;
        }
    }

    pclose(fp);
    return has_nopasswd;
}

/**
 * Check if user has sudo privileges by calling sudo -l
 * This properly checks the sudoers configuration
 */
int check_sudo_privileges(const char *username) {
    char command[256];
    FILE *fp;
    char buffer[1024];
    int has_privileges = 0;

    /* Check for NULL or empty username */
    if (!username || *username == '\0') {
        return 0;
    }

    /* Build sudo -l command for the user */
    snprintf(command, sizeof(command), "sudo -l -U %s 2>/dev/null", username);

    /* Execute sudo -l to check privileges */
    fp = popen(command, "r");
    if (!fp) {
        /* If we can't run sudo -l, fall back to group check */
        return check_sudo_privileges_fallback(username);
    }

    /* Read output and look for privilege indicators */
    while (fgets(buffer, sizeof(buffer), fp)) {
        /* Look for common sudo privilege patterns */
        if (strstr(buffer, "(ALL)") ||
            strstr(buffer, "may run the following commands") ||
            strstr(buffer, "NOPASSWD:")) {
            has_privileges = 1;
            break;
        }
    }

    pclose(fp);
    return has_privileges;
}

/**
 * Fallback check using group membership
 * Used when sudo -l is not available
 */
int check_sudo_privileges_fallback(const char *username) {
    struct group *admin_group;
    char **member;
    const char *groups_to_check[] = {"wheel", "sudo", "admin", NULL};
    int i;

    /* Check for NULL or empty username */
    if (!username || *username == '\0') {
        return 0;
    }

    /* Check multiple possible admin groups */
    for (i = 0; groups_to_check[i] != NULL; i++) {
        admin_group = getgrnam(groups_to_check[i]);
        if (!admin_group || !admin_group->gr_mem) {
            continue;
        }

        for (member = admin_group->gr_mem; *member; member++) {
            if (strcmp(*member, username) == 0) {
                return 1;
            }
        }
    }

    return 0;
}
