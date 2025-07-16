/**
 * sudosh.h - Secure Interactive Sudo Shell
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * This file is part of sudosh, a secure interactive shell that provides
 * elevated privileges with extensive logging, security protections, and
 * audit capabilities.
 */

#ifndef SUDOSH_H
#define SUDOSH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <termios.h>
#include <sys/resource.h>
#include <ctype.h>
#include <sys/stat.h>
#include <limits.h>
#include <time.h>
#include <sys/select.h>
#include <dirent.h>
#include <sys/file.h>
#include <fcntl.h>

#ifndef MOCK_AUTH
#include <security/pam_appl.h>
#endif

/* Platform-specific includes and compatibility */
#ifdef __linux__
    /* Linux-specific includes */
    #include <sys/prctl.h>
#elif defined(__APPLE__) && defined(__MACH__)
    /* macOS-specific includes */
    #include <sys/sysctl.h>
    #include <libproc.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    /* BSD-specific includes */
    #include <sys/sysctl.h>
#endif

/* External environment variable */
extern char **environ;

/* Global verbose flag */
extern int verbose_mode;

/* Global test mode flag */
extern int test_mode;

/* Configuration constants */
#define MAX_COMMAND_LENGTH 4096
#define MAX_USERNAME_LENGTH 256
#define MAX_PASSWORD_LENGTH 256
#define SUDOSH_VERSION "1.6.0"
#define INACTIVITY_TIMEOUT 300  /* 300 seconds (5 minutes) */

/* Authentication cache constants */
#define AUTH_CACHE_TIMEOUT 900  /* 15 minutes (900 seconds) - same as sudo default */
#define AUTH_CACHE_DIR "/var/run/sudosh"
#define AUTH_CACHE_FILE_PREFIX "auth_cache_"
#define MAX_CACHE_PATH_LENGTH 512

/* Color support constants */
#define MAX_COLOR_CODE_LENGTH 32
#define MAX_PS1_LENGTH 1024

/* ANSI color codes */
#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_DIM "\033[2m"
#define ANSI_UNDERLINE "\033[4m"

/* Standard colors */
#define ANSI_BLACK "\033[30m"
#define ANSI_RED "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BLUE "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN "\033[36m"
#define ANSI_WHITE "\033[37m"

/* Bright colors */
#define ANSI_BRIGHT_BLACK "\033[90m"
#define ANSI_BRIGHT_RED "\033[91m"
#define ANSI_BRIGHT_GREEN "\033[92m"
#define ANSI_BRIGHT_YELLOW "\033[93m"
#define ANSI_BRIGHT_BLUE "\033[94m"
#define ANSI_BRIGHT_MAGENTA "\033[95m"
#define ANSI_BRIGHT_CYAN "\033[96m"
#define ANSI_BRIGHT_WHITE "\033[97m"

/* Platform-specific compatibility macros */
#ifndef PATH_MAX
    #ifdef __APPLE__
        #define PATH_MAX 1024
    #else
        #define PATH_MAX 4096
    #endif
#endif

/* PAM constants for systems that might not have them */
#ifndef PAM_MAX_NUM_MSG
    #define PAM_MAX_NUM_MSG 32
#endif

/* Sudoers file paths */
#define SUDOERS_PATH "/etc/sudoers"
#define SUDOERS_DIR "/etc/sudoers.d"

/* Exit codes */
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define EXIT_AUTH_FAILURE 2
#define EXIT_COMMAND_NOT_FOUND 127

/* Logging priorities */
#define LOG_COMMAND LOG_NOTICE
#define LOG_AUTH_SUCCESS LOG_INFO
#define LOG_AUTH_FAILURE LOG_WARNING
#define LOG_ERROR LOG_ERR

/* Structure to hold user information */
struct user_info {
    uid_t uid;
    gid_t gid;
    char *username;
    char *home_dir;
    char *shell;
};

/* Structure to hold command information */
struct command_info {
    char *command;
    char **argv;
    int argc;
    char **envp;
};

/* Structure to hold color configuration */
struct color_config {
    char username_color[MAX_COLOR_CODE_LENGTH];
    char hostname_color[MAX_COLOR_CODE_LENGTH];
    char path_color[MAX_COLOR_CODE_LENGTH];
    char prompt_color[MAX_COLOR_CODE_LENGTH];
    char reset_color[MAX_COLOR_CODE_LENGTH];
    int colors_enabled;
};

/* Structure to hold authentication cache information */
struct auth_cache {
    char username[MAX_USERNAME_LENGTH];
    time_t timestamp;
    pid_t session_id;
    uid_t uid;
    gid_t gid;
    char tty[64];
    char hostname[256];
};

/* NSS source types */
enum nss_source_type {
    NSS_SOURCE_FILES,
    NSS_SOURCE_SSSD,
    NSS_SOURCE_LDAP,
    NSS_SOURCE_UNKNOWN
};

/* NSS configuration entry */
struct nss_source {
    enum nss_source_type type;
    char *name;
    struct nss_source *next;
};

/* NSS configuration */
struct nss_config {
    struct nss_source *passwd_sources;
    struct nss_source *sudoers_sources;
};

/* Sudoers user specification */
struct sudoers_userspec {
    char **users;           /* List of users this rule applies to */
    char **hosts;           /* List of hosts this rule applies to */
    char **commands;        /* List of commands allowed */
    int nopasswd;           /* Whether password is required */
    char *runas_user;       /* User to run as (default: root) */
    char *source_file;      /* Source file where this rule was found */
    struct sudoers_userspec *next;
};

/* Sudoers configuration */
struct sudoers_config {
    struct sudoers_userspec *userspecs;
    char *includedir;       /* Directory for included files */
};

/* Function prototypes */

/* Authentication functions */
int authenticate_user(const char *username);
int authenticate_user_cached(const char *username);
#ifndef MOCK_AUTH
int pam_conversation(int num_msg, const struct pam_message **msg,
                    struct pam_response **resp, void *appdata_ptr);
#endif
char *get_password(const char *prompt);

/* Authentication cache functions */
int check_auth_cache(const char *username);
int update_auth_cache(const char *username);
void clear_auth_cache(const char *username);
void cleanup_auth_cache(void);
char *get_auth_cache_path(const char *username);
int create_auth_cache_dir(void);

/* User management functions */
struct user_info *get_user_info(const char *username);
void free_user_info(struct user_info *user);
int check_sudo_privileges(const char *username);
int check_sudo_privileges_fallback(const char *username);
int check_sudo_privileges_enhanced(const char *username);
int check_nopasswd_privileges_enhanced(const char *username);
int check_nopasswd_sudo_l(const char *username);

/* NSS configuration functions */
struct nss_config *read_nss_config(void);
void free_nss_config(struct nss_config *config);
struct user_info *get_user_info_nss(const char *username, struct nss_config *nss_config);
struct nss_source *create_nss_source(const char *name);

/* Sudoers parsing functions */
struct sudoers_config *parse_sudoers_file(const char *filename);
void free_sudoers_config(struct sudoers_config *config);
int check_sudoers_privileges(const char *username, const char *hostname, struct sudoers_config *sudoers);
int check_sudoers_nopasswd(const char *username, const char *hostname, struct sudoers_config *sudoers);

/* SSSD integration functions */
int check_sssd_privileges(const char *username);
struct user_info *get_user_info_sssd(const char *username);

/* Enhanced privilege checking */
int check_sudo_privileges_enhanced(const char *username);
int check_command_permission(const char *username, const char *command);

/* List available commands */
void list_available_commands(const char *username);

/* Command execution functions */
int parse_command(const char *input, struct command_info *cmd);
int execute_command(struct command_info *cmd, struct user_info *user);
char *find_command_in_path(const char *command);
void free_command_info(struct command_info *cmd);

/* Logging functions */
void init_logging(void);
void log_command(const char *username, const char *command, int success);
void log_authentication(const char *username, int success);
void log_session_start(const char *username);
void log_session_end(const char *username);
void log_error(const char *message);
void log_security_violation(const char *username, const char *violation);
void close_logging(void);

/* Session logging functions */
int init_session_logging(const char *logfile);
void log_session_input(const char *input);
void log_session_output(const char *output);
void close_session_logging(void);

/* Command history functions */
int init_command_history(const char *username);
void log_command_history(const char *command);
void close_command_history(void);

/* History navigation functions */
int load_history_buffer(void);
void free_history_buffer(void);
char *get_history_entry(int index);
int get_history_count(void);
char *expand_history(const char *command);
void add_to_history_buffer(const char *command);

/* Security functions */
void sanitize_environment(void);
void setup_signal_handlers(void);
void set_current_username(const char *username);
void drop_privileges(void);
int check_privileges(void);
void secure_terminal(void);
int validate_command(const char *command);
int validate_command_with_length(const char *command, size_t buffer_len);
void init_security(void);
int is_interrupted(void);
int received_sigint_signal(void);
void reset_sigint_flag(void);
void cleanup_security(void);

/* Enhanced command security functions */
int is_shell_command(const char *command);
int is_ssh_command(const char *command);
int is_interactive_editor(const char *command);
int is_safe_command(const char *command);
int is_dangerous_command(const char *command);
int check_dangerous_flags(const char *command);
int check_system_directory_access(const char *command);
int is_safe_readonly_command(const char *command);
int is_dangerous_system_operation(const char *command);
int is_destructive_archive_operation(const char *command);
int user_has_unrestricted_access(const char *username);
int prompt_user_confirmation(const char *command, const char *warning);

/* Target user functionality */
extern char *target_user;  /* Global target user variable */
int check_runas_permissions(const char *username, const char *target_user);
int validate_target_user(const char *target_user);
struct user_info *get_target_user_info(const char *target_user);

/* Utility functions */
void print_banner(void);
void print_help(void);
void print_commands(void);
void print_history(void);
char *trim_whitespace(char *str);
int is_empty_command(const char *command);
char *read_command(void);
int handle_builtin_command(const char *command);
uid_t get_real_uid(void);
char *get_current_username(void);
struct user_info *get_real_user_info(void);
int is_whitespace_only(const char *str);
char *safe_strdup(const char *str);

/* Color support functions */
struct color_config *init_color_config(void);
void free_color_config(struct color_config *config);
int parse_ps1_colors(const char *ps1, struct color_config *config);
int parse_zsh_prompt_colors(const char *prompt, struct color_config *config);
int detect_terminal_colors(void);
void preserve_color_environment(void);
void cleanup_color_config(void);

/* Tab completion functions */
char **complete_path(const char *text, int start, int end);
char *find_completion_start(const char *buffer, int pos);
void insert_completion(char *buffer, int *pos, int *len, const char *completion, const char *prefix);

/* Main program functions */
int main_loop(void);
void cleanup_and_exit(int exit_code);

#endif /* SUDOSH_H */
