/**
 * main.c - Sudosh Main Program
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Main entry point for sudosh - secure interactive sudo shell with
 * comprehensive logging, security protections, and audit capabilities.
 */

#include "sudosh.h"
#include "dangerous_commands.h"
#include "editor_detection.h"
#include <stdarg.h>

/* Minimal diagnostics to /tmp for test harness debugging */
static void diag_logf(const char *fmt, ...) {
    extern int test_mode;
    if (!test_mode) return;
    FILE *f = fopen("/tmp/sudosh_diag.log", "a");
    if (!f) return;
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    fprintf(f, "[%ld.%09ld pid=%d] ", (long)ts.tv_sec, ts.tv_nsec, getpid());
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f);
    fclose(f);
}

/* Global verbose flag */
int verbose_mode = 0;
/* Global test mode flag (cached at startup) */
int test_mode = 0;

/* Global Ansible detection configuration */
int ansible_detection_enabled = 1;  /* Enabled by default */
int ansible_detection_force = 0;    /* Force detection result */
int ansible_detection_verbose = 0;  /* Verbose Ansible detection output */
/* Shell enhancements config */
int rc_alias_import_enabled = 1;


/* Global detection system variables */
struct ansible_detection_info *global_ansible_info = NULL;
struct ai_detection_info *global_ai_info = NULL;

/* Sudo-compatibility (argv[0]=="sudo") and non-interactive (-n) flags */
int sudo_compat_mode_flag = 0;
int non_interactive_mode_flag = 0;

/**
 * Execute a single command and exit (like sudo)
 */
static int execute_single_command(const char *command_str, const char *target_user) {
    struct user_info *user;
    struct command_info cmd;
    int result;
    char *username;

    /* Initialize logging */
    init_logging();

    /* Initialize security measures */
    init_security();

    /* Initialize file locking system - warn but don't fail */
    if (init_file_locking() != 0) {
        fprintf(stderr, "sudosh: warning: file locking system unavailable\n");
        /* Continue execution - file locking will be checked per-command */
    }

    /* Get current username using getpwuid for reliability */
    struct passwd *pwd = getpwuid(getuid());
    if (!pwd) {
        fprintf(stderr, "sudosh: failed to determine current user\n");
        return EXIT_FAILURE;
    }
    username = safe_strdup(pwd->pw_name);
    if (!username) {
        fprintf(stderr, "sudosh: failed to allocate memory for username\n");
        return EXIT_FAILURE;
    }

    /* Determine effective user - in test mode, use current user; otherwise use target_user or root */
    const char *effective_user;
    diag_logf("-c mode start: cmd='%s' cached_test_mode=%d", command_str, test_mode);
    if (test_mode) {
        /* Test mode: use current user for both authentication and execution */
        effective_user = username;
    } else {
        /* Normal mode: use target_user or default to root */
        effective_user = target_user ? target_user : "root";
    }

    /* Check if user has NOPASSWD privileges, considering command danger and environment */
    int has_nopasswd = check_nopasswd_privileges_with_command(username, command_str);
    diag_logf("-c mode nopasswd=%d", has_nopasswd);

    if (!has_nopasswd) {
        /* Authenticate user - authenticate as current user, but may execute as effective_user */
        int auth_ok = authenticate_user(username);
        diag_logf("-c mode auth_ok=%d", auth_ok);
        if (auth_ok != 1) {
            fprintf(stderr, "sudosh: authentication failed\n");
            free(username);
            return EXIT_FAILURE;
        }
    } else {
        /* User has NOPASSWD privileges, skip authentication */
        if (verbose_mode) {
            printf("sudosh: NOPASSWD privileges detected, skipping authentication\n");
        }
        log_authentication_with_ansible_context(username, 1);  /* Log successful authentication */
    }

    user = get_user_info(effective_user);
    if (!user) {
        fprintf(stderr, "sudosh: failed to get user information\n");
        free(username);
        return EXIT_FAILURE;
    }

    /* Validate command early to honor security policy */
    int valid = validate_command(command_str);
    diag_logf("-c mode validate=%d for cmd='%s'", valid, command_str);
    if (valid == 2) {
        /* Special case: shell command in sudo mode - drop to interactive shell */
        free_user_info(user);
        free(username);
        /* Start interactive shell instead of executing the command */
        return main_loop();
    } else if (!valid) {
        fprintf(stderr, "sudosh: command rejected for security reasons\n");
        free_user_info(user);
        free(username);
        return EXIT_FAILURE;
    }

    /* Parse the command */
    if (parse_command(command_str, &cmd) != 0) {
        fprintf(stderr, "sudosh: failed to parse command: %s\n", command_str);
        free_user_info(user);
        free(username);
        return EXIT_FAILURE;
    }

    /* Log command execution */
    log_command_with_ansible_context(username, command_str, 0);

    /* Execute the command */
    result = execute_command(&cmd, user);

    /* Clean up */
    free_command_info(&cmd);
    free_user_info(user);
    free(username);

    return result;
}

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

    /* Detect if this is an Ansible session */
    if (ansible_detection_enabled) {
        global_ansible_info = detect_ansible_session();

        /* Handle forced Ansible detection */
        if (ansible_detection_force && global_ansible_info) {
            global_ansible_info->is_ansible_session = 1;
            global_ansible_info->method = ANSIBLE_DETECTION_FORCED;
            global_ansible_info->confidence_level = 100;
            strncpy(global_ansible_info->detection_details, "forced via command line",
                   sizeof(global_ansible_info->detection_details) - 1);
        }

        if (global_ansible_info) {
            log_ansible_detection(global_ansible_info);

            if ((verbose_mode || ansible_detection_verbose) && global_ansible_info->is_ansible_session) {
                printf("sudosh: Ansible session detected (%s, confidence: %d%%)\n",
                       global_ansible_info->detection_details,
                       global_ansible_info->confidence_level);
            } else if (ansible_detection_verbose) {
                printf("sudosh: Ansible session not detected (confidence: %d%%)\n",
                       global_ansible_info->confidence_level);
            }
        }
    } else {
        if (ansible_detection_verbose) {
            printf("sudosh: Ansible detection disabled\n");
        }
    }

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
            /* For automation sessions (Ansible) or AI sessions, skip the lecture to avoid cluttering logs */
            if ((!global_ansible_info || !global_ansible_info->is_ansible_session) &&
                (!global_ai_info || !global_ai_info->should_block)) {
                /* Cache miss or expired, show lecture for interactive sessions */
                printf("We trust you have received the usual lecture from the local System\n");
                printf("Administrator. It usually boils down to these three things:\n\n");
                printf("    #1) Respect the privacy of others.\n");
                printf("    #2) Think before you type.\n");
                printf("    #3) With great power comes great responsibility.\n\n");
            }
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
        log_authentication_with_ansible_context(username, 1);  /* Log successful authentication */
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

    /* Initialize shell enhancement systems */
    if (!init_alias_system()) {
        if (verbose_mode) {
            fprintf(stderr, "Warning: Could not initialize alias system\n");
        }
    }

    if (!init_directory_stack()) {
        if (verbose_mode) {
            fprintf(stderr, "Warning: Could not initialize directory stack\n");
        }
    }

    /* Log session start with Ansible context */
    log_session_start_with_ansible_context(username);

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
            /* Silent expansion per Unix philosophy - expansion is logged but not displayed */
            free(command_line);
            command_line = expanded_command;
        }

        /* Expand aliases */
        char *alias_expanded = expand_aliases(command_line);
        if (alias_expanded && strcmp(alias_expanded, command_line) != 0) {
            /* Alias was expanded successfully */
            free(command_line);
            command_line = alias_expanded;
        } else if (alias_expanded) {
            /* No expansion occurred, free the duplicate */
            free(alias_expanded);
        } else {
            /* Alias expansion failed due to security validation */
            fprintf(stderr, "sudosh: alias expansion failed security validation\n");
            free(command_line);
            continue;
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

        /* Check if user has privileges for this specific command */
        /* First check if it's a safe command - these are always allowed */
        if (is_safe_command(command_line)) {
            /* Safe command - allow it regardless of sudo privileges */
        } else if (!has_sudo_privileges) {
            fprintf(stderr, "sudosh: %s is not in the sudoers file and '%s' is not a safe command\n",
                    username, command_line);
            fprintf(stderr, "Available safe commands: ls, pwd, whoami, id, date, uptime, w, who, last\n");
            log_security_violation(username, "attempted privileged command without sudoers access");
            free(command_line);
            continue;
        } else if (!check_command_permission(username, command_line)) {
            /* User has sudo privileges but not for this specific command */
            fprintf(stderr, "sudosh: %s is not allowed to run '%s' according to sudoers configuration\n",
                    username, command_line);
            log_security_violation(username, "attempted command not permitted by sudoers");
            free(command_line);
            continue;
        }

        /* Check if this is a pipeline command */
        if (is_pipeline_command(command_line)) {
            /* Handle pipeline execution */
            struct pipeline_info pipeline;

            if (parse_pipeline(command_line, &pipeline) != 0) {
                fprintf(stderr, "sudosh: failed to parse pipeline\n");
                free(command_line);
                continue;
            }

            /* Execute pipeline */
            result = execute_pipeline(&pipeline, user);

            /* Log pipeline execution */
            if (target_user) {
                char log_message[1024];
                snprintf(log_message, sizeof(log_message), "pipeline: %s (as %s)", command_line, target_user);
                log_command(username, log_message, (result == 0));
            } else {
                char log_message[1024];
                snprintf(log_message, sizeof(log_message), "pipeline: %s", command_line);
                log_command_with_ansible_context(username, log_message, (result == 0));
            }

            /* Clean up pipeline structure */
            free_pipeline_info(&pipeline);
        } else {
            /* Handle regular command execution */

            /* Parse command */
            if (parse_command(command_line, &cmd) != 0) {
                fprintf(stderr, "sudosh: failed to parse command\n");
                free(command_line);
                continue;
            }

            /* Check if this command requires authentication despite NOPASSWD */
            if (should_require_authentication(username, command_line)) {
                /* This command requires authentication in the current environment */
                if (!authenticate_user_cached(username)) {
                    fprintf(stderr, "sudosh: authentication required for command '%s' in editor environment\n", command_line);
                    fprintf(stderr, "sudosh: reason: %s\n", get_danger_explanation(command_line));
                    free_command_info(&cmd);
                    free(command_line);
                    continue;
                }
            }

            /* Check if this is a conditionally blocked command requiring authentication */
            if (is_conditionally_blocked_command(command_line)) {
                /* Check if user has authorization */
                if (!check_conditionally_blocked_command_authorization(username, command_line)) {
                    fprintf(stderr, "sudosh: command '%s' requires sudo privileges\n", command_line);
                    free_command_info(&cmd);
                    free(command_line);
                    continue;
                }

                /* User has authorization, but check if authentication is needed */
                if (!check_nopasswd_privileges_enhanced(username)) {
                    /* User doesn't have NOPASSWD, require authentication */
                    if (!authenticate_user_cached(username)) {
                        fprintf(stderr, "sudosh: authentication required for command '%s'\n", command_line);
                        free_command_info(&cmd);
                        free(command_line);
                        continue;
                    }
                }
            }

            /* Execute command */
            result = execute_command(&cmd, user);

            /* Update last exit status for prompt customization */
            extern int last_exit_status; last_exit_status = result;

            /* Log command execution with target user info and Ansible context */
            if (target_user) {
                char log_message[1024];
                snprintf(log_message, sizeof(log_message), "%s (as %s)", command_line, target_user);
                log_command_with_ansible_context(username, log_message, (result == 0));
            } else {
                log_command_with_ansible_context(username, command_line, (result == 0));
            }

            /* Clean up command structure */
            free_command_info(&cmd);
        }

        free(command_line);
    }

    /* Check if we exited due to interruption */
    if (is_interrupted()) {
        printf("\nInterrupted - exiting gracefully\n");
    }

    /* Log session end */
    log_session_end(username);

    /* Close command history logging */
    close_command_history();

    /* Close session logging */
    close_logging();

    /* Free history buffer */
    free_history_buffer();

    /* Clean up shell enhancement systems */
    cleanup_alias_system();
    cleanup_directory_stack();

    /* Clean up authentication cache */
    cleanup_auth_cache();

    /* Clean up security */
    cleanup_security();

    /* Clean up file locking system */
    cleanup_file_locking();

    /* Clean up */
    free_user_info(user);
    free(username);

    return EXIT_SUCCESS;
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    const char *tm = getenv("SUDOSH_TEST_MODE");
    test_mode = (tm && strcmp(tm, "1") == 0) ? 1 : 0;
    diag_logf("main start argc=%d argv1=%s testmode_env=%s cached=%d", argc, (argc>1?argv[1]:"(none)"), tm?tm:"(null)", test_mode);
    int exit_code;
    char *session_logfile = NULL;
    char *custom_prompt = NULL;
    int i;

    /* sudo-compat mode detection: if invoked as 'sudo', enable compatibility behavior */
    const char *invoked = (argc > 0 ? argv[0] : NULL);
    const char *slash = invoked ? strrchr(invoked, '/') : NULL;
    const char *invoked_name = slash ? slash + 1 : invoked;
    int sudo_compat_mode = (invoked_name && strcmp(invoked_name, "sudo") == 0);
    sudo_compat_mode_flag = sudo_compat_mode;
    /* Early AI detection and blocking - must happen before any other processing */
    struct ai_detection_info *ai_info = detect_ai_session();
    if (ai_info && should_block_ai_session(ai_info)) {
        fprintf(stderr, "\n");
        fprintf(stderr, "=== SECURITY RESTRICTION ===\n");
        fprintf(stderr, "ERROR: %s AI session detected and blocked.\n", ai_info->tool_name);
        fprintf(stderr, "\n");
        fprintf(stderr, "%s is not authorized to perform privileged operations.\n", ai_info->tool_name);
        fprintf(stderr, "This restriction is in place for security reasons.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "If you need to perform administrative tasks:\n");
        fprintf(stderr, "1. Exit the %s session\n", ai_info->tool_name);
        fprintf(stderr, "2. Use sudosh directly from your terminal\n");
        fprintf(stderr, "3. Or use standard sudo for specific commands\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Session details: %s\n", ai_info->detection_details);
        fprintf(stderr, "============================\n");

        /* Log the blocking attempt */
        openlog("sudosh", LOG_PID, LOG_AUTHPRIV);
        log_ai_detection(ai_info);
        closelog();

        free_ai_detection_info(ai_info);
        exit(EXIT_FAILURE);
    }

    /* Store AI detection info for later use */
    global_ai_info = ai_info;

    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options] [command [args...]]\n\n", argv[0]);
            printf("sudosh - Interactive sudo shell and command executor\n\n");
            if (sudo_compat_mode) {
                printf("sudo-compat mode: basic sudo-style invocation supported (subset)\n");
            }

            printf("Options:\n");
            printf("  -h, --help              Show this help message\n");
            printf("      --version           Show version information\n");
            printf("      --build-info        Show detailed build information\n");
            printf("  -v, --verbose           Enable verbose output\n");
            printf("  -l, --list              List available sudo rules and permissions\n");
            printf("  -ll                     List sudo rules with detailed command categories\n");
            printf("  -L, --log-session FILE  Log entire session to FILE\n");
            printf("  -u, --user USER         Run commands as target USER\n");
            printf("  -c, --command COMMAND   Execute COMMAND and exit (like sudo -c)\n");
            if (sudo_compat_mode) {
                printf("  -p, --prompt PROMPT     Use custom password prompt\n");
            }
            printf("      --rc-alias-import   Enable importing aliases from shell rc files (default)\n");
            printf("      --no-rc-alias-import Disable importing aliases from shell rc files\n");
            printf("      --ansible-detect    Enable Ansible session detection (default)\n");
            printf("      --no-ansible-detect Disable Ansible session detection\n");
            printf("      --ansible-force     Force Ansible session mode\n");
            printf("      --ansible-verbose   Enable verbose Ansible detection output\n\n");
            printf("Command Execution:\n");
            printf("  sudosh                  Start interactive shell (default)\n");
            printf("  sudosh command          Execute command and exit\n");
            printf("  sudosh -c \"command\"     Execute command and exit (explicit)\n");
            printf("  sudosh -u user command  Execute command as specific user\n\n");
            printf("sudosh provides both an interactive shell and direct command execution.\n");
            printf("All commands are authenticated and logged to syslog.\n");
            printf("Use -l to list sudo rules and permissions, -ll for detailed command categories.\n");
            printf("Use -L to also log the complete session to a file.\n");
            printf("Use -v for verbose output including privilege detection details.\n");
            printf("Use -u to run commands as a specific target user (requires sudoers permission).\n");
            return EXIT_SUCCESS;

        } else if (strcmp(argv[i], "--rc-alias-import") == 0) {
            rc_alias_import_enabled = 1;
        } else if (strcmp(argv[i], "--no-rc-alias-import") == 0) {
            rc_alias_import_enabled = 0;
        } else if (strcmp(argv[i], "--ansible-detect") == 0) {
            ansible_detection_enabled = 1;
        } else if (strcmp(argv[i], "--no-ansible-detect") == 0) {
            ansible_detection_enabled = 0;
        } else if (strcmp(argv[i], "--ansible-force") == 0) {
            ansible_detection_force = 1;
        } else if (strcmp(argv[i], "--ansible-verbose") == 0) {
            ansible_detection_verbose = 1;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("sudosh %s\n", SUDOSH_VERSION);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-V") == 0 && sudo_compat_mode) {
            /* sudo-compat: -V (version) */
            printf("sudosh %s\n", SUDOSH_VERSION);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--build-info") == 0) {
            const char *git = getenv("SUDOSH_BUILD_GIT");
            const char *date = getenv("SUDOSH_BUILD_DATE");
            const char *user = getenv("SUDOSH_BUILD_USER");
            if (!git) git = "unknown";
            if (!date) date = "unknown";
            if (!user) user = "unknown";
            printf("sudosh %s\n", SUDOSH_VERSION);
            printf("build: git=%s date=%s by=%s\n", git, date, user);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-v") == 0 && sudo_compat_mode) {
            /* sudo-compat: -v = validate/update timestamp (no command execution) */
            char *username = get_current_username();
            if (username) {
                if (check_auth_cache(username)) {
                    update_auth_cache(username);
                    if (verbose_mode) printf("sudosh: refreshed authentication cache for %s\n", username);
                } else {
                    if (!non_interactive_mode_flag) {
                        if (authenticate_user(username)) {
                            update_auth_cache(username);
                            if (verbose_mode) printf("sudosh: authenticated and cached for %s\n", username);
                        } else {
                            fprintf(stderr, "sudosh: authentication failed\n");
                            free(username);
                            return EXIT_AUTH_FAILURE;
                        }
                    } else {
                        fprintf(stderr, "sudosh: non-interactive mode prevents authentication\n");
                        free(username);
                        return EXIT_AUTH_FAILURE;
                    }
                }
                free(username);
            }
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--verbose") == 0 || (strcmp(argv[i], "-v") == 0 && !sudo_compat_mode)) {
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

            /* Check if this is -ll for detailed output */
            if (strcmp(argv[i], "-ll") == 0) {
                list_available_commands_detailed(username);
            } else {
                list_available_commands_basic(username);
            }
            free(username);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-ll") == 0) {
            /* List available commands with detailed categories and exit */
            char *username = get_current_username();
            if (!username) {
                fprintf(stderr, "sudosh: unable to determine current user\n");
                return EXIT_FAILURE;
            }

            /* Initialize logging for any security violations */
            init_logging();

            list_available_commands_detailed(username);
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
        } else if (strcmp(argv[i], "--ansible-detect") == 0) {
            ansible_detection_enabled = 1;
        } else if (strcmp(argv[i], "--no-ansible-detect") == 0) {
            ansible_detection_enabled = 0;
        } else if (strcmp(argv[i], "--ansible-force") == 0) {
            ansible_detection_force = 1;
            ansible_detection_enabled = 1;  /* Force implies enabled */
        } else if (strcmp(argv[i], "--ansible-verbose") == 0) {
            ansible_detection_verbose = 1;
        } else if (strcmp(argv[i], "-n") == 0) {
            /* Non-interactive: refuse any auth prompt (compat with sudo -n) */
            non_interactive_mode_flag = 1;
        } else if (strcmp(argv[i], "-k") == 0) {
            /* Invalidate cached authentication and exit */
            char *username = get_current_username();
            if (username) {
                clear_auth_cache(username);
                if (verbose_mode) printf("sudosh: cleared authentication cache for %s\n", username);
                free(username);
            }
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-c") == 0) {
            /* -c command: execute command and exit (sudo-compat: matches common sudo usage) */
            if (i + 1 >= argc) {
                fprintf(stderr, "sudosh: option '%s' requires an argument\n", argv[i]);
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return EXIT_FAILURE;
            }
            const char *cmd = argv[++i];
            /* In sudo-compat mode we still enforce sudosh mitigations: no direct shells, redirection, or injection */
            diag_logf("argv -c path: handing off to execute_single_command compat=%d", sudo_compat_mode);
            return execute_single_command(cmd, target_user);
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--prompt") == 0) {
            /* Custom password prompt (sudo -p compatibility) */
            if (i + 1 >= argc) {
                fprintf(stderr, "sudosh: option '%s' requires an argument\n", argv[i]);
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return EXIT_FAILURE;
            }
            custom_prompt = argv[++i];
            set_custom_password_prompt(custom_prompt);
        } else if (sudo_compat_mode && (
                   strcmp(argv[i], "-E") == 0 || strcmp(argv[i], "-H") == 0 ||
                   strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "-s") == 0 ||
                   strcmp(argv[i], "-A") == 0 || strcmp(argv[i], "-S") == 0 ||
                   strcmp(argv[i], "-b") == 0)) {
            fprintf(stderr, "sudosh: option '%s' is unsupported in sudo-compat mode (security policy)\n", argv[i]);
            fprintf(stderr, "See '%s --help' for supported options.\n", argv[0]);
            return EXIT_FAILURE;
        } else if (argv[i][0] != '-') {
            /* Non-option argument: treat as command to execute (sudo compat: bare 'sudo cmd') */
            /* Build command from remaining arguments */
            char command_buffer[4096] = {0};
            for (int j = i; j < argc; j++) {
                if (j > i) strncat(command_buffer, " ", sizeof(command_buffer) - strlen(command_buffer) - 1);
                strncat(command_buffer, argv[j], sizeof(command_buffer) - strlen(command_buffer) - 1);
            }
            /* Execute the command directly and exit */
            diag_logf("argv non-option path: handing off to execute_single_command compat=%d", sudo_compat_mode);
            return execute_single_command(command_buffer, target_user);
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

        /* Load configuration file if present to toggle features */
        sudosh_config_t *cfg = sudosh_config_init();
        if (cfg) {
            /* Try common config paths; optional */
            const char *paths[] = { "/etc/sudosh.conf", "/usr/local/etc/sudosh.conf", NULL };
            for (int pi = 0; paths[pi]; ++pi) {
                sudosh_config_load(cfg, paths[pi]);
            }
            /* Apply shell enhancements related config */
            rc_alias_import_enabled = cfg->rc_alias_import_enabled;
            sudosh_config_free(cfg);

        }
        /* Session logging enabled silently - logged to syslog for audit */
    }

    /* Initialize security measures */
    init_security();

    /* Initialize file locking system - warn but don't fail */
    if (init_file_locking() != 0) {
        fprintf(stderr, "sudosh: warning: file locking system unavailable\n");
        /* Continue execution - file locking will be checked per-command */
    }

    /* Run main program loop */
    exit_code = main_loop();

    /* Close session logging */
    close_session_logging();

    /* Clean up and exit */
    cleanup_and_exit(exit_code);

    return exit_code; /* Should never reach here */
}
