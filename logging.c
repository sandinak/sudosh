#include "sudosh.h"

/* Global variables for logging */
static int logging_initialized = 0;

/**
 * Initialize syslog for sudosh
 */
void init_logging(void) {
    if (!logging_initialized) {
        openlog("sudosh", LOG_PID | LOG_CONS, LOG_AUTHPRIV);
        logging_initialized = 1;
    }
}

/**
 * Log command execution
 */
void log_command(const char *username, const char *command, int success) {
    char hostname[256];
    char *tty;
    char *pwd;
    
    if (!logging_initialized) {
        init_logging();
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }

    /* Get TTY */
    tty = ttyname(STDIN_FILENO);
    if (!tty) {
        tty = "unknown";
    } else {
        /* Remove /dev/ prefix if present */
        if (strncmp(tty, "/dev/", 5) == 0) {
            tty += 5;
        }
    }

    /* Get current working directory */
    pwd = getcwd(NULL, 0);
    if (!pwd) {
        pwd = strdup("unknown");
    }

    if (success) {
        syslog(LOG_COMMAND, 
               "%s : TTY=%s ; PWD=%s ; USER=root ; COMMAND=%s",
               username, tty, pwd, command);
    } else {
        syslog(LOG_ERROR,
               "%s : TTY=%s ; PWD=%s ; USER=root ; COMMAND=%s (FAILED)",
               username, tty, pwd, command);
    }

    if (pwd) {
        free(pwd);
    }
}

/**
 * Log authentication attempts
 */
void log_authentication(const char *username, int success) {
    char hostname[256];
    char *tty;
    
    if (!logging_initialized) {
        init_logging();
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }

    /* Get TTY */
    tty = ttyname(STDIN_FILENO);
    if (!tty) {
        tty = "unknown";
    } else {
        /* Remove /dev/ prefix if present */
        if (strncmp(tty, "/dev/", 5) == 0) {
            tty += 5;
        }
    }

    if (success) {
        syslog(LOG_AUTH_SUCCESS,
               "%s : TTY=%s ; authentication succeeded",
               username, tty);
    } else {
        syslog(LOG_AUTH_FAILURE,
               "%s : TTY=%s ; authentication failed",
               username, tty);
    }
}

/**
 * Log session start
 */
void log_session_start(const char *username) {
    char hostname[256];
    char *tty;
    
    if (!logging_initialized) {
        init_logging();
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }

    /* Get TTY */
    tty = ttyname(STDIN_FILENO);
    if (!tty) {
        tty = "unknown";
    } else {
        /* Remove /dev/ prefix if present */
        if (strncmp(tty, "/dev/", 5) == 0) {
            tty += 5;
        }
    }

    syslog(LOG_INFO,
           "%s : TTY=%s ; session opened for user root",
           username, tty);
}

/**
 * Log session end
 */
void log_session_end(const char *username) {
    char hostname[256];
    char *tty;
    
    if (!logging_initialized) {
        init_logging();
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }

    /* Get TTY */
    tty = ttyname(STDIN_FILENO);
    if (!tty) {
        tty = "unknown";
    } else {
        /* Remove /dev/ prefix if present */
        if (strncmp(tty, "/dev/", 5) == 0) {
            tty += 5;
        }
    }

    syslog(LOG_INFO,
           "%s : TTY=%s ; session closed for user root",
           username, tty);
}

/**
 * Log error messages
 */
void log_error(const char *message) {
    if (!logging_initialized) {
        init_logging();
    }

    syslog(LOG_ERROR, "error: %s", message);
}

/**
 * Log security violations
 */
void log_security_violation(const char *username, const char *violation) {
    char hostname[256];
    char *tty;
    
    if (!logging_initialized) {
        init_logging();
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }

    /* Get TTY */
    tty = ttyname(STDIN_FILENO);
    if (!tty) {
        tty = "unknown";
    } else {
        /* Remove /dev/ prefix if present */
        if (strncmp(tty, "/dev/", 5) == 0) {
            tty += 5;
        }
    }

    syslog(LOG_WARNING,
           "%s : TTY=%s ; SECURITY VIOLATION: %s",
           username, tty, violation);
}

/**
 * Close logging
 */
void close_logging(void) {
    if (logging_initialized) {
        closelog();
        logging_initialized = 0;
    }
}
