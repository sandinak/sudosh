#include "sudosh.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== Testing Privilege Escalation for Sudoers Reading ===\n");
    
    char *username = get_current_username();
    if (!username) {
        printf("❌ Could not get current username\n");
        return 1;
    }
    
    printf("🧪 Testing for user: %s\n", username);
    printf("📋 Current UIDs: real=%d, effective=%d\n", getuid(), geteuid());
    
    // Test direct sudoers parsing (should work with setuid)
    printf("\n1️⃣ Testing direct sudoers parsing:\n");
    struct sudoers_config *config = parse_sudoers_file("/etc/sudoers");
    if (config) {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) != 0) {
            strcpy(hostname, "localhost");
        }
        
        int has_privileges = check_sudoers_privileges(username, hostname, config);
        int has_nopasswd = check_sudoers_nopasswd(username, hostname, config);
        
        printf("   Sudoers file parsed: ✅ SUCCESS\n");
        printf("   Include directory: %s\n", config->includedir ? config->includedir : "NULL");
        
        // Count rules
        int count = 0;
        struct sudoers_userspec *spec = config->userspecs;
        while (spec) {
            count++;
            spec = spec->next;
        }
        
        printf("   Rules found: %d\n", count);
        printf("   Privileges: %s\n", has_privileges ? "✅ YES" : "❌ NO");
        printf("   NOPASSWD: %s\n", has_nopasswd ? "✅ YES" : "❌ NO");
        
        free_sudoers_config(config);
    } else {
        printf("   ❌ Failed to parse sudoers file\n");
    }
    
    // Test enhanced privilege checking
    printf("\n2️⃣ Testing enhanced privilege checking:\n");
    int enhanced_privileges = check_sudo_privileges_enhanced(username);
    printf("   Enhanced privileges: %s\n", enhanced_privileges ? "✅ YES" : "❌ NO");
    
    // Test enhanced NOPASSWD checking
    printf("\n3️⃣ Testing enhanced NOPASSWD checking:\n");
    int enhanced_nopasswd = check_nopasswd_privileges_enhanced(username);
    printf("   Enhanced NOPASSWD: %s\n", enhanced_nopasswd ? "✅ YES" : "❌ NO");
    
    // Compare with sudo -l method
    printf("\n4️⃣ Comparing with sudo -l method:\n");
    int sudo_l_privileges = check_sudo_privileges(username);
    int sudo_l_nopasswd = check_nopasswd_sudo_l(username);
    printf("   sudo -l privileges: %s\n", sudo_l_privileges ? "✅ YES" : "❌ NO");
    printf("   sudo -l NOPASSWD: %s\n", sudo_l_nopasswd ? "✅ YES" : "❌ NO");
    
    // Summary
    printf("\n📊 Summary:\n");
    printf("   Direct parsing vs sudo -l privileges: %s\n", 
           (enhanced_privileges == sudo_l_privileges) ? "✅ MATCH" : "❌ DIFFER");
    printf("   Direct parsing vs sudo -l NOPASSWD: %s\n", 
           (enhanced_nopasswd == sudo_l_nopasswd) ? "✅ MATCH" : "❌ DIFFER");
    
    if (enhanced_privileges && !sudo_l_privileges) {
        printf("   🎉 Direct parsing found privileges that sudo -l missed!\n");
    } else if (!enhanced_privileges && sudo_l_privileges) {
        printf("   ⚠️  sudo -l found privileges that direct parsing missed\n");
    }
    
    free(username);
    
    printf("\n✅ Privilege escalation test complete!\n");
    return 0;
}
