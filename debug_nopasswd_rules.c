#include "sudosh.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== Debugging NOPASSWD Rules ===\n");
    
    char *username = get_current_username();
    if (!username) {
        printf("❌ Could not get current username\n");
        return 1;
    }
    
    printf("🧪 Debugging NOPASSWD for user: %s\n", username);
    
    struct sudoers_config *config = parse_sudoers_file("/etc/sudoers");
    if (!config) {
        printf("❌ Failed to parse sudoers file\n");
        free(username);
        return 1;
    }
    
    printf("✅ Sudoers file parsed successfully\n");
    printf("📁 Include directory: %s\n", config->includedir ? config->includedir : "NULL");
    
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }
    
    printf("🖥️  Hostname: %s\n", hostname);
    
    // Show all rules in detail
    printf("\n📋 All parsed rules:\n");
    struct sudoers_userspec *spec = config->userspecs;
    int count = 0;
    
    while (spec) {
        count++;
        printf("\n   Rule %d:\n", count);
        
        printf("     Users: ");
        if (spec->users) {
            for (int i = 0; spec->users[i]; i++) {
                printf("'%s' ", spec->users[i]);
            }
        } else {
            printf("NULL");
        }
        printf("\n");
        
        printf("     Hosts: ");
        if (spec->hosts) {
            for (int i = 0; spec->hosts[i]; i++) {
                printf("'%s' ", spec->hosts[i]);
            }
        } else {
            printf("NULL");
        }
        printf("\n");
        
        printf("     Commands: ");
        if (spec->commands) {
            for (int i = 0; spec->commands[i]; i++) {
                printf("'%s' ", spec->commands[i]);
            }
        } else {
            printf("NULL");
        }
        printf("\n");
        
        printf("     NOPASSWD: %s\n", spec->nopasswd ? "YES" : "NO");
        printf("     Runas user: %s\n", spec->runas_user ? spec->runas_user : "NULL");
        
        // Test if this rule matches our user
        printf("     Matches user '%s': ", username);
        if (spec->users) {
            int matches = 0;
            for (int i = 0; spec->users[i]; i++) {
                if (strcmp(spec->users[i], username) == 0) {
                    matches = 1;
                    break;
                }
                // Check for group match
                if (spec->users[i][0] == '%') {
                    struct group *grp = getgrnam(spec->users[i] + 1);
                    if (grp && grp->gr_mem) {
                        for (char **member = grp->gr_mem; *member; member++) {
                            if (strcmp(*member, username) == 0) {
                                matches = 1;
                                break;
                            }
                        }
                    }
                }
            }
            printf("%s\n", matches ? "YES" : "NO");
        } else {
            printf("NO (no users defined)\n");
        }
        
        spec = spec->next;
    }
    
    printf("\n📊 Total rules: %d\n", count);
    
    // Test privilege and NOPASSWD checking
    printf("\n🔍 Testing privilege checking:\n");
    int has_privileges = check_sudoers_privileges(username, hostname, config);
    int has_nopasswd = check_sudoers_nopasswd(username, hostname, config);
    
    printf("   Privileges: %s\n", has_privileges ? "✅ YES" : "❌ NO");
    printf("   NOPASSWD: %s\n", has_nopasswd ? "✅ YES" : "❌ NO");
    
    free_sudoers_config(config);
    free(username);
    
    printf("\n✅ Debug complete!\n");
    return 0;
}
