#include "sudosh.h"

/**
 * Main program loop - interactive shell
 */
int main_loop(void) {
    char *username = NULL;
    char *command_line = NULL;
    struct user_info *user = NULL;
    struct command_info cmd;
    int result;
    int builtin_result;

    /* Get current username */
    username = get_current_username();
    if (!username) {
        fprintf(stderr, "sudosh: unable to determine current user\n");
        return EXIT_FAILURE;
    }

    /* Set username for signal handler */
    set_current_username(username);

    /* Check if user has sudo privileges */
    if (!check_sudo_privileges(username)) {
        fprintf(stderr, "sudosh: %s is not in the sudoers file. This incident will be reported.\n", username);
        log_security_violation(username, "user not in sudoers");
        free(username);
        return EXIT_AUTH_FAILURE;
    }

    /* Authenticate user */
    printf("We trust you have received the usual lecture from the local System\n");
    printf("Administrator. It usually boils down to these three things:\n\n");
    printf("    #1) Respect the privacy of others.\n");
    printf("    #2) Think before you type.\n");
    printf("    #3) With great power comes great responsibility.\n\n");
    
    if (!authenticate_user(username)) {
        fprintf(stderr, "sudosh: authentication failed\n");
        free(username);
        return EXIT_AUTH_FAILURE;
    }

    /* Get user information */
    user = get_user_info(username);
    if (!user) {
        fprintf(stderr, "sudosh: unable to get user information\n");
        free(username);
        return EXIT_FAILURE;
    }

    /* Log session start */
    log_session_start(username);

    /* Print banner */
    print_banner();

    /* Main command loop */
    while (!is_interrupted()) {
        /* Read command from user */
        command_line = read_command();
        if (!command_line) {
            /* EOF or error */
            break;
        }

        /* Trim whitespace */
        command_line = trim_whitespace(command_line);

        /* Skip empty commands */
        if (is_empty_command(command_line)) {
            free(command_line);
            continue;
        }

        /* Check for built-in commands */
        builtin_result = handle_builtin_command(command_line);
        if (builtin_result == -1) {
            /* Exit command */
            free(command_line);
            break;
        } else if (builtin_result == 1) {
            /* Built-in command handled */
            free(command_line);
            continue;
        }

        /* Validate command for security */
        if (!validate_command(command_line)) {
            fprintf(stderr, "sudosh: command rejected for security reasons\n");
            free(command_line);
            continue;
        }

        /* Parse command */
        if (parse_command(command_line, &cmd) != 0) {
            fprintf(stderr, "sudosh: failed to parse command\n");
            free(command_line);
            continue;
        }

        /* Execute command */
        result = execute_command(&cmd, user);
        
        /* Log command execution */
        log_command(username, command_line, (result == 0));

        /* Clean up command structure */
        free_command_info(&cmd);
        free(command_line);
    }

    /* Log session end */
    log_session_end(username);

    /* Clean up */
    free_user_info(user);
    free(username);

    return EXIT_SUCCESS;
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    int exit_code;

    /* Parse command line arguments */
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [options]\n\n", argv[0]);
            printf("sudosh - Interactive sudo shell\n\n");
            printf("Options:\n");
            printf("  -h, --help     Show this help message\n");
            printf("  -v, --version  Show version information\n\n");
            printf("sudosh provides an interactive shell with sudo privileges.\n");
            printf("All commands are authenticated and logged to syslog.\n");
            return EXIT_SUCCESS;
        } else if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            printf("sudosh %s\n", SUDOSH_VERSION);
            printf("Interactive sudo shell with command logging\n");
            return EXIT_SUCCESS;
        } else {
            fprintf(stderr, "sudosh: unknown option '%s'\n", argv[1]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* Initialize logging */
    init_logging();

    /* Initialize security measures */
    init_security();

    /* Run main program loop */
    exit_code = main_loop();

    /* Clean up and exit */
    cleanup_and_exit(exit_code);

    return exit_code; /* Should never reach here */
}
