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

#ifndef MOCK_AUTH
#include <security/pam_appl.h>
#endif

/* External environment variable */
extern char **environ;

/* Configuration constants */
#define MAX_COMMAND_LENGTH 4096
#define MAX_USERNAME_LENGTH 256
#define MAX_PASSWORD_LENGTH 256
#define SUDOSH_VERSION "1.0.0"

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
void cleanup_security(void);

/* Utility functions */
void print_banner(void);
void print_help(void);
char *trim_whitespace(char *str);
int is_empty_command(const char *command);
char *read_command(void);
int handle_builtin_command(const char *command);
char *get_current_username(void);
int is_whitespace_only(const char *str);
char *safe_strdup(const char *str);

/* Main program functions */
int main_loop(void);
void cleanup_and_exit(int exit_code);

#endif /* SUDOSH_H */
