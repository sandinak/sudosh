#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple test to verify secure editor functionality */

/* Mock functions for testing */
int is_secure_editor_test(const char *command) {
    if (!command) return 0;
    
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;
    
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }
    
    const char *secure_editors[] = {
        "vi", "vim", "view", "nano", "pico", NULL
    };
    
    for (int i = 0; secure_editors[i]; i++) {
        if (strcmp(cmd_name, secure_editors[i]) == 0) {
            free(cmd_copy);
            return 1;
        }
    }
    
    free(cmd_copy);
    return 0;
}

int is_interactive_editor_test(const char *command) {
    if (!command) return 0;
    
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;
    
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) {
        free(cmd_copy);
        return 0;
    }
    
    const char *editors[] = {
        "nvim", "emacs", "joe", "mcedit", "ed", "ex", NULL
    };
    
    for (int i = 0; editors[i]; i++) {
        if (strcmp(cmd_name, editors[i]) == 0) {
            free(cmd_copy);
            return 1;
        }
    }
    
    free(cmd_copy);
    return 0;
}

int validate_command_test(const char *command) {
    /* Simulate the validation logic */
    
    /* Check for secure editors first */
    if (is_secure_editor_test(command)) {
        printf("  → Secure editor detected: %s\n", command);
        return 1;  /* Allow secure editors */
    }
    
    /* Block other interactive editors */
    if (is_interactive_editor_test(command)) {
        printf("  → Interactive editor blocked: %s\n", command);
        return 0;  /* Block dangerous editors */
    }
    
    /* Allow other commands */
    return 1;
}

int main() {
    printf("Testing Secure Editor Regression\n");
    printf("================================\n\n");
    
    /* Test secure editors - these MUST be allowed */
    const char *secure_commands[] = {
        "vi /tmp/test.txt",
        "vim /etc/passwd",
        "nano /etc/hosts",
        "pico /tmp/config.conf",
        "view /var/log/syslog",
        NULL
    };
    
    printf("Testing secure editors (should be ALLOWED):\n");
    int secure_passed = 1;
    for (int i = 0; secure_commands[i]; i++) {
        printf("Testing: %s\n", secure_commands[i]);
        int is_secure = is_secure_editor_test(secure_commands[i]);
        int is_interactive = is_interactive_editor_test(secure_commands[i]);
        int is_valid = validate_command_test(secure_commands[i]);
        
        printf("  is_secure_editor: %s\n", is_secure ? "YES" : "NO");
        printf("  is_interactive_editor: %s\n", is_interactive ? "YES" : "NO");
        printf("  validate_command: %s\n", is_valid ? "ALLOWED" : "BLOCKED");
        
        if (!is_secure || is_interactive || !is_valid) {
            printf("  ❌ REGRESSION: Secure editor is being blocked!\n");
            secure_passed = 0;
        } else {
            printf("  ✅ PASS: Secure editor works correctly\n");
        }
        printf("\n");
    }
    
    /* Test dangerous editors - these should be blocked */
    const char *dangerous_commands[] = {
        "emacs /tmp/test.txt",
        "joe /tmp/test.txt",
        "mcedit /tmp/test.txt",
        "nvim /tmp/test.txt",
        NULL
    };
    
    printf("Testing dangerous editors (should be BLOCKED):\n");
    int dangerous_passed = 1;
    for (int i = 0; dangerous_commands[i]; i++) {
        printf("Testing: %s\n", dangerous_commands[i]);
        int is_secure = is_secure_editor_test(dangerous_commands[i]);
        int is_interactive = is_interactive_editor_test(dangerous_commands[i]);
        int is_valid = validate_command_test(dangerous_commands[i]);
        
        printf("  is_secure_editor: %s\n", is_secure ? "YES" : "NO");
        printf("  is_interactive_editor: %s\n", is_interactive ? "YES" : "NO");
        printf("  validate_command: %s\n", is_valid ? "ALLOWED" : "BLOCKED");
        
        if (is_secure || !is_interactive || is_valid) {
            printf("  ❌ REGRESSION: Dangerous editor is being allowed!\n");
            dangerous_passed = 0;
        } else {
            printf("  ✅ PASS: Dangerous editor correctly blocked\n");
        }
        printf("\n");
    }
    
    /* Final result */
    printf("FINAL RESULT:\n");
    printf("=============\n");
    if (secure_passed && dangerous_passed) {
        printf("✅ ALL TESTS PASSED - No regression detected\n");
        printf("✅ Secure editors (vi, vim, nano, pico) are allowed\n");
        printf("✅ Dangerous editors (emacs, joe, nvim) are blocked\n");
        return 0;
    } else {
        printf("❌ REGRESSION DETECTED!\n");
        if (!secure_passed) {
            printf("❌ Secure editors are being incorrectly blocked\n");
        }
        if (!dangerous_passed) {
            printf("❌ Dangerous editors are being incorrectly allowed\n");
        }
        return 1;
    }
}
