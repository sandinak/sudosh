/**
 * auth.c - Authentication and Authorization
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Handles user authentication, privilege checking, sudoers parsing,
 * and target user validation for sudosh.
 */

#include "sudosh.h"

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
