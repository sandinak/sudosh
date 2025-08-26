#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <dirent.h>
#include <limits.h>

/**
 * Test tab completion functionality
 * This test validates the fix for proper file path expansion
 */

// Simplified completion functions for testing
char *find_completion_start_test(const char *buffer, int pos);
int is_command_position_test(const char *buffer, int pos);
char **complete_path_test(const char *text, int is_command_context);

void test_file_completion() {
    printf("Testing file completion...\n");
    
    // Test completing /etc/pas should include /etc/passwd
    char **matches = complete_path("/etc/pas", 0, 8, 0);  // Not in command context
    
    int found_passwd = 0;
    if (matches) {
        for (int i = 0; matches[i]; i++) {
            printf("  Match %d: %s\n", i, matches[i]);
            if (strstr(matches[i], "passwd")) {
                found_passwd = 1;
            }
        }
        
        // Free matches
        for (int i = 0; matches[i]; i++) {
            free(matches[i]);
        }
        free(matches);
    }
    
    if (found_passwd) {
        printf("  ✅ PASS: Found passwd in completion results\n");
    } else {
        printf("  ❌ FAIL: passwd not found in completion results\n");
    }
    printf("\n");
}

void test_command_completion() {
    printf("Testing command completion...\n");
    
    // Test completing /bin/l should include executables only
    char **matches = complete_path("/bin/l", 0, 6, 1);  // In command context
    
    int found_ls = 0;
    if (matches) {
        for (int i = 0; matches[i]; i++) {
            printf("  Match %d: %s\n", i, matches[i]);
            if (strstr(matches[i], "ls")) {
                found_ls = 1;
            }
        }
        
        // Free matches
        for (int i = 0; matches[i]; i++) {
            free(matches[i]);
        }
        free(matches);
    }
    
    if (found_ls) {
        printf("  ✅ PASS: Found ls in command completion results\n");
    } else {
        printf("  ❌ FAIL: ls not found in command completion results\n");
    }
    printf("\n");
}

void test_prefix_extraction() {
    printf("Testing prefix extraction...\n");
    
    // Test find_completion_start function
    char *prefix1 = find_completion_start("vi /etc/pas", 11);
    if (prefix1 && strcmp(prefix1, "/etc/pas") == 0) {
        printf("  ✅ PASS: Correctly extracted prefix '/etc/pas'\n");
    } else {
        printf("  ❌ FAIL: Expected '/etc/pas', got '%s'\n", prefix1 ? prefix1 : "NULL");
    }
    if (prefix1) free(prefix1);
    
    char *prefix2 = find_completion_start("ls /etc/p", 9);
    if (prefix2 && strcmp(prefix2, "/etc/p") == 0) {
        printf("  ✅ PASS: Correctly extracted prefix '/etc/p'\n");
    } else {
        printf("  ❌ FAIL: Expected '/etc/p', got '%s'\n", prefix2 ? prefix2 : "NULL");
    }
    if (prefix2) free(prefix2);
    
    printf("\n");
}

void test_command_position_detection() {
    printf("Testing command position detection...\n");
    
    // Test is_command_position function
    int is_cmd1 = is_command_position("vi /etc/pas", 11);
    if (!is_cmd1) {
        printf("  ✅ PASS: Correctly identified '/etc/pas' as NOT command position\n");
    } else {
        printf("  ❌ FAIL: Incorrectly identified '/etc/pas' as command position\n");
    }
    
    int is_cmd2 = is_command_position("vi", 2);
    if (is_cmd2) {
        printf("  ✅ PASS: Correctly identified 'vi' as command position\n");
    } else {
        printf("  ❌ FAIL: Incorrectly identified 'vi' as NOT command position\n");
    }
    
    int is_cmd3 = is_command_position("/bin/ls", 7);
    if (is_cmd3) {
        printf("  ✅ PASS: Correctly identified '/bin/ls' as command position\n");
    } else {
        printf("  ❌ FAIL: Incorrectly identified '/bin/ls' as NOT command position\n");
    }
    
    printf("\n");
}

void test_specific_case() {
    printf("Testing specific case: 'vi /etc/pas<TAB>'...\n");
    
    // Simulate the exact scenario from the issue
    const char *buffer = "vi /etc/pas";
    int pos = 11;  // cursor at end of "pas"
    
    char *prefix = find_completion_start(buffer, pos);
    if (!prefix) {
        printf("  ❌ FAIL: Could not extract prefix\n");
        return;
    }
    
    printf("  Extracted prefix: '%s'\n", prefix);
    
    int is_cmd = is_command_position(buffer, pos);
    printf("  Is command position: %s\n", is_cmd ? "yes" : "no");
    
    char **matches = NULL;
    if (is_cmd && strchr(prefix, '/')) {
        matches = complete_path(prefix, 0, strlen(prefix), 1);
    } else if (!is_cmd) {
        matches = complete_path(prefix, 0, strlen(prefix), 0);
    }
    
    int found_passwd = 0;
    int found_incorrect = 0;
    
    if (matches) {
        printf("  Completion results:\n");
        for (int i = 0; matches[i]; i++) {
            printf("    %d: '%s'\n", i, matches[i]);
            if (strstr(matches[i], "passwd")) {
                found_passwd = 1;
            }
            // Check for incorrect matches like /etc/pm
            if (strstr(matches[i], "/etc/pm") && !strstr(matches[i], "passwd")) {
                found_incorrect = 1;
            }
        }
        
        // Free matches
        for (int i = 0; matches[i]; i++) {
            free(matches[i]);
        }
        free(matches);
    } else {
        printf("  No matches found\n");
    }
    
    if (found_passwd && !found_incorrect) {
        printf("  ✅ PASS: Found passwd and no incorrect matches\n");
    } else if (found_passwd && found_incorrect) {
        printf("  ⚠️  PARTIAL: Found passwd but also incorrect matches\n");
    } else if (!found_passwd && !found_incorrect) {
        printf("  ❌ FAIL: No passwd found and no matches\n");
    } else {
        printf("  ❌ FAIL: No passwd found but incorrect matches present\n");
    }
    
    free(prefix);
    printf("\n");
}

int main() {
    printf("Tab Completion Fix Test Suite\n");
    printf("=============================\n\n");
    
    test_prefix_extraction();
    test_command_position_detection();
    test_file_completion();
    test_command_completion();
    test_specific_case();
    
    printf("Test suite completed.\n");
    return 0;
}
