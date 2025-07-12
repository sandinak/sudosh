/**
 * main.c - Sudosh Main Program
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Main entry point for sudosh - secure interactive sudo shell with
 * comprehensive logging, security protections, and audit capabilities.
 */

#include "sudosh.h"

/* Global verbose flag */
int verbose_mode = 0;

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
    int has_sudo_privileges = check_sudo_privileges_enhanced(username);
    if (!has_sudo_privileges) {
        printf("Note: You have limited privileges. Only safe commands (ls, pwd, whoami, etc.) are available.\n");
        log_security_violation(username, "user not in sudoers - limited to safe commands");
    }

    /* If target user specified, validate and check permissions */
    if (target_user) {
        if (!validate_target_user(target_user)) {
            fprintf(stderr, "sudosh: invalid target user '%s'\n", target_user);
            free(username);
            return EXIT_FAILURE;
        }

        if (!check_runas_permissions(username, target_user)) {
            fprintf(stderr, "sudosh: %s is not allowed to run commands as %s\n", username, target_user);
            log_security_violation(username, "unauthorized target user access attempt");
            free(username);
            return EXIT_AUTH_FAILURE;
        }

        if (verbose_mode) {
            printf("sudosh: validated permission to run commands as user '%s'\n", target_user);
        }
    }

    /* Check if user has NOPASSWD privileges */
    int has_nopasswd = check_nopasswd_privileges_enhanced(username);

    if (!has_nopasswd) {
        /* Check authentication cache first, then authenticate if needed */
        if (!check_auth_cache(username)) {
            /* Cache miss or expired, show lecture and authenticate */
            printf("We trust you have received the usual lecture from the local System\n");
            printf("Administrator. It usually boils down to these three things:\n\n");
            printf("    #1) Respect the privacy of others.\n");
            printf("    #2) Think before you type.\n");
            printf("    #3) With great power comes great responsibility.\n\n");
        }

        if (!authenticate_user_cached(username)) {
            fprintf(stderr, "sudosh: authentication failed\n");
            free(username);
            return EXIT_AUTH_FAILURE;
        }
    } else {
        /* User has NOPASSWD privileges, skip authentication */
        if (verbose_mode) {
            printf("sudosh: NOPASSWD privileges detected, skipping authentication\n");
        }
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

    /* Load command history for navigation */
    if (load_history_buffer() != 0) {
        /* Not a fatal error, just warn */
        if (verbose_mode) {
            fprintf(stderr, "Warning: Could not load command history for navigation\n");
        }
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

        /* Expand history references (e.g., !1, !42) */
        char *expanded_command = expand_history(command_line);
        if (expanded_command) {
            /* Show the expanded command if it's different */
            if (strcmp(command_line, expanded_command) != 0) {
                printf("Expanded: %s\n", expanded_command);
            }
            free(command_line);
            command_line = expanded_command;
        }

        /* Log command to history */
        log_command_history(command_line);

        /* Add command to in-memory history buffer for immediate arrow key access */
        add_to_history_buffer(command_line);

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

        /* Check if user has privileges for this command */
        if (!has_sudo_privileges && !is_safe_command(command_line)) {
            fprintf(stderr, "sudosh: %s is not in the sudoers file and '%s' is not a safe command\n",
                    username, command_line);
            fprintf(stderr, "Available safe commands: ls, pwd, whoami, id, date, uptime, w, who\n");
            log_security_violation(username, "attempted privileged command without sudoers access");
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

        /* Log command execution with target user info */
        if (target_user) {
            char log_message[1024];
            snprintf(log_message, sizeof(log_message), "%s (as %s)", command_line, target_user);
            log_command(username, log_message, (result == 0));
        } else {
            log_command(username, command_line, (result == 0));
        }

        /* Clean up command structure */
        free_command_info(&cmd);
        free(command_line);
    }

    /* Check if we exited due to interruption */
    if (is_interrupted()) {
        printf("\nInterrupted - exiting gracefully\n");
    } else {
        printf("\nExiting sudosh\n");
    }

    /* Log session end */
    log_session_end(username);

    /* Close command history logging */
    close_command_history();

    /* Close session logging */
    close_logging();

    /* Free history buffer */
    free_history_buffer();

    /* Clean up authentication cache */
    cleanup_auth_cache();

    /* Clean up security */
    cleanup_security();

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
            printf("      --version           Show version information\n");
            printf("  -v, --verbose           Enable verbose output\n");
            printf("  -l, --list              List available commands showing each permission source\n");
            printf("  -L, --log-session FILE  Log entire session to FILE\n");
            printf("  -u, --user USER         Run commands as target USER\n\n");
            printf("sudosh provides an interactive shell with sudo privileges.\n");
            printf("All commands are authenticated and logged to syslog.\n");
            printf("Use -l to list available commands showing each permission source separately.\n");
            printf("Use -L to also log the complete session to a file.\n");
            printf("Use -v for verbose output including privilege detection details.\n");
            printf("Use -u to run commands as a specific target user (requires sudoers permission).\n");
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("sudosh %s\n", SUDOSH_VERSION);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose_mode = 1;
        } else if (strcmp(argv[i], "--list") == 0 || strcmp(argv[i], "-l") == 0) {
            /* List available commands and exit */
            char *username = get_current_username();
            if (!username) {
                fprintf(stderr, "sudosh: unable to determine current user\n");
                return EXIT_FAILURE;
            }

            /* Initialize logging for any security violations */
            init_logging();

            list_available_commands(username);
            free(username);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--log-session") == 0 || strcmp(argv[i], "-L") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "sudosh: option '%s' requires an argument\n", argv[i]);
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return EXIT_FAILURE;
            }
            session_logfile = argv[++i];
        } else if (strcmp(argv[i], "--user") == 0 || strcmp(argv[i], "-u") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "sudosh: option '%s' requires an argument\n", argv[i]);
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return EXIT_FAILURE;
            }
            target_user = argv[++i];
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
