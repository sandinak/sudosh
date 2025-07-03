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

#ifndef MOCK_AUTH
#include <security/pam_appl.h>
#endif

/* External environment variable */
extern char **environ;

/* Global verbose flag */
extern int verbose_mode;

/* Configuration constants */
#define MAX_COMMAND_LENGTH 4096
#define MAX_USERNAME_LENGTH 256
#define MAX_PASSWORD_LENGTH 256
#define SUDOSH_VERSION "1.3.0"
#define INACTIVITY_TIMEOUT 300  /* 300 seconds (5 minutes) */

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
#ifndef MOCK_AUTH
int pam_conversation(int num_msg, const struct pam_message **msg,
                    struct pam_response **resp, void *appdata_ptr);
#endif
char *get_password(const char *prompt);

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

/* Tab completion functions */
char **complete_path(const char *text, int start, int end);
char *find_completion_start(const char *buffer, int pos);
void insert_completion(char *buffer, int *pos, int *len, const char *completion, const char *prefix);

/* Main program functions */
int main_loop(void);
void cleanup_and_exit(int exit_code);

#endif /* SUDOSH_H */
