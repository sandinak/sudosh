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

    /* Check if user has sudo privileges using enhanced checking */
    if (!check_sudo_privileges_enhanced(username)) {
        fprintf(stderr, "sudosh: %s is not in the sudoers file. This incident will be reported.\n", username);
        log_security_violation(username, "user not in sudoers");
        free(username);
        return EXIT_AUTH_FAILURE;
    }

    /* Check if user has NOPASSWD privileges */
    int has_nopasswd = check_nopasswd_privileges_enhanced(username);

    if (!has_nopasswd) {
        /* Authenticate user with password */
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
    } else {
        /* User has NOPASSWD privileges, skip authentication */
        printf("sudosh: NOPASSWD privileges detected, skipping authentication\n");
        log_authentication(username, 1);  /* Log successful authentication */
    }

    /* Get user information */
    user = get_user_info(username);
    if (!user) {
        fprintf(stderr, "sudosh: unable to get user information\n");
        free(username);
        return EXIT_FAILURE;
    }

    /* Initialize command history logging */
    if (init_command_history(username) != 0) {
        /* Not a fatal error, just warn */
        fprintf(stderr, "Warning: Could not initialize command history logging\n");
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

        /* Log the input command */
        log_session_input(command_line);

        /* Log command to history */
        log_command_history(command_line);

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

    /* Close command history logging */
    close_command_history();

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
    char *session_logfile = NULL;
    int i;

    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options]\n\n", argv[0]);
            printf("sudosh - Interactive sudo shell\n\n");
            printf("Options:\n");
            printf("  -h, --help              Show this help message\n");
            printf("  -v, --version           Show version information\n");
            printf("  -l, --log-session FILE  Log entire session to FILE\n\n");
            printf("sudosh provides an interactive shell with sudo privileges.\n");
            printf("All commands are authenticated and logged to syslog.\n");
            printf("Use -l to also log the complete session to a file.\n");
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("sudosh %s\n", SUDOSH_VERSION);
            printf("Interactive sudo shell with command logging\n");
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--log-session") == 0 || strcmp(argv[i], "-l") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "sudosh: option '%s' requires an argument\n", argv[i]);
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return EXIT_FAILURE;
            }
            session_logfile = argv[++i];
        } else {
            fprintf(stderr, "sudosh: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* Initialize logging */
    init_logging();

    /* Initialize session logging if requested */
    if (session_logfile) {
        if (init_session_logging(session_logfile) != 0) {
            fprintf(stderr, "sudosh: failed to initialize session logging to '%s'\n", session_logfile);
            return EXIT_FAILURE;
        }
        printf("Session logging enabled to: %s\n", session_logfile);
    }

    /* Initialize security measures */
    init_security();

    /* Run main program loop */
    exit_code = main_loop();

    /* Close session logging */
    close_session_logging();

    /* Clean up and exit */
    cleanup_and_exit(exit_code);

    return exit_code; /* Should never reach here */
}
