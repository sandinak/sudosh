#include "dangerous_commands.h"
#include "sudosh.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Highly dangerous commands that should always require password in editors */
static const char *critical_dangerous_commands[] = {
    /* System modification */
    "rm", "rmdir", "unlink", "shred",
    "mv", "cp", "dd", "truncate",
    
    /* Permission changes */
    "chmod", "chown", "chgrp", "chattr",
    "setfacl", "setcap",
    
    /* System services */
    "systemctl", "service", "init",
    "shutdown", "reboot", "halt", "poweroff",
    
    /* Package management */
    "apt", "apt-get", "yum", "dnf", "rpm", "dpkg",
    "snap", "flatpak", "brew", "pip", "npm",
    
    /* Network configuration */
    "iptables", "ip6tables", "ufw", "firewall-cmd",
    "ifconfig", "ip", "route", "netstat",
    
    /* User management */
    "useradd", "userdel", "usermod", "passwd",
    "groupadd", "groupdel", "groupmod",
    "su", "sudo", "sudoedit",
    
    /* File system operations */
    "mount", "umount", "fsck", "mkfs",
    "fdisk", "parted", "lvm", "mdadm",
    
    /* Process management */
    "kill", "killall", "pkill", "killproc",
    
    /* Archive operations with potential for damage */
    "tar", "gzip", "gunzip", "zip", "unzip",
    
    NULL
};

/* Moderately dangerous commands that require password in editors but not standard shells */
static const char *moderate_dangerous_commands[] = {
    /* Text editors that can modify system files */
    "vi", "vim", "nano", "emacs", "gedit",
    "kate", "code", "atom", "sublime",
    
    /* File operations */
    "touch", "mkdir", "ln", "find",
    "rsync", "scp", "sftp",
    
    /* System information that could be sensitive */
    "ps", "top", "htop", "lsof", "netstat",
    "ss", "who", "w", "last", "lastlog",
    
    /* Log viewing */
    "tail", "head", "less", "more", "cat",
    "grep", "awk", "sed",
    
    /* Development tools */
    "make", "gcc", "g++", "python", "perl",
    "ruby", "node", "java", "javac",
    
    /* Database operations */
    "mysql", "psql", "sqlite3", "mongo",
    
    NULL
};

/* File paths that are considered sensitive */
static const char *sensitive_paths[] = {
    "/etc/", "/var/log/", "/var/run/", "/var/lib/",
    "/usr/bin/", "/usr/sbin/", "/bin/", "/sbin/",
    "/boot/", "/root/", "/home/", "/opt/",
    "/sys/", "/proc/", "/dev/",
    NULL
};

/* Command patterns that indicate dangerous operations */
static const char *dangerous_patterns[] = {
    /* Recursive operations */
    " -R", " --recursive", " -rf", " -Rf", " -fr", " -fR",
    
    /* Force operations */
    " -f", " --force", " -y", " --yes",
    
    /* System-wide operations */
    " --system", " --global", " --all",
    
    /* Privilege escalation */
    "sudo ", "su ", "runuser ",
    
    /* Redirection to sensitive files */
    "> /etc/", ">> /etc/", "> /var/", ">> /var/",
    "> /usr/", ">> /usr/", "> /boot/", ">> /boot/",
    
    NULL
};

/**
 * Check if a command is critically dangerous
 */
int is_critical_dangerous_command(const char *command) {
    if (!command) return 0;
    
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;
    
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }
    
    /* Remove path prefix if present */
    char *base_name = strrchr(cmd_name, '/');
    if (base_name) {
        cmd_name = base_name + 1;
    }
    
    /* Check against critical dangerous commands */
    for (int i = 0; critical_dangerous_commands[i]; i++) {
        if (strcmp(cmd_name, critical_dangerous_commands[i]) == 0) {
            free(cmd_copy);
            return 1;
        }
    }
    
    free(cmd_copy);
    return 0;
}

/**
 * Check if a command is moderately dangerous
 */
int is_moderate_dangerous_command(const char *command) {
    if (!command) return 0;
    
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;
    
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }
    
    /* Remove path prefix if present */
    char *base_name = strrchr(cmd_name, '/');
    if (base_name) {
        cmd_name = base_name + 1;
    }
    
    /* Check against moderate dangerous commands */
    for (int i = 0; moderate_dangerous_commands[i]; i++) {
        if (strcmp(cmd_name, moderate_dangerous_commands[i]) == 0) {
            free(cmd_copy);
            return 1;
        }
    }
    
    free(cmd_copy);
    return 0;
}

/**
 * Check if a command involves sensitive file paths
 */
int involves_sensitive_paths(const char *command) {
    if (!command) return 0;
    
    for (int i = 0; sensitive_paths[i]; i++) {
        if (strstr(command, sensitive_paths[i])) {
            return 1;
        }
    }
    
    return 0;
}

/**
 * Check if a command contains dangerous patterns
 */
int contains_dangerous_patterns(const char *command) {
    if (!command) return 0;
    
    for (int i = 0; dangerous_patterns[i]; i++) {
        if (strstr(command, dangerous_patterns[i])) {
            return 1;
        }
    }
    
    return 0;
}

/**
 * Comprehensive check if a command should require password in editor environments
 */
int requires_password_in_editor(const char *command) {
    if (!command) return 0;
    
    /* Critical commands always require password */
    if (is_critical_dangerous_command(command)) {
        return 1;
    }
    
    /* Moderate commands require password in editors */
    if (is_moderate_dangerous_command(command)) {
        return 1;
    }
    
    /* Commands involving sensitive paths require password */
    if (involves_sensitive_paths(command)) {
        return 1;
    }
    
    /* Commands with dangerous patterns require password */
    if (contains_dangerous_patterns(command)) {
        return 1;
    }
    
    return 0;
}

/**
 * Get a human-readable explanation of why a command is dangerous
 */
const char *get_danger_explanation(const char *command) {
    if (!command) return "Unknown command";
    
    if (is_critical_dangerous_command(command)) {
        return "Critical system command that can cause significant damage";
    }
    
    if (is_moderate_dangerous_command(command)) {
        return "Command that can modify system files or access sensitive information";
    }
    
    if (involves_sensitive_paths(command)) {
        return "Command involves access to sensitive system directories";
    }
    
    if (contains_dangerous_patterns(command)) {
        return "Command contains potentially dangerous flags or patterns";
    }
    
    return "Command is considered safe";
}
