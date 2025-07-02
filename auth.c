#include "sudosh.h"

/* Global variable for password input */
static char *g_password = NULL;

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
 * Check if user has sudo privileges
 * This is a simplified check - in a real implementation,
 * you would parse /etc/sudoers or use sudo's API
 */
int check_sudo_privileges(const char *username) {
    struct group *wheel_group;
    char **member;

    /* Check for NULL or empty username */
    if (!username || *username == '\0') {
        return 0;
    }

    /* Check if user is in wheel group */
    wheel_group = getgrnam("wheel");
    if (!wheel_group) {
        /* Try sudo group on Debian/Ubuntu systems */
        wheel_group = getgrnam("sudo");
        if (!wheel_group) {
            return 0;
        }
    }

    /* Check if gr_mem is NULL */
    if (!wheel_group->gr_mem) {
        return 0;
    }

    for (member = wheel_group->gr_mem; *member; member++) {
        if (strcmp(*member, username) == 0) {
            return 1;
        }
    }

    return 0;
}
