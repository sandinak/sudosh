#include "src/sudosh.h"

int test_credential_stuffing() {
    /* Test if sudosh properly handles authentication attempts with invalid users */
    const char *invalid_users[] = {
        "nonexistent_user_12345",
        "fake_admin_user",
        "test_invalid_user",
        "",  /* Empty username */
        NULL
    };
    
    for (int i = 0; invalid_users[i]; i++) {
        printf("Testing invalid user: '%s'\n", invalid_users[i]);
        /* Test if authentication fails for invalid users */
        if (authenticate_user(invalid_users[i])) {
            printf("  VULNERABLE: Authentication succeeded!\n");
            /* Authentication succeeded for invalid user - vulnerable */
            return 1;
        } else {
            printf("  SECURE: Authentication failed\n");
        }
    }

    /* Test if authentication properly rejects users with invalid characters */
    const char *malicious_users[] = {
        "user;rm -rf /",
        "user`whoami`",
        "user$(id)",
        "user/../root",
        NULL
    };
    
    for (int i = 0; malicious_users[i]; i++) {
        printf("Testing malicious user: '%s'\n", malicious_users[i]);
        if (authenticate_user(malicious_users[i])) {
            printf("  VULNERABLE: Authentication succeeded!\n");
            /* Authentication succeeded for malicious username - vulnerable */
            return 1;
        } else {
            printf("  SECURE: Authentication failed\n");
        }
    }
    
    return 0; /* Secure - all invalid authentications were rejected */
}

int main() {
    printf("=== Credential Stuffing Test Debug ===\n");
    int result = test_credential_stuffing();
    printf("\nTest result: %s\n", result ? "VULNERABLE" : "SECURE");
    return result;
}
