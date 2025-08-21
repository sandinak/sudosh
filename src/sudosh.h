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
#include <fcntl.h>
#include "ai_detection.h"
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

/* Include common utilities and error handling */
#include "sudosh_common.h"

/* External environment variable - declared in unistd.h */
extern char **environ;

/* Global verbose flag */
extern int verbose_mode;

/* Global test mode flag */
extern int test_mode;

/* Global sudo compatibility mode flag */
extern int sudo_compat_mode_flag;

/* Last command exit status for prompt customization */
extern int last_exit_status;

/* Shell enhancements configuration */
extern int rc_alias_import_enabled; /* default enabled; can be toggled via CLI or config */

/* Global detection system results */
extern struct ansible_detection_info *global_ansible_info;
extern struct ai_detection_info *global_ai_info;

/* Global Ansible detection configuration */
extern int ansible_detection_enabled;
extern int ansible_detection_force;
extern int ansible_detection_verbose;

/* Sudo-compat globals (defined in main.c) */
extern int non_interactive_mode_flag; /* -n: non-interactive (no prompts) */
extern int sudo_compat_mode_flag;     /* argv[0]=="sudo" */

/* Configuration constants */
#define MAX_COMMAND_LENGTH 4096
#define MAX_USERNAME_LENGTH 256
#define MAX_PASSWORD_LENGTH 256
#ifndef SUDOSH_VERSION
#define SUDOSH_VERSION "1.9.4"
#endif
#define INACTIVITY_TIMEOUT 300  /* 300 seconds (5 minutes) */

/* File locking constants */
#define LOCK_DIR "/var/run/sudosh/locks"
#define LOCK_TIMEOUT 1800  /* 30 minutes (1800 seconds) */
#define MAX_LOCK_PATH_LENGTH 512
#define LOCK_FILE_EXTENSION ".lock"

/* Authentication cache constants */
#define AUTH_CACHE_TIMEOUT 900  /* 15 minutes (900 seconds) - same as sudo default */
#define AUTH_CACHE_DIR "/var/run/sudosh"
#define AUTH_CACHE_FILE_PREFIX "auth_cache_"
#define MAX_CACHE_PATH_LENGTH 512

/* Color support constants */
#define MAX_COLOR_CODE_LENGTH 32
#define MAX_PS1_LENGTH 1024

/* Shell enhancement constants */
#define MAX_ALIAS_NAME_LENGTH 64
#define MAX_ALIAS_VALUE_LENGTH 1024

/* Ansible detection constants */
#define MAX_PROCESS_NAME_LENGTH 256
#define MAX_PROCESS_TREE_DEPTH 10
#define ANSIBLE_ENV_VAR_COUNT 20
#define MAX_ALIASES 256
#define ALIAS_FILE_NAME ".sudosh_aliases"
#define MAX_DIR_STACK_DEPTH 32

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

/* Redirection types */
typedef enum {
    REDIRECT_NONE = 0,
    REDIRECT_INPUT,
    REDIRECT_OUTPUT,
    REDIRECT_OUTPUT_APPEND
} redirect_type_t;

/* Structure to hold command information */
struct command_info {
    char *command;
    char **argv;
    int argc;
    char **envp;
    /* Redirection support */
    redirect_type_t redirect_type;
    char *redirect_file;
    int redirect_append;
};

/* Structure to hold pipeline information */
struct pipeline_command {
    struct command_info cmd;
    int input_fd;   /* File descriptor for input */
    int output_fd;  /* File descriptor for output */
    pid_t pid;      /* Process ID when running */
};

struct pipeline_info {
    struct pipeline_command *commands;
    int num_commands;
    int *pipe_fds;  /* Array of pipe file descriptors */
    int num_pipes;  /* Number of pipes (num_commands - 1) */
};

/* Structure to hold file lock information */
struct file_lock_info {
    char *file_path;        /* Canonical path of the locked file */
    char *username;         /* User who owns the lock */
    pid_t pid;              /* Process ID of the editing session */
    time_t timestamp;       /* When the lock was acquired */
    char *lock_file_path;   /* Path to the lock file */
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

/* Structure to hold alias information */
struct alias_entry {
    char *name;
    char *value;
    struct alias_entry *next;
};

/* Structure to hold directory stack for pushd/popd */
struct dir_stack_entry {
    char *path;
    struct dir_stack_entry *next;
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

/* Ansible detection result types */
enum ansible_detection_method {
    ANSIBLE_NOT_DETECTED = 0,
    ANSIBLE_DETECTED_ENV_VAR,
    ANSIBLE_DETECTED_PARENT_PROCESS,
    ANSIBLE_DETECTED_CONTEXT,
    ANSIBLE_DETECTED_HEURISTIC,
    ANSIBLE_DETECTION_FORCED
};

/* Ansible detection information */
struct ansible_detection_info {
    enum ansible_detection_method method;
    int confidence_level;           /* 0-100, higher = more confident */
    char detected_env_vars[ANSIBLE_ENV_VAR_COUNT][64];
    int env_var_count;
    char parent_process_name[MAX_PROCESS_NAME_LENGTH];
    pid_t parent_pid;
    char detection_details[512];    /* Human-readable detection details */
    time_t detection_time;
    int is_ansible_session;         /* Final boolean result */
    char automation_type[32];       /* "ansible" or "unknown" */
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
int check_global_nopasswd_privileges_enhanced(const char *username);
int check_nopasswd_sudo_l(const char *username);

/* NSS configuration functions */
struct nss_config *read_nss_config(void);
void free_nss_config(struct nss_config *config);
struct user_info *get_user_info_nss(const char *username, struct nss_config *nss_config);
struct nss_source *create_nss_source(const char *name);

/* Enhanced NSS functions without sudo dependency */
struct user_info *get_user_info_files(const char *username);
struct user_info *get_user_info_sssd_direct(const char *username);
struct user_info *get_user_info_sssd_socket(const char *username);
int check_admin_groups_files(const char *username);
int check_admin_groups_getgrnam(const char *username);
int check_admin_groups_sssd_direct(const char *username);
int check_sssd_groups_socket(const char *username);
int check_sudo_privileges_nss(const char *username);
int check_command_permission_nss(const char *username, const char *command);

/* Sudoers parsing functions */
struct sudoers_config *parse_sudoers_file(const char *filename);
void free_sudoers_config(struct sudoers_config *config);
int check_sudoers_privileges(const char *username, const char *hostname, struct sudoers_config *sudoers);
int check_sudoers_nopasswd(const char *username, const char *hostname, struct sudoers_config *sudoers);
int check_sudoers_global_nopasswd(const char *username, const char *hostname, struct sudoers_config *sudoers);
int check_sudoers_command_permission(const char *username, const char *hostname, const char *command, struct sudoers_config *sudoers);

/* SSSD integration functions */
int check_sssd_privileges(const char *username);
struct user_info *get_user_info_sssd(const char *username);

/* Enhanced privilege checking - already declared above */
/* int check_sudo_privileges_enhanced(const char *username); */
int check_command_permission(const char *username, const char *command);
int check_command_permission_sudo_fallback(const char *username, const char *command);

/* Enhanced authentication functions for editor environments */
int should_require_authentication(const char *username, const char *command);
int check_nopasswd_privileges_with_command(const char *username, const char *command);

/* List available commands */
void list_available_commands(const char *username);
void print_safe_commands_section(void);
void print_blocked_commands_section(void);

/* Pager support */
int get_terminal_height(void);
void execute_with_pager(void (*func)(const char*), const char *arg);

/* Command execution functions */
int parse_command(const char *input, struct command_info *cmd);
int execute_command(struct command_info *cmd, struct user_info *user);
char *find_command_in_path(const char *command);
char *expand_equals_expression(const char *arg);
void free_command_info(struct command_info *cmd);
int validate_ansible_command(const char *command, const char *username);
int check_sudo_command_allowed(const char *username, const char *command);

/* Shell operator parsing functions */
int contains_shell_operators(const char *input);
int parse_command_with_shell_operators(const char *input, struct command_info *cmd);
int parse_command_with_redirection(const char *input, struct command_info *cmd);
int tokenize_command_line(const char *input, char ***argv, int *argc, int *argv_size);
char *trim_whitespace_inplace(char *str);

/* Pipeline execution functions */
int is_pipeline_command(const char *input);
int parse_pipeline(const char *input, struct pipeline_info *pipeline);
int execute_pipeline(struct pipeline_info *pipeline, struct user_info *user);
void free_pipeline_info(struct pipeline_info *pipeline);
int is_whitelisted_pipe_command(const char *command);
int is_secure_pager_command(const char *command);
void setup_secure_pager_environment(void);
int validate_pipeline_security(struct pipeline_info *pipeline);
int validate_pipeline_with_permissions(struct pipeline_info *pipeline, const char *username);
void log_pipeline_start(struct pipeline_info *pipeline);
void log_pipeline_completion(struct pipeline_info *pipeline, int exit_code);

/* Logging functions */
void init_logging(void);
void log_command(const char *username, const char *command, int success);
void log_authentication(const char *username, int success);
void log_session_start(const char *username);
void log_session_end(const char *username);
/* void log_error(const char *message); */ /* Already declared in sudosh_common.h */
void log_security_violation(const char *username, const char *violation);

/* Ansible-aware logging functions */
void log_command_with_ansible_context(const char *username, const char *command, int exit_status);
void log_authentication_with_ansible_context(const char *username, int success);
void log_session_start_with_ansible_context(const char *username);
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
int validate_secure_pipeline(const char *command);
int validate_command_for_pipeline(const char *command);
int validate_safe_redirection(const char *command);
int is_safe_redirection_target(const char *target);
const char *get_redirection_error_message(const char *target);
int is_text_processing_command(const char *cmd_name);
int validate_text_processing_command(const char *command);
void init_security(void);
int is_interrupted(void);
int received_sigint_signal(void);
void reset_sigint_flag(void);
void cleanup_security(void);

/* Enhanced command security functions */
int is_sudoedit_command(const char *command);
int is_shell_command(const char *command);
int handle_shell_command_in_sudo_mode(const char *command);
int is_ssh_command(const char *command);
int is_secure_pager(const char *command);
void setup_secure_pager_environment(void);
int is_secure_editor(const char *command);
void setup_secure_editor_environment(void);
int is_interactive_editor(const char *command);
int is_safe_command(const char *command);
int is_dangerous_command(const char *command);
int is_system_control_command(const char *command);
int is_disk_operations_command(const char *command);
int is_network_security_command(const char *command);
int is_communication_command(const char *command);
int is_privilege_escalation_command(const char *command);
int is_conditionally_blocked_command(const char *command);
int check_conditionally_blocked_command_authorization(const char *username, const char *command);
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

/* File locking functions */
int init_file_locking(void);
void cleanup_file_locking(void);
int is_file_locking_available(void);
int acquire_file_lock(const char *file_path, const char *username, pid_t pid);
int release_file_lock(const char *file_path, const char *username, pid_t pid);
struct file_lock_info *check_file_lock(const char *file_path);
void free_file_lock_info(struct file_lock_info *lock_info);
char *resolve_canonical_path(const char *file_path);
int cleanup_stale_locks(void);
int is_editing_command(const char *command);
char *extract_file_argument(const char *command);

/* Utility functions */
void print_banner(void);
void print_help(void);
void print_commands(void);
void print_history(void);
void print_path_info(void);
int validate_path_security(const char *path_env);
char *trim_whitespace(char *str);
int is_empty_command(const char *command);
char *read_command(void);
int handle_builtin_command(const char *command);
uid_t get_real_uid(void);
char *get_current_username(void);
struct user_info *get_real_user_info(void);
int is_whitespace_only(const char *str);
char *safe_strdup(const char *str);

/* History search helper for reverse-i-search (non-interactive core) */
int history_search_last_index(const char *needle);

/* Color support functions */
struct color_config *init_color_config(void);
void free_color_config(struct color_config *config);
int parse_ps1_colors(const char *ps1, struct color_config *config);
int parse_zsh_prompt_colors(const char *prompt, struct color_config *config);
int detect_terminal_colors(void);
void preserve_color_environment(void);
void cleanup_color_config(void);

/* Tab completion functions */
char *get_directory_context_for_empty_prefix(const char *buffer, int pos);
int is_cd_command(const char *buffer, int pos);
char **complete_path(const char *text, int start, int end, int executables_only, int directories_only);
char **complete_equals_expansion(const char *text);
char **complete_command(const char *text);
int is_command_position(const char *buffer, int pos);
char *find_completion_start(const char *buffer, int pos);
void insert_completion(char *buffer, int *pos, int *len, const char *completion, const char *prefix);
int get_terminal_width(void);
void display_matches_in_columns(char **matches);

/* Shell enhancement functions */
/* Alias management */
int init_alias_system(void);
void cleanup_alias_system(void);
int add_alias(const char *name, const char *value);
int remove_alias(const char *name);
char *get_alias_value(const char *name);
void print_aliases(void);
int load_aliases_from_file(void);
int load_aliases_from_shell_rc_files(void);

int save_aliases_to_file(void);
char *expand_aliases(const char *command);
char *expand_aliases_internal(const char *command);
int validate_alias_name(const char *name);
int validate_alias_value(const char *value);
int check_dangerous_alias_patterns(const char *alias_name, const char *alias_value);
int validate_alias_expansion_safety(const char *alias_name, const char *alias_value);
int validate_expanded_alias_command(const char *expanded_command, const char *alias_name, const char *alias_value);
/* Iterate alias names matching a prefix; returns count found */
int alias_iterate_names_with_prefix(const char *prefix, int (*cb)(const char *name, void *), void *ctx);

/* Directory stack management */
int init_directory_stack(void);
void cleanup_directory_stack(void);
int pushd(const char *dir);
int popd(void);
void print_dirs(void);

/* Environment variable management */
int handle_export_command(const char *command);
int handle_unset_command(const char *command);
void print_environment(void);
int validate_env_var_name(const char *name);
int is_safe_env_var(const char *name);

/* Command information helpers */
int handle_which_command(const char *command);
int handle_type_command(const char *command);
char *find_command_type(const char *command);

/* Ansible detection functions */
struct ansible_detection_info *detect_ansible_session(void);
void free_ansible_detection_info(struct ansible_detection_info *info);
int is_ansible_environment_variable(const char *var_name);
int check_ansible_environment_variables(struct ansible_detection_info *info);
int check_ansible_become_method(struct ansible_detection_info *info);
int check_ansible_parent_process(struct ansible_detection_info *info);
int check_ansible_execution_context(struct ansible_detection_info *info);
char *get_parent_process_name(pid_t pid);
pid_t get_parent_process_id(void);
int walk_process_tree_for_ansible(pid_t start_pid, int max_depth);
void log_ansible_detection(const struct ansible_detection_info *info);

/* Main program functions */
int main_loop(void);
void cleanup_and_exit(int exit_code);

/* Terminal state management */
void save_terminal_state(void);
void restore_terminal_state(void);

#endif /* SUDOSH_H */
