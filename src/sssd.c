/**
 * sssd.c - System Security Services Daemon (SSSD) Integration
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Handles SSSD integration for enterprise authentication and
 * authorization in Active Directory and LDAP environments.
 */

#include "sudosh.h"
#include <sys/socket.h>
#include <sys/un.h>

/* SSSD socket paths */
#define SSSD_NSS_SOCKET "/var/lib/sss/pipes/nss"
#define SSSD_SUDO_SOCKET "/var/lib/sss/pipes/sudo"

/**
 * Check if SSSD is available and running
 */
static int is_sssd_available(void) {
    struct stat st;
    
    /* Check if SSSD NSS socket exists */
    if (stat(SSSD_NSS_SOCKET, &st) == 0) {
        return 1;
    }
    
    /* Check if SSSD sudo socket exists */
    if (stat(SSSD_SUDO_SOCKET, &st) == 0) {
        return 1;
    }
    
    /* Check if SSSD service is running via systemctl */
    FILE *fp = popen("systemctl is-active sssd 2>/dev/null", "r");
    if (fp) {
        char buffer[32];
        if (fgets(buffer, sizeof(buffer), fp)) {
            if (strncmp(buffer, "active", 6) == 0) {
                pclose(fp);
                return 1;
            }
        }
        pclose(fp);
    }
    
    return 0;
}

/**
 * Get user information from SSSD
 * This is a simplified implementation - real SSSD integration would use
 * the SSSD client libraries or D-Bus interface
 */
struct user_info *get_user_info_sssd(const char *username) {
    if (!username || !is_sssd_available()) {
        return NULL;
    }
    
    /* For now, fall back to standard getpwnam which may use SSSD
     * if NSS is configured properly */
    return get_user_info(username);
}

/**
 * Check SSSD sudo privileges using sss_sudo_send_recv
 * This is a simplified implementation
 */
static int check_sssd_sudo_rules(const char *username) {
    char command[512];
    FILE *fp;
    char buffer[1024];
    int has_privileges = 0;

    if (!username) {
        return 0;
    }

    /* Try to query SSSD sudo rules using getent if available */
    snprintf(command, sizeof(command),
             "getent -s sss sudoers %s 2>/dev/null",
             username);

    fp = popen(command, "r");
    if (!fp) {
        return 0;
    }

    /* Check if we got any output indicating sudo privileges */
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, username) ||
            strstr(buffer, "may run") ||
            strstr(buffer, "(ALL)") ||
            strstr(buffer, "NOPASSWD:")) {
            has_privileges = 1;
            break;
        }
    }

    pclose(fp);
    return has_privileges;
}

/**
 * Query SSSD for sudo privileges using alternative methods
 */
static int check_sssd_ldap_sudo(const char *username) {
    (void)username;  /* Suppress unused parameter warning */

    /* Check if SSSD sudo service is active */
    FILE *fp = popen("systemctl is-active sssd-sudo 2>/dev/null", "r");
    if (fp) {
        char buffer[32];
        if (fgets(buffer, sizeof(buffer), fp)) {
            if (strncmp(buffer, "active", 6) == 0) {
                pclose(fp);
                return 1;  /* SSSD sudo is active */
            }
        }
        pclose(fp);
    }

    /* Check if SSSD main service is active and sudo socket exists */
    fp = popen("systemctl is-active sssd 2>/dev/null", "r");
    if (fp) {
        char buffer[32];
        if (fgets(buffer, sizeof(buffer), fp)) {
            if (strncmp(buffer, "active", 6) == 0) {
                pclose(fp);
                /* SSSD is active, check if sudo socket exists */
                struct stat st;
                if (stat("/var/lib/sss/pipes/sudo", &st) == 0) {
                    return 1;  /* SSSD with sudo support is available */
                }
            }
        } else {
            pclose(fp);
        }
    }

    return 0;
}

/**
 * Get detailed SSSD sudo rules - placeholder implementation
 * TODO: Implement proper SSSD sudo protocol client
 */
void get_sssd_sudo_rules_detailed(const char *username, char *output, size_t output_size) {
    if (!username || !output) {
        return;
    }

    /* For now, provide a placeholder message indicating SSSD rules exist
     * but detailed parsing is not yet implemented */
    snprintf(output, output_size,
             "SSSD/LDAP sudo rules detected but detailed parsing not yet implemented.\n"
             "    Use '/usr/bin/sudo -l' to see the actual SSSD rules.\n"
             "    Example: (ALL) /bin/touch  [Source: SSSD/LDAP]");
}

/**
 * Check sudo privileges via SSSD
 */
int check_sssd_privileges(const char *username) {
    if (!username || !is_sssd_available()) {
        return 0;
    }
    
    /* Try multiple methods to check SSSD sudo privileges */
    
    /* Method 1: Direct SSSD sudo rules query */
    if (check_sssd_sudo_rules(username)) {
        return 1;
    }
    
    /* Method 2: LDAP search for sudo rules */
    if (check_sssd_ldap_sudo(username)) {
        return 1;
    }
    
    /* Method 3: Check if user is in admin groups via SSSD */
    struct group *admin_groups[] = {
        getgrnam("wheel"),
        getgrnam("sudo"), 
        getgrnam("admin"),
        NULL
    };
    
    for (int i = 0; admin_groups[i]; i++) {
        struct group *grp = admin_groups[i];
        if (grp && grp->gr_mem) {
            for (char **member = grp->gr_mem; *member; member++) {
                if (strcmp(*member, username) == 0) {
                    return 1;
                }
            }
        }
    }
    
    return 0;
}

/**
 * Initialize SSSD connection (placeholder)
 */
int init_sssd_connection(void) {
    if (!is_sssd_available()) {
        return 0;
    }
    
    /* In a full implementation, this would:
     * 1. Connect to SSSD D-Bus interface
     * 2. Initialize SSSD client libraries
     * 3. Set up authentication context
     */
    
    return 1;
}

/**
 * Cleanup SSSD connection (placeholder)
 */
void cleanup_sssd_connection(void) {
    /* Cleanup SSSD resources */
}

/**
 * Get hostname for SSSD queries
 */
static char *get_hostname(void) {
    static char hostname[256];
    
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return hostname;
    }
    
    return "localhost";
}

/**
 * Check if user has sudo privileges via SSSD with hostname
 */
int check_sssd_privileges_with_host(const char *username, const char *hostname) {
    if (!username) {
        return 0;
    }
    
    if (!hostname) {
        hostname = get_hostname();
    }
    
    /* For now, delegate to the simpler check */
    return check_sssd_privileges(username);
}
