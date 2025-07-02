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
                /* Parse and check sudoers file */
                sudoers_config = parse_sudoers_file(NULL);
                if (sudoers_config) {
                    has_privileges = check_sudoers_privileges(username, hostname, sudoers_config);
                    free_sudoers_config(sudoers_config);
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

    /* If no privileges found via NSS sources, fall back to original method */
    if (!has_privileges) {
        has_privileges = check_sudo_privileges(username);
    }

    return has_privileges;
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
                /* Parse and check sudoers file for NOPASSWD */
                sudoers_config = parse_sudoers_file(NULL);
                if (sudoers_config) {
                    has_nopasswd = check_sudoers_nopasswd(username, hostname, sudoers_config);
                    free_sudoers_config(sudoers_config);
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
